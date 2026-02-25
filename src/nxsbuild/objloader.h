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
#ifndef NX_OBJLOADER_H
#define NX_OBJLOADER_H

#include "meshloader.h"
#include "../common/virtualarray.h"

#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <cstdint>


class ObjLoader: public MeshLoader {
public:
	ObjLoader(std::string file, std::string mtl);
	~ObjLoader();

	void setMaxMemory(uint64_t max_memory);
	uint32_t getTriangles(uint32_t size, Triangle *buffer);
	uint32_t getVertices(uint32_t size, Splat *vertex);

private:

	void readMTL();
	void cacheTextureUV();
	void cacheVertices();

	std::ifstream file;
	std::string filepath;
	std::string mtl;
	VirtualArray<Vertex> vertices;
	std::vector<float> vtxtuv;
	uint64_t n_vertices;
	uint64_t n_triangles;
	uint64_t current_triangle;
	uint64_t current_vertex;
	int64_t  current_tri_pos = 0;
	uint32_t current_color = 0;
	int32_t  current_texture_id = -1;
	std::map<std::string, uint32_t> colors_map;
	std::map<std::string, std::string> textures_map;
};
#endif // NX_OBJLOADER_H
