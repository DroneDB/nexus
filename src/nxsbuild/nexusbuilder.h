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
#ifndef NX_NEXUSBUILDER_H
#define NX_NEXUSBUILDER_H

#include <vector>
#include <string>
#include <mutex>
#include <cstdint>

#include <vcg/space/box3.h>

#include "../common/signature.h"
#include "../common/dag.h"
#include "../common/virtualarray.h"
#include "../common/image.h"
#include "../common/mmap_file.h"
#include "texpyramid.h"



class KDTree;
class KDTreeSoup;
class KDTreeCloud;
class Stream;
class StreamSoup;
class StreamCloud;

class TMesh;

//Structures used for normalization
struct NodeBox {
	vcg::Point3f axes[3];
	vcg::Box3f box;

	NodeBox() {}
	NodeBox(KDTree *tree, uint32_t block);
	bool isIn(vcg::Point3f &p);
	std::vector<bool> markBorders(nx::Node &node, vcg::Point3f *p, uint16_t *f);
};

class NVertex {
public:
	NVertex(uint32_t b, uint32_t i, vcg::Point3f p, vcg::Point3s *n):
		node(b), index(i), point(p), normal(n) {}
	uint32_t node;
	uint32_t index;
	vcg::Point3f point;
	vcg::Point3s *normal;

	bool operator<(const NVertex &v) const {
		if(point == v.point)
			return node > v.node;  //ascending orde: the first will be the highest node order (the leafest node)
		return point < v.point;
	}
};


class NexusBuilder {
public:
	enum Components { FACES = 1, NORMALS = 2, COLORS = 4, TEXTURES = 8 };
	NexusBuilder(uint32_t components);
	NexusBuilder(nx::Signature &sig);

	bool hasNormals() { return header.signature.vertex.hasNormals(); }
	bool hasColors() { return header.signature.vertex.hasColors(); }
	bool hasTextures() const { return header.signature.vertex.hasTextures(); }

	void initAtlas(std::vector<nx::Image>& textures);
	bool initAtlas(std::vector<LoadTexture>& textures);
	void create(KDTree *input, Stream *output, uint top_node_size);
	void createLevel(KDTree *input, Stream *output, int level);
	void createCloudLevel(KDTreeCloud *input, StreamCloud *output, int level);
	void createMeshLevel(KDTreeSoup *input, StreamSoup *output, int level);


	void setMaxMemory(uint64_t m) {
		max_memory = m;
		chunks.setMaxMemory(m);
		atlas.cache_max = m;
	}
	void setScaling(float s) { scaling = s; atlas.scale = sqrt(scaling); }

	void saturate();

	void reverseDag();
	void save(std::string filename);

	std::mutex m_input;     //locks input stream when building nodes multithread
	std::mutex m_output;    //locks output stream when building nodes multithread
	std::mutex m_builder;   //locks builders data (patches, etc.)
	std::mutex m_chunks;    //locks builder chunks (the cache)
	std::mutex m_atlas;     //locks atlas (the cache)
	std::mutex m_texsimply; //locks the temporary data simplification structure for texture.
	std::mutex m_textures;  //locks texture temporary file


	nx::MappedFile file;

	VirtualChunks chunks;
	std::vector<NodeBox> boxes; //a box for each node

	nx::Header header;
	std::vector<nx::Node> nodes;
	std::vector<nx::Patch> patches;
	std::vector<nx::Texture> textures;
	std::vector<std::string> images;

	uint64_t input_pixels, output_pixels;
	nx::TexAtlas atlas;
	nx::TempMappedFile nodeTex; //texture images for each node stored here.
	uint64_t max_memory;
	int n_threads = 4;

	float scaling;
	bool useNodeTex; //use node textures
	int tex_quality;
	int max_node_triangles = 32000;
	int max_node_tex_size = 4096;
	bool createPowTwoTex;
	bool deepzoom = false; //use deepzoom style where each node is in a different file.

	//if too many texel per edge, simplification is inhibited, but don't quit prematurely
	int skipSimplifyLevels = 0;

	void processBlock(KDTreeSoup *input, StreamSoup *output, uint block, int level);

	nx::Image extractNodeTex(TMesh &mesh, int level, float &error, float &pixelXedge);
	void invertNodes(); //
	void saturateNode(uint32_t n);
	void optimizeNode(uint32_t node, unsigned char *chunk);
	void uniformNormals();
	void appendBorderVertices(uint32_t origin, uint32_t destination, std::vector<NVertex> &vertices);

	void testSaturation();
};

#endif // NX_NEXUSBUILDER_H
