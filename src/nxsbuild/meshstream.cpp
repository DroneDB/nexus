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

#include <filesystem>
#include <algorithm>

#include "../common/logger.h"

#include "meshstream.h"
#include "meshloader.h"
#include "plyloader.h"
#include "tsploader.h"
#include "objloader.h"
#include "stlloader.h"
#include "vcgloadermesh.h"
#include "vcgloader.h"
#include "tsloader.h"


#include <iostream>

using namespace std;
namespace fs = std::filesystem;

Stream::Stream():
	has_colors(false),
	has_normals(false),
	has_textures(false),
	vertex_quantization(0),
	current_triangle(0),
	current_block(0) {
}

void Stream::setVertexQuantization(double q) {
	vertex_quantization = q;
}

MeshLoader *Stream::getLoader(std::string file, std::string material) {
	MeshLoader *loader = nullptr;

	auto endsWith = [](const std::string &s, const std::string &suffix) {
		if(suffix.size() > s.size()) return false;
		return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin(),
			[](char a, char b){ return tolower(a) == tolower(b); });
	};

	if(endsWith(file, ".ply"))
		loader = new PlyLoader(file);

	else if(endsWith(file, ".tsp"))
		loader = new TspLoader(file);

	else if(endsWith(file, ".obj"))
		loader = new ObjLoader(file, material);

	else if(endsWith(file, ".stl"))
		loader = new STLLoader(file);

	else if(endsWith(file, ".ts")) {
		TsLoader *ts = new TsLoader(file);
		loader = ts;
		if(!colormap.empty())
			ts->useColormapFor(colormap[0], colormap[1]);
	}

	else
		loader = new VcgLoader<VcgMesh>(file);

	return loader;
}

vcg::Box3d Stream::getBox(std::vector<std::string> paths) {

	vcg::Box3d box;

	uint32_t length = (1<<20);
	Splat *vertices = new Splat[length];


	for(auto &file : paths) {
		MeshLoader *loader = getLoader(file, std::string());
		loader->setMaxMemory(512*(1<<20));
		while(true) {
			int count = loader->getVertices(length, vertices);
			if(count == 0) break;
		}
		box.Add(loader->box);
		delete loader;
	}
	delete []vertices;
	return box;
}

void Stream::load(MeshLoader *loader) {
	loader->setVertexQuantization((float)vertex_quantization);
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

void Stream::load(std::vector<std::string> paths, std::string material) {

	has_colors = true;
	has_normals = true;
	has_textures = true;
	for(auto &file : paths) {
		MeshLoader *loader = getLoader(file, material);
		load(loader);
		delete loader;
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

uint64_t Stream::getLevel(int64_t v) {
	static const int MultiplyDeBruijnBitPosition[32] =  {
		0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
		31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
	};
	return MultiplyDeBruijnBitPosition[((unsigned int)((v & -v) * 0x077CB531U)) >> 27];
}

void Stream::computeOrder() {
	order.clear();
	for(int level = (int)levels.size()-1; level >= 0; level--) {
		std::vector<uint64_t> &level_blocks = levels[level];
		for(uint i = 0; i < level_blocks.size(); i++) {
			order.push_back(level_blocks[i]);
		}
	}
}



//SOUP

StreamSoup::StreamSoup(std::string prefix):
	VirtualTriangleSoup(prefix) {
}

void StreamSoup::loadMesh(MeshLoader *loader) {
	loader->setMaxMemory(maxMemory());
	loader->texOffset = (int)textures.size();
	uint32_t length = (1<<20);
	Triangle *triangles = new Triangle[length];
	while(true) {
		int count = loader->getTriangles(length, triangles);
		if(count == 0) break;
		for(int i = 0; i < count; i++) {
			assert(triangles[i].node == 0);
			pushTriangle(triangles[i]);
		}
	}
	delete []triangles;
}

void StreamSoup::pushTriangle(Triangle &triangle) {

	vcg::Point3f v[3];
	for(int k = 0; k < 3; k++) {
		v[k] = vcg::Point3f(triangle.vertices[k].v);
		box.Add(v[k]);
	}

	uint64_t level = getLevel(current_triangle);
	assert(levels.size() >= level);

	uint64_t block = 0;
	if(levels.size() == level) {
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

	flush();
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

StreamCloud::StreamCloud(std::string prefix):
	VirtualVertexCloud(prefix) {
}

void StreamCloud::loadMesh(MeshLoader *loader) {
	loader->setMaxMemory(maxMemory());
	uint32_t length = (1<<20);
	Splat *vertices = new Splat[length];
	while(true) {
		int count = loader->getVertices(length, vertices);

		if(count == 0) break;
		for(int i = 0; i < count; i++) {
			assert(vertices[i].node == 0);
			pushVertex(vertices[i]);
		}
	}
	delete []vertices;
}

void StreamCloud::pushVertex(Splat &vertex) {

	vcg::Point3f p(vertex.v);
	box.Add(p);

	uint64_t level = getLevel(current_triangle);
	assert(levels.size() >= level);

	uint64_t block = 0;
	if(levels.size() == level) {
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

	flush();
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
