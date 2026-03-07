/*
Nexus

Copyright(C) 2012 - Federico Ponchio
ISTI - Italian National Research Council - Visual Computing Lab

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License (http://www.gnu.org/licenses/gpl.txt)
for more details.
*/

#include <iostream>
#include <fstream>
#include <cstring>
#include <cassert>
#include <filesystem>
#include <mutex>
#include <stdexcept>

#include <BS_thread_pool.hpp>

#include "../common/logger.h"
#include "../common/image.h"
#include "../common/mmap_file.h"

#include "vertex_cache_optimizer.h"

#include "nexusbuilder.h"
#include "kdtree.h"
#include "meshstream.h"
#include "mesh.h"
#include "tmesh.h"
#include "../common/nexusdata.h"

#include <vcg/math/similarity2.h>
#include <vcg/space/rect_packer.h>

using namespace std;
using namespace nx;

namespace fs = std::filesystem;

static int64_t pad(int64_t s) {
	const int64_t padding = NEXUS_PADDING;
	int64_t m = (s-1) & ~(padding -1);
	return m + padding;
}



unsigned int nextPowerOf2 ( unsigned int n )
{
	unsigned count = 0;

	if (n && !(n & (n - 1)))
		return n;

	while( n != 0)
	{
		n >>= 1;
		count += 1;
	}

	return 1 << count;
}


NodeBox::NodeBox(KDTree *tree, uint32_t block) {
	for(int k = 0; k < 3; k++)
		axes[k] = tree->axes[k];
	box = tree->block_boxes[block];
}

bool NodeBox::isIn(vcg::Point3f &p) {
	return KDTree::isIn(axes, box, p);
}

vector<bool> NodeBox::markBorders(Node &node, vcg::Point3f *p, uint16_t *f) {
	vector<bool> border(node.nvert, false);
	for(int i = 0; i < node.nface; i++) {
		bool outside = false;
		for(int k = 0; k < 3; k++) {
			uint16_t index = f[i*3 + k];
			outside |= !isIn(p[index]);
		}
		if(outside)
			for(int k = 0; k < 3; k++) {
				uint16_t index = f[i*3 + k];
				border[index] = true;
			}
	}
	return border;
}

NexusBuilder::NexusBuilder(uint32_t components): chunks("cache_chunks"), scaling(0.5), useNodeTex(true), tex_quality(92) {

	Signature &signature = header.signature;
	signature.vertex.setComponent(VertexElement::COORD, Attribute(Attribute::FLOAT, 3));
	if(components & FACES)
		signature.face.setComponent(FaceElement::INDEX, Attribute(Attribute::UNSIGNED_SHORT, 3));
	if(components & NORMALS)
		signature.vertex.setComponent(VertexElement::NORM, Attribute(Attribute::SHORT, 3));
	if(components & COLORS)
		signature.vertex.setComponent(VertexElement::COLOR, Attribute(Attribute::BYTE, 4));
	if(components & TEXTURES)
		signature.vertex.setComponent(FaceElement::TEX, Attribute(Attribute::FLOAT, 2));

	header.version = 2;
	header.signature = signature;
	header.nvert = header.nface = header.n_nodes = header.n_patches = header.n_textures = 0;

	// nodeTex is a TempMappedFile, automatically created open
}

NexusBuilder::NexusBuilder(Signature &signature): chunks("cache_chunks"), scaling(0.5) {
	header.version = 2;
	header.signature = signature;
	header.nvert = header.nface = header.n_nodes = header.n_patches = header.n_textures = 0;
}

void NexusBuilder::initAtlas(std::vector<nx::Image>& textures) {
	if(textures.size()) {
		atlas.addTextures(textures);
	}
}

bool NexusBuilder::initAtlas(std::vector<LoadTexture> &textures) {
	if(textures.size()) {
		bool success = atlas.addTextures(textures);
		if(!success)
			return false;
	}
	return true;
}

void NexusBuilder::create(KDTree *tree, Stream *stream, uint top_node_size) {
	Node sink;
	sink.first_patch = 0;
	nodes.push_back(sink);

	int level = 0;
	int last_top_level_size = 0;
	do {
		tree->clear();
		if(level % 2) tree->setAxesDiagonal();
		else tree->setAxesOrthogonal();

		tree->load(stream);
		stream->clear();

		createLevel(tree, stream, level);
		level++;
		if(skipSimplifyLevels <= 0 && last_top_level_size != 0 && stream->size()/(float)last_top_level_size > 0.9f) {
			break;
		}
		last_top_level_size = (int)stream->size();
		skipSimplifyLevels--;
	} while(stream->size() > (uint64_t)top_node_size);

	reverseDag();
	saturate();
}

class UnionFind {
public:
	std::vector<int> parents;
	void init(int size) {
		parents.resize(size);
		for(int i = 0; i < size; i++)
			parents[i] = i;
	}

	int root(int p) {
		while(p != parents[p])
			p = parents[p] = parents[parents[p]];
		return p;
	}
	void link(int p0, int p1) {
		int r0 = root(p0);
		int r1 = root(p1);
		parents[r1] = r0;
	}
	int compact(std::vector<int> &node_component) {
		node_component.resize(parents.size());
		std::map<int, int> remap;
		for(size_t i = 0; i < parents.size(); i++) {
			int root = (int)i;
			while(root != parents[root])
				root = parents[root];
			parents[i] = root;
			node_component[i] = remap.emplace(root, (int)remap.size()).first->second;
		}
		return (int)remap.size();
	}

};

nx::Image NexusBuilder::extractNodeTex(TMesh &mesh, int level, float &error, float &pixelXedge) {
	std::vector<vcg::Box2f> boxes;
	std::vector<int> box_texture;
	std::vector<int> vertex_to_tex(mesh.vert.size(), -1);
	std::vector<int> vertex_to_box;


	UnionFind components;
	components.init((int)mesh.vert.size());

	for(auto &face: mesh.face) {
		int v[3];
		for(int i = 0; i < 3; i++) {
			v[i] = (int)(face.V(i) - &*mesh.vert.begin());

			int &t = vertex_to_tex[v[i]];

			if(t != -1 && t != face.tex) LOGW << "Missing vertex replication across seams";
			t = face.tex;
		}
		components.link(v[0], v[1]);
		components.link(v[0], v[2]);
		assert(components.root(v[1]) == components.root(v[2]));
	}
	int n_boxes = components.compact(vertex_to_box);

	for(auto &face: mesh.face) {
		for(int i = 0; i < 3; i++) {
			int v = (int)(face.V(i) - &*mesh.vert.begin());
			vertex_to_tex[v] = face.tex;
		}
	}

	boxes.resize(n_boxes);
	box_texture.resize(n_boxes, -1);
	for(size_t i = 0; i < mesh.vert.size(); i++) {
		int b = vertex_to_box[i];
		int tex = vertex_to_tex[i];
		if(tex < 0) continue;

		vcg::Box2f &box = boxes[b];
		box_texture[b] = tex;
		auto &t = mesh.vert[i].T().P();
		if(t[0] != 1.0f)
			t[0] = fmod(t[0], 1.0f);
		if(t[1] != 1.0f)
			t[1] = fmod(t[1], 1.0f);

		if(t[0] != 0.0f || t[1] != 0.0f)
			box.Add(t);
	}

	int count = 0;
	std::vector<int> remap(n_boxes, -1);
	for(int i = 0; i < n_boxes; i++) {
		if(box_texture[i] == -1)
			continue;
		boxes[count] = boxes[i];
		box_texture[count] = box_texture[i];
		remap[i] = count++;
	}
	boxes.resize(count);
	box_texture.resize(count);
	for(int &b: vertex_to_box) {
		if(b >= 0 && b < n_boxes)
			b = remap[b];
		else
			b = -1;
	}

	std::vector<vcg::Point2i> sizes(boxes.size());
	std::vector<vcg::Point2i> origins(boxes.size());
	for(size_t b = 0; b < boxes.size(); b++) {
		auto &box = boxes[b];
		if(box.DimX() > 0.9f) {
			for(auto &face: mesh.face) {
				for(int i = 0; i < 3; i++) {
					(void)face.V(i);
					int j = (i+1)%3;
					(void)j;
				}
			}
		}
		int tex = box_texture[b];
		if(tex < 0) continue;

		float w = (float)atlas.width(tex, level);
		float h = (float)atlas.height(tex, level);
		if(w <= 0 || h <= 0) continue;
		float px = 1/(float)w;
		float py = 1/(float)h;
		box.Offset(vcg::Point2f(px, py));
		vcg::Point2i &size = sizes[b];
		vcg::Point2i &origin = origins[b];
		origin[0] = (int)std::max(0.0f, floor(box.min[0]/px));
		origin[1] = (int)std::max(0.0f, floor(box.min[1]/py));
		if(origin[0] >= (int)w) origin[0] = (int)w-1;
		if(origin[1] >= (int)h) origin[1] = (int)h-1;

		assert(origin[0] >= 0);

		size[0] = (int)(std::min(w, ceil(box.max[0]/px))) - origin[0];
		size[1] = (int)(std::min(h, ceil(box.max[1]/py))) - origin[1];
		if(size[0] <= 0) size[0] = 1;
		if(size[1] <= 0) size[1] = 1;
	}

	std::vector<vcg::Point2i> mapping;
	vcg::Point2i maxSize(1096, 1096);
	vcg::Point2i finalSize;
	bool success = false;
	for(int i = 0; i < 5; i++, maxSize[0]*= 2, maxSize[1]*= 2) {
		if(sizes.size() == 0) {
			finalSize = vcg::Point2i(1, 1);
			success = true;
			break;
		}
		bool too_large = false;
		for(auto s: sizes) {
			if(s[0] >= maxSize[0] || s[1] >= maxSize[1])
				too_large = true;
		}
		if(too_large) {
			continue;
		}
		mapping.clear();
		success = vcg::RectPacker<float>::PackInt(sizes, maxSize, mapping, finalSize);
		if(success)
			break;
	}
	if(!success) {
		cerr << "Failed packing: the texture in a single nexus node would be > 16K\n";
		cerr << "Try to reduce the size of the nodes using -t (default is 4096)";
		nx::Image image(finalSize[0], finalSize[1]);
		return image;
	}

	if (createPowTwoTex) {
		finalSize[ 0 ] = (int) nextPowerOf2( finalSize[ 0 ] );
		finalSize[ 1 ] = (int) nextPowerOf2( finalSize[ 1 ] );
	}

	nx::Image image(finalSize[0], finalSize[1]);
	image.fill(127, 127, 127, 255);

	float pdx = 1/(float)image.width();
	float pdy = 1/(float)image.height();

	for(size_t i = 0; i < mesh.vert.size(); i++) {
		auto &p = mesh.vert[i];
		auto &uv = p.T().P();
		int b = vertex_to_box[i];
		if(b == -1 || b >= (int)origins.size() || b >= (int)mapping.size() || b >= (int)box_texture.size()) {
			uv = vcg::Point2f(0.0f, 0.0f);
			continue;
		}
		vcg::Point2i &o = origins[b];
		vcg::Point2i m = mapping[b];

		int tex = box_texture[b];
		if(tex < 0) {
			uv = vcg::Point2f(0.0f, 0.0f);
			continue;
		}
		float w = (float)atlas.width(tex, level);
		float h = (float)atlas.height(tex, level);
		if(w <= 0 || h <= 0) {
			uv = vcg::Point2f(0.0f, 0.0f);
			continue;
		}
		float px = 1/(float)w;
		float py = 1/(float)h;

		if(uv[0] < 0.0f)
			uv[0] = 0.0f;
		if(uv[1] < 0.0f)
			uv[1] = 0.0f;

		float dx = uv[0]/px - o[0];
		float dy = uv[1]/py - o[1];
		if(dx < 0.0f) dx = 0.0f;
		if(dy < 0.0f) dy = 0.0f;

		uv[0] = (m[0] + dx)*pdx;
		uv[1] = (m[1] + dy)*pdy;

		assert(!isnan(uv[0]));
		assert(!isnan(uv[1]));
	}
	//compute error:
	float pdx2 = pdx*pdx;
	error = 0.0;
	pixelXedge = 0.0f;
	for(auto &face: mesh.face) {
		for(int k = 0; k < 3; k++) {
			int j = (k==2)?0:k+1;

			float edge = vcg::SquaredNorm(face.P(k) - face.P(j));
			float pixel = vcg::SquaredNorm(face.V(k)->T().P() - face.V(j)->T().P())/pdx2;
			pixelXedge += pixel;
			if(pixel > 10) pixel = 10;
			if(pixel < 1)
				error += edge;
			else
				error += edge/pixel;
		}
	}
	pixelXedge = sqrt(pixelXedge/mesh.face.size()*3);
	error = sqrt(error/mesh.face.size()*3);

	double areausage = 0.0;
	for(int i = 0; i < (int)mesh.face.size(); i++) {
		auto &face = mesh.face[i];
		int b = vertex_to_box[face.V(0) - &(mesh.vert[0])];
		if(b < 0 || b >= (int)origins.size()) continue;
		auto V0 = face.V(0)->T().P();
		auto V1 = face.V(1)->T().P();
		auto V2 = face.V(2)->T().P();
		areausage += (V2 - V0)^(V2 - V1)/2;
	}
	(void)areausage;

	{
		for(int i = 0; i < (int)boxes.size(); i++) {
			int source = box_texture[i];
			if(source < 0) continue;
			vcg::Point2i &o = origins[i];
			vcg::Point2i &s = sizes[i];

			nx::Image rect;
			{
				std::lock_guard<std::mutex> locker(m_atlas);
				rect = atlas.read(source, level, Rect(o[0], o[1], s[0], s[1]));
			}
			if(i < (int)mapping.size())
				image.blit(mapping[i][0], mapping[i][1], rect);
		}
	}

	image = image.mirrored();
	if (image.width() > max_node_tex_size || image.height() > max_node_tex_size){
		int maxDim = std::max<int>(image.width(), image.height());
		float ratio = (float)max_node_tex_size / (float)maxDim;
		image = image.scaled((int)(image.width() * ratio), (int)(image.height() * ratio));
	}
	return image;
}

void NexusBuilder::createCloudLevel(KDTreeCloud *input, StreamCloud *output, int level) {

	for(uint block = 0; block < input->nBlocks(); block++) {
		Cloud cloud = input->get(block);
		assert(cloud.size() < (1<<16));
		if(cloud.size() == 0) continue;

		Mesh mesh;
		mesh.load(cloud);

		int target_points = (int)(cloud.size()*scaling);
		std::vector<AVertex> deleted = mesh.simplifyCloud(target_points);

		uint32_t mesh_size = mesh.serializedSize(header.signature);
		mesh_size = (uint32_t)pad(mesh_size);
		uint32_t chunk = chunks.addChunk(mesh_size);
		unsigned char *buffer = chunks.getChunk(chunk);
		uint32_t patch_offset = (uint32_t)patches.size();

		std::vector<Patch> node_patches;
		mesh.serialize(buffer, header.signature, node_patches);

		std::reverse(node_patches.begin(), node_patches.end());
		patches.insert(patches.end(), node_patches.begin(), node_patches.end());

		uint32_t current_node = (uint32_t)nodes.size();
		nx::Node node = mesh.getNode();
		node.offset = chunk;
		node.error = mesh.averageDistance();
		node.first_patch = patch_offset;
		nodes.push_back(node);
		boxes.push_back(NodeBox(input, block));

		swap(mesh.vert, deleted);
		mesh.vn = (int)mesh.vert.size();

		Splat *vertices = new Splat[mesh.vn];
		mesh.getVertices(vertices, current_node);

		for(int i = 0; i < mesh.vn; i++) {
			Splat &s = vertices[i];
			output->pushVertex(s);
		}

		delete []vertices;
	}
}


void NexusBuilder::processBlock(KDTreeSoup *input, StreamSoup *output, uint block, int level) {
	TMesh mesh;
	TMesh tmp;

	Mesh mesh1;
	uint32_t mesh_size;


	int ntriangles = 0;
	{
		std::lock_guard<std::mutex> locker(m_input);
		Soup soup = input->get(block);
		assert(soup.size() < (1<<16));
		if(soup.size() == 0) return;

		ntriangles = (int)soup.size();
		if(!hasTextures()) {
			mesh1.load(soup);
		} else {
			mesh.load(soup);
		}
	}



	if(!hasTextures()) {
		input->lock(mesh1, block);
		mesh_size = mesh1.serializedSize(header.signature);

	} else {
		input->lock(mesh, block);

		vcg::tri::Append<TMesh,TMesh>::MeshCopy(tmp,mesh);
		for(int i = 0; i < (int)tmp.face.size(); i++) {
			tmp.face[i].node = mesh.face[i].node;
			tmp.face[i].tex = mesh.face[i].tex;
		}
		tmp.splitSeams(header.signature);
		if(tmp.vert.size() > 60000) {
			cerr << "Unable to properly simplify due to fragmented parametrization\n"
				 << "Try to reduce the size of the nodes using -f (default is 32768)" << endl;
			return;
		}

		mesh_size = tmp.serializedSize(header.signature);
	}
	mesh_size = (uint32_t)pad(mesh_size);
	unsigned char *buffer = new unsigned char[mesh_size];

	std::vector<Patch> node_patches;

	float error;
	float pixelXedge;
	if(!hasTextures()) {
		mesh1.serialize(buffer, header.signature, node_patches);

	} else {

		if(useNodeTex) {
			nx::Image nodetex = extractNodeTex(tmp, level, error, pixelXedge);
			tmp.serialize(buffer, header.signature, node_patches);

			Texture t;

			{
				std::lock_guard<std::mutex> locker(m_textures);
				t.offset = (uint32_t)(nodeTex.size()/NEXUS_PADDING);

				output_pixels += nodetex.width()*nodetex.height();

				// Write JPEG to nodeTex temp file
				std::vector<unsigned char> jpegBuf;
				nodetex.saveToMemory(jpegBuf, tex_quality);
				nodeTex.write(reinterpret_cast<const char*>(jpegBuf.data()), jpegBuf.size());

				uint64_t size = pad(nodeTex.size());
				nodeTex.resize(size);
				nodeTex.seek(size);
			}
			{
				std::lock_guard<std::mutex> locker(m_builder);
				textures.push_back(t);
				for(Patch &patch: node_patches)
					patch.texture = (uint32_t)textures.size()-1;
			}
		}
	}
	uint32_t chunk;
	{
		std::lock_guard<std::mutex> locker(m_chunks);
		chunk = chunks.addChunk(mesh_size);
		unsigned char *chunk_buffer = chunks.getChunk(chunk);
		memcpy(chunk_buffer, buffer, mesh_size);
		chunks.dropChunk(chunk);
	}
	delete []buffer;



	nx::Node node;
	if(!hasTextures())
		node = mesh1.getNode();
	else
		node = tmp.getNode();


	int nface;
	{

		if(!hasTextures()) {
			mesh1.lockVertices();
			{
				std::lock_guard<std::mutex> locker(m_texsimply);
				mesh1.quadricInit();
			}

			error = mesh1.simplify((uint16_t)(ntriangles*scaling), Mesh::QUADRICS);
			nface = mesh1.fn;

		} else {
			std::lock_guard<std::mutex> locker(m_texsimply);
			int nvert = (int)(ntriangles*scaling);

			if(skipSimplifyLevels > 0)
				nvert = ntriangles;

			else if(nvert < 64)
				nvert = 64;

			float e = mesh.simplify(nvert, TMesh::QUADRICS);
			if(!useNodeTex)
				error = e;
			nface = mesh.fn;
		}
	}


	uint32_t current_node;
	{
		std::lock_guard<std::mutex> locker(m_builder);

		uint32_t patch_offset = (uint32_t)patches.size();
		std::reverse(node_patches.begin(), node_patches.end());
		patches.insert(patches.end(), node_patches.begin(), node_patches.end());

		current_node = (uint32_t)nodes.size();
		node.offset = chunk;
		node.error = error;


		node.first_patch = patch_offset;

		nodes.push_back(node);
		boxes.push_back(NodeBox(input, block));
	}


	Triangle *triangles = new Triangle[nface];
	if(!hasTextures()) {
		mesh1.getTriangles(triangles, current_node);
	} else {
		mesh.getTriangles(triangles, current_node);
	}

	{
		std::lock_guard<std::mutex> locker(m_output);
		for(int i = 0; i < nface; i++) {
			Triangle &t = triangles[i];
			if(!t.isDegenerate())
				output->pushTriangle(triangles[i]);
		}
	}
	delete []triangles;
}

void NexusBuilder::createMeshLevel(KDTreeSoup *input, StreamSoup *output, int level) {
	atlas.buildLevel(level);
	if(level > 0)
		atlas.flush(level-1);

	BS::thread_pool pool(n_threads);

	for(uint block = 0; block < input->nBlocks(); block++) {
		pool.detach_task([this, input, output, block, level]() {
			this->processBlock(input, output, block, level);
		});
	}
	pool.wait();
}


void NexusBuilder::createLevel(KDTree *in, Stream *out, int level) {
	KDTreeSoup *isSoup = dynamic_cast<KDTreeSoup *>(in);
	if(!isSoup) {
		KDTreeCloud *input = dynamic_cast<KDTreeCloud *>(in);
		StreamCloud *output = dynamic_cast<StreamCloud *>(out);
		createCloudLevel(input, output, level);
	} else {
		KDTreeSoup *input = dynamic_cast<KDTreeSoup *>(in);
		StreamSoup *output = dynamic_cast<StreamSoup *>(out);
		createMeshLevel(input, output, level);
	}
}

void NexusBuilder::saturate() {
	for(int node = (int)nodes.size()-2; node >= 0; node--)
		saturateNode(node);

	nodes.back().error = 0;
}

void NexusBuilder::testSaturation() {

	for(uint n = 0; n < nodes.size()-1; n++) {
		Node &node = nodes[n];
		vcg::Sphere3f &sphere = node.sphere;
		for(uint p = node.first_patch; p < node.last_patch(); p++) {
			Patch &patch = patches[p];
			Node &child = nodes[patch.node];
			vcg::Sphere3f s = child.sphere;
			float dist = (sphere.Center() - s.Center()).Norm();
			float R = sphere.Radius();
			float r = s.Radius();
			(void)dist; (void)R; (void)r;
			assert(sphere.IsIn(child.sphere));
			assert(child.error < node.error);
		}
	}
}

void NexusBuilder::reverseDag() {

	std::reverse(nodes.begin(), nodes.end());
	std::reverse(boxes.begin(), boxes.end());
	std::reverse(patches.begin(), patches.end());

	for(uint i = 0; i < nodes.size(); i++)
		nodes[i].first_patch = (uint32_t)(patches.size() -1 - nodes[i].first_patch);

	for(uint i = (uint)nodes.size()-1; i >= 1; i--)
		nodes[i].first_patch = nodes[i-1].first_patch +1;
	nodes[0].first_patch  = 0;

	for(uint i = 0; i < patches.size(); i++) {
		patches[i].node = (uint32_t)(nodes.size() - 1 - patches[i].node);
	}
}


void NexusBuilder::save(std::string filename) {

	file.setFileName(filename);
	if(!file.open(nx::MappedFile::ReadWrite | nx::MappedFile::Truncate))
		throw std::runtime_error("could not open file " + filename);

	if(header.signature.vertex.hasNormals() && header.signature.face.hasIndex())
		uniformNormals();

	if(textures.size())
		textures.push_back(Texture());

	header.nface = 0;
	header.nvert = 0;
	header.n_nodes = (uint32_t)nodes.size();
	header.n_patches = (uint32_t)patches.size();

	header.n_textures = (uint32_t)textures.size();
	header.version = 2;

	uint32_t nroots = header.n_nodes;
	for(uint32_t j = 0; j < nroots; j++) {
		for(uint32_t i = nodes[j].first_patch; i < nodes[j].last_patch(); i++)
			if(patches[i].node < nroots)
				nroots = patches[i].node;
		nodes[j].error = nodes[j].tight_radius;
	}

	header.sphere = vcg::Sphere3f();
	for(uint32_t i = 0; i < nroots; i++)
		header.sphere.Add(nodes[i].tightSphere());

	for(uint i = 0; i < nodes.size()-1; i++) {
		nx::Node &node = nodes[i];
		header.nface += node.nface;
		header.nvert += node.nvert;
	}

	uint64_t size = sizeof(Header)  +
			nodes.size()*sizeof(Node) +
			patches.size()*sizeof(Patch) +
			textures.size()*sizeof(Texture);
	size = pad(size);
	uint64_t index_size = size;

	std::vector<uint32_t> node_chunk;
	for(uint32_t i = 0; i < (uint32_t)nodes.size()-1; i++)
		node_chunk.push_back(nodes[i].offset);

	for(uint i = 0; i < nodes.size()-1; i++) {
		nodes[i].offset = (uint32_t)(size/NEXUS_PADDING);
		uint32_t chunk = node_chunk[i];
		size += chunks.chunkSize(chunk);
	}
	nodes.back().offset = (uint32_t)(size/NEXUS_PADDING);

	if(textures.size()) {
		if(!useNodeTex) {
			for(uint i = 0; i < textures.size()-1; i++) {
				uint32_t s = textures[i].offset;
				textures[i].offset = (uint32_t)(size/NEXUS_PADDING);
				size += s;
				size = pad(size);
			}
			textures.back().offset = (uint32_t)(size/NEXUS_PADDING);

		} else {
			if(header.signature.flags & Signature::Flags::DEEPZOOM) {
				textures.back().offset = (uint32_t)(nodeTex.size()/NEXUS_PADDING);
			} else {
				for(uint i = 0; i < textures.size()-1; i++)
					textures[i].offset += (uint32_t)(size/NEXUS_PADDING);
				size += nodeTex.size();
				textures.back().offset = (uint32_t)(size/NEXUS_PADDING);
			}
		}
	}

	file.write((char*)&header, sizeof(Header));
	assert(nodes.size());
	file.write((char*)&(nodes[0]), sizeof(Node)*nodes.size());
	if(patches.size())
		file.write((char*)&(patches[0]), sizeof(Patch)*patches.size());
	if(textures.size())
		file.write((char*)&(textures[0]), sizeof(Texture)*textures.size());
	file.seek(index_size);

	//NODES
	std::string basename = filename.substr(0, filename.size() - 4) + "_files";
	if(header.signature.flags & Signature::Flags::DEEPZOOM) {
		fs::create_directories(basename);
	}
	for(uint i = 0; i < node_chunk.size(); i++) {
		uint32_t chunk = node_chunk[i];
		unsigned char *buffer = chunks.getChunk(chunk);
		optimizeNode(i, buffer);
		if(header.signature.flags & Signature::Flags::DEEPZOOM) {
			std::string nodepath = basename + "/" + std::to_string(i) + ".nxn";
			std::ofstream nodefile(nodepath, std::ios::binary);
			nodefile.write((char*)buffer, chunks.chunkSize(chunk));
		} else
			file.write((char*)buffer, chunks.chunkSize(chunk));
	}

	//TEXTURES
	if(textures.size()) {
		if(useNodeTex) {
			if(header.signature.flags & Signature::Flags::DEEPZOOM) {
				for(uint i = 0; i < textures.size()-1; i++) {
					uint32_t s = textures[i].offset*(uint32_t)NEXUS_PADDING;
					uint32_t sz = textures[i+1].offset*(uint32_t)NEXUS_PADDING - s;

					std::vector<char> buf(sz);
					nodeTex.seek(s);
					nodeTex.read(buf.data(), sz);

					std::string texpath = basename + "/" + std::to_string(i) + ".jpg";
					std::ofstream texfile(texpath, std::ios::binary);
					texfile.write(buf.data(), sz);
				}
			} else {
				nodeTex.seek(0);

				int64_t buffer_size = 64*1024*1024; //64 MB
				std::vector<char> buf(buffer_size);
				while(true) {
					int64_t nread = nodeTex.read(buf.data(), buffer_size);
					if(nread <= 0) break;
					file.write(buf.data(), nread);
				}
			}
		} else {
			for(int i = 0; i < (int)textures.size()-1; i++) {
				std::ifstream imgfile(images[i], std::ios::binary);
				if(!imgfile.is_open())
					throw std::runtime_error("could not load img " + images[i]);
				std::vector<char> contents((std::istreambuf_iterator<char>(imgfile)),
										  std::istreambuf_iterator<char>());
				file.write(contents.data(), contents.size());
				uint64_t s = file.pos();
				s = pad(s);
				file.resize(s);
				file.seek(s);
			}
		}
	}

	file.close();
}


void NexusBuilder::saturateNode(uint32_t n) {
	const float epsilon = 1.01f;

	nx::Node &node = nodes[n];
	for(uint32_t i = node.first_patch; i < node.last_patch(); i++) {
		nx::Patch &patch = patches[i];
		if(patch.node == nodes.size()-1)
			return;

		nx::Node &child = nodes[patch.node];
		if(node.error <= child.error)
			node.error = child.error*epsilon;

		if(!node.sphere.IsIn(child.sphere)) {
			float dist = (child.sphere.Center() - node.sphere.Center()).Norm();
			dist += child.sphere.Radius();
			if(dist > node.sphere.Radius())
				node.sphere.Radius() = dist;
		}
	}
	node.sphere.Radius() *= epsilon;
}

void NexusBuilder::optimizeNode(uint32_t n, unsigned char *chunk) {
	return;
	Node &node = nodes[n];
	assert(node.nface);

	uint16_t *faces = (uint16_t  *)(chunk + node.nvert*header.signature.vertex.size());

	uint start =  0;
	for(uint i = node.first_patch; i < node.last_patch(); i++) {
		Patch &patch = patches[i];
		uint end = patch.triangle_offset;
		uint nface = end - start;
		uint16_t *triangles = new uint16_t[nface*3];

		bool success = vmath::vertex_cache_optimizer::optimize_post_tnl(24, faces + 3*start, nface, node.nvert,  triangles);
		if(success)
			memcpy(faces + start, triangles, 3*sizeof(uint16_t)*nface);
		else
			cout << "Failed cache optimization" << endl;
		delete []triangles;
		start = end;
	}
}

void NexusBuilder::appendBorderVertices(uint32_t origin, uint32_t destination, std::vector<NVertex> &vertices) {
	Node &node = nodes[origin];
	uint32_t chunk = node.offset;

	unsigned char *buffer = chunks.getChunk(chunk, origin != destination);

	vcg::Point3f *point = (vcg::Point3f *)buffer;
	int sz = sizeof(vcg::Point3f) + header.signature.vertex.hasTextures()*sizeof(vcg::Point2f);
	vcg::Point3s *normal = (vcg::Point3s *)(buffer + sz * node.nvert);
	uint16_t *face = (uint16_t *)(buffer + header.signature.vertex.size()*node.nvert);

	NodeBox &nodebox = boxes[origin];

	vector<bool> border = nodebox.markBorders(node, point, face);
	for(int i = 0; i < node.nvert; i++) {
		if(border[i])
			vertices.push_back(NVertex(origin, i, point[i], normal + i));
	}
}


void NexusBuilder::uniformNormals() {

	std::vector<NVertex> vertices;

	uint32_t sink = (uint32_t)nodes.size()-1;
	for(int t = sink-1; t > 0; t--) {
		Node &target = nodes[t];

		vcg::Box3f box = boxes[t].box;
		box.Offset(box.Diag()/10);

		vertices.clear();
		appendBorderVertices(t, t, vertices);

		bool last_level = (patches[target.first_patch].node == sink);

		if(last_level) {

			for(int n = t-1; n >= 0; n--) {
				Node &node = nodes[n];
				if(patches[node.first_patch].node != sink) continue;
				if(!box.Collide(boxes[n].box)) continue;

				appendBorderVertices(n, t, vertices);
			}

		} else {

			for(uint p = target.first_patch; p < target.last_patch(); p++) {
				uint n = patches[p].node;

				appendBorderVertices(n, t, vertices);
			}
		}

		if(!vertices.size()) {
			continue;
		}

		sort(vertices.begin(), vertices.end());

		uint start = 0;
		while(start < vertices.size()) {
			NVertex &v = vertices[start];

			uint last = start+1;
			while(last < vertices.size() && vertices[last].point == v.point)
				last++;

			if(last_level && last - start > 1) {
				vcg::Point3f normalf(0, 0, 0);
				for(uint k = start; k < last; k++) {
					for(int l = 0; l < 3; l++)
						normalf[l] += (*vertices[k].normal)[l];
				}
				normalf.Normalize();
				vcg::Point3s normals;
				for(int l = 0; l < 3; l++)
					normals[l] = (short)(normalf[l]*32766);

				for(uint k = start; k < last; k++)
					*vertices[k].normal = normals;

			} else
				for(uint k = start; k < last; k++)
					*vertices[k].normal =*v.normal;

			start = last;
		}
	}
}
