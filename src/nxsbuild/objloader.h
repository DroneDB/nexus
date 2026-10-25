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
#include "../common/textfile.h"
#include "../common/virtualarray.h"

#include <string>
#include <unordered_map>
#include <vector>


class ObjLoader: public MeshLoader {
public:
	//mtl, when not empty, is the material library to use instead of the one the model names
	ObjLoader(const std::string &file, const std::string &mtl);
	~ObjLoader();

	void setMaxMemory(uint64_t max_memory);
	uint32_t getTriangles(uint32_t size, Triangle *buffer);
	uint32_t getVertices(uint32_t size, Splat *vertex);

private:
	//the vertex, texture coordinate and normal of one corner of a face as zero based indices,
	//-1 when the face does not give one
	struct Corner {
		int64_t vertex, uv, normal;
	};

	void readMTL(); //obj file searched for mtllib
	void cacheTextureUV();
	void cacheVertices();
	//reads the corners of a face line into corners; throws when it is malformed or refers to
	//an element the file does not define
	void parseFace(const char *line);
	int64_t resolveIndex(int64_t index, uint64_t seen, uint64_t total, const char *what, const char *line);

	nx::InputFile file;
	std::string filename;
	std::string mtl;
	VirtualArray<Vertex> vertices;
	std::vector<float> vtxtuv;
	std::vector<Corner> corners;
	uint64_t n_vertices;
	uint64_t n_normals = 0;
	uint64_t n_triangles;
	uint64_t current_triangle;
	uint64_t current_vertex;
	uint64_t current_tri_pos = 0;
	uint64_t current_vertex_pos = 0;
	bool cached = false;
	//how many vertices, texture coordinates and normals the lines read so far define: a
	//negative index counts back from them
	uint64_t seen_vertices = 0, seen_uvs = 0, seen_normals = 0;
	uint32_t  current_color = 0;
	int32_t  current_texture_id = -1;
	std::unordered_map<std::string, uint32_t> colors_map;
	std::unordered_map<std::string, std::string> textures_map;
};
#endif // NX_OBJLOADER_H
