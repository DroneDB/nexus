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
#include <cmath>

#include "meshstream.h"
#include "meshloader.h"
#include "plyloader.h"
#include "tsploader.h"
#include "objloader.h"
#include "stlloader.h"
#include "vcgloadermesh.h"
#include "vcgloader.h"
#include "tsloader.h"
#include "../common/pathutil.h"


#include <iostream>

using namespace std;

Stream::Stream(const std::string &cachePrefix):
	has_colors(false),
	has_normals(false),
	has_textures(false),
	cache_prefix(cachePrefix),
	vertex_quantization(0),
	current_triangle(0),
	current_block(0) {
}

void Stream::setVertexQuantization(double q) {
	vertex_quantization = q;
}

std::unique_ptr<MeshLoader> Stream::getLoader(const std::string &file, const std::string &material) {
	std::unique_ptr<MeshLoader> loader;
	if(nx::endsWith(file, ".ply"))
		loader.reset(new PlyLoader(file, cache_prefix + "_plyvertex"));

	else if(nx::endsWith(file, ".tsp"))
		loader.reset(new TspLoader(file));

	else if(nx::endsWith(file, ".obj"))
		loader.reset(new ObjLoader(file, material));

	else if(nx::endsWith(file, ".stl"))
		loader.reset(new STLLoader(file));

	else if(nx::endsWith(file, ".ts")) {
		TsLoader *ts = new TsLoader(file);
		loader.reset(ts);
		if(colormap.size() >= 2)
			ts->useColormapFor(colormap[0], colormap[1]);
	}

	else
		loader.reset(new VcgLoader<VcgMesh>(file));
	/*        else if(file.endsWith(".off"))
		loader = new OffLoader(file, vertex_quantization, max_memory);*/

//	else
//		throw nx::Error("Input format for file " + file + " is not supported at the moment");
	return loader;
}

vcg::Box3d Stream::getBox(const std::vector<std::string> &paths) {

	vcg::Box3d box;

	uint32_t length = (1<<20); //times 52 bytes. (128Mb of data)
	std::unique_ptr<Splat[]> vertices(new Splat[length]);
	
	
	for(const std::string &file: paths) {
		std::unique_ptr<MeshLoader> loader = getLoader(file, std::string());
		loader->setMaxMemory(512*(1<<20)); //read only once... does'nt really matter.
		while(true) {
			int count = loader->getVertices(length, vertices.get());
			if(count == 0) break;
		}
		box.Add(loader->box);
	}
	return box;
}

void Stream::load(MeshLoader *loader) {
	loader->setVertexQuantization(vertex_quantization);
	loader->origin = origin;
	loader->scale = scale;
	loadMesh(loader);
	has_colors &= loader->hasColors();
	has_normals &= loader->hasNormals();
	has_textures &= loader->hasTextures();

	if(has_textures) {
		for(auto tex: loader->texture_filenames) {
			textures.push_back(tex);
		}
	}
}

void Stream::load(const std::vector<std::string> &paths, const std::string &material) {

	has_colors = true;
	has_normals = true;
	has_textures = true;
	for(const std::string &file: paths) {
		std::unique_ptr<MeshLoader> loader = getLoader(file, material);
		load(loader.get());
		//box.Add(loader->box); //this lineB AFTER the mesh is streamed
	}
	current_triangle = 0;
	flush();
}


void Stream::clear() {
	clearVirtual();
	levels.clear();
	order.clear();
	textures.clear();
	current_triangle = 0;
	current_block = 0;
	box = vcg::Box3f();
}

//bit twiddling hacks
uint64_t Stream::getLevel(int64_t v) {
	static const int MultiplyDeBruijnBitPosition[32] =  {
		0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
		31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
	};
	return MultiplyDeBruijnBitPosition[((unsigned int)((v & -v) * 0x077CB531U)) >> 27];
}

void Stream::computeOrder() {
	order.clear();
	for(int level = levels.size()-1; level >= 0; level--) {
		std::vector<uint64_t> &level_blocks = levels[level];
		for(unsigned int i = 0; i < level_blocks.size(); i++) {
			order.push_back(level_blocks[i]);
		}
	}
}



//SOUP

StreamSoup::StreamSoup(const std::string &prefix):
	Stream(prefix),
	VirtualTriangleSoup(prefix) {
}

void StreamSoup::loadMesh(MeshLoader *loader) {
	loader->setMaxMemory(maxMemory());
	loader->texOffset = textures.size();
	//get 128Mb of data
	uint32_t length = (1<<20); //times 52 bytes.
	std::unique_ptr<Triangle[]> triangles(new Triangle[length]);
	while(true) {
		int count = loader->getTriangles(length, triangles.get());
		if(count == 0) break;
		for(int i = 0; i < count; i++) {
			assert(triangles[i].node == 0);
			for(int k = 0; k < 3; k++)
				countUvVertex(triangles[i].vertices[k], triangles[i].tex);
			pushTriangle(triangles[i]);
		}
	}
}

//count distinct corners with HyperLogLog
void StreamSoup::countUvVertex(const Vertex &v, const int tex) {
	//hash `FNV-1a` over the fields TMesh::splitSeams compares
	auto h = 14695981039346656037ull;
	auto fnv = [&h](const void *data, const size_t n) {
		const auto b = static_cast<const unsigned char *>(data);
		for(size_t i = 0; i < n; i++)
			h = (h ^ b[i]) * 1099511628211ull;
	};
	fnv(v.v, sizeof(v.v));
	fnv(v.t, sizeof(v.t));
	fnv(&tex, sizeof(tex));

	//finalizer `splitmix64` as FNV mixes the high bits poorly but HyperLogLog uses them
	auto finalize = [](uint64_t x) {
		x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
		x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
		return x ^ (x >> 31);
	};
	h = finalize(h);

	uint64_t reg = h >> (64 - UvVerticesHllBits);
	uint64_t rest = h << UvVerticesHllBits;
	uint8_t rank = 1; //position of the first 1 bit
	while(rank <= 64 - UvVerticesHllBits && !(rest & (1ull << 63))) {
		rest <<= 1;
		rank++;
	}
	if(rank > uv_vertices_hll[reg])
		uv_vertices_hll[reg] = rank;
}

double StreamSoup::estimatedUvVertices() const {
	const double m = uv_vertices_hll.size();
	const double alpha = 0.7213/(1 + 1.079/m);
	double sum = 0;
	auto empty = 0;
	for(const uint8_t r: uv_vertices_hll) {
		sum += std::ldexp(1.0, -r);
		if(r == 0) empty++;
	}
	double estimate = alpha * m * m / sum;
	if(estimate <= 2.5*m && empty > 0)
		estimate = m * std::log(m/empty);
	return estimate;
}

void StreamSoup::pushTriangle(Triangle &triangle) {

	//ignore degenerate faces
	vcg::Point3f v[3];
	for(int k = 0; k < 3; k++) {
		v[k] = vcg::Point3f(triangle.vertices[k].v);
		box.Add(v[k]);
	}

	/* MOVED TO LOADER AND SIMPLIFIER */
	/*ignore degenerate faces
	if(v[0] == v[1] || v[1] == v[2] || v[2] == v[0])
		return; */

	uint64_t level = getLevel(current_triangle);
	assert(levels.size() >= level);

	uint64_t block = 0;
	if(levels.size() == level) {  //need to add a level
		levels.push_back(std::vector<uint64_t>());
		block = addBlock(level);

	} else {
		block = levels[level].back();
		if(isBlockFull(block))
			block = addBlock(level);
	}

	Soup soup = get(block);
	soup.push_back(triangle);

	current_triangle++;
}

Soup StreamSoup::streamTriangles() {
	if(current_block == 0)
		computeOrder();
	if(current_block == order.size())
		return Soup(NULL, NULL, 0);

	flush(); //save memory
	uint64_t block = order[current_block];
	current_block++;

	return get(block);
}

void StreamSoup::clearVirtual() {
	VirtualTriangleSoup::clear();
}

uint64_t StreamSoup::addBlock(uint64_t level) {
	uint64_t block = VirtualTriangleSoup::addBlock();
	levels[level].push_back(block);
	return block;
}



//Cloud

StreamCloud::StreamCloud(const std::string &prefix):
	Stream(prefix),
	VirtualVertexCloud(prefix) {
}

void StreamCloud::loadMesh(MeshLoader *loader) {
	loader->setMaxMemory(maxMemory());
	//get 128Mb of data
	uint32_t length = (1<<20); //times 52 bytes.
	std::unique_ptr<Splat[]> vertices(new Splat[length]);
	while(true) {
		int count = loader->getVertices(length, vertices.get());

		if(count == 0) break;
		for(int i = 0; i < count; i++) {
			assert(vertices[i].node == 0);
			pushVertex(vertices[i]);
		}
	}
}

void StreamCloud::pushVertex(Splat &vertex) {

	//ignore degenerate faces
	vcg::Point3f p(vertex.v);
	box.Add(p);

	uint64_t level = getLevel(current_triangle);
	assert(levels.size() >= level);

	uint64_t block = 0;
	if(levels.size() == level) {  //need to add a level
		levels.push_back(std::vector<uint64_t>());
		block = addBlock(level);

	} else {
		block = levels[level].back();
		if(isBlockFull(block))
			block = addBlock(level);
	}

	Cloud cloud = get(block);
	cloud.push_back(vertex);

	current_triangle++;
}

Cloud StreamCloud::streamVertices() {
	if(current_block == 0)
		computeOrder();

	if(current_block == order.size())
		return Cloud(NULL, NULL, 0);

	flush(); //save memory
	uint64_t block = order[current_block];
	current_block++;

	return get(block);
}

void StreamCloud::clearVirtual() {
	VirtualVertexCloud::clear();
}

uint64_t StreamCloud::addBlock(uint64_t level) {
	uint64_t block = VirtualVertexCloud::addBlock();
	levels[level].push_back(block);
	return block;
}

