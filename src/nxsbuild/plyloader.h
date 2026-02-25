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
#ifndef NX_PLYLOADER_H
#define NX_PLYLOADER_H

#include "meshloader.h"
#include "../common/virtualarray.h"

#include <string>
#include <cstdint>
#include <wrap/ply/plylib.h>


class PlyLoader: public MeshLoader {
public:
	PlyLoader(std::string file);
	~PlyLoader();

	void setMaxMemory(uint64_t max_memory);
	uint32_t getTriangles(uint32_t size, Triangle *buffer);
	uint32_t getVertices(uint32_t size, Splat *vertex);

	uint32_t nVertices() { return (uint32_t)n_vertices; }
	uint32_t nTriangles() { return (uint32_t)n_triangles; }
private:
	vcg::ply::PlyFile pf;
	bool double_coords = false;
	bool has_vertex_tex_coords = false;
	int64_t vertices_element;
	int64_t faces_element;

	VirtualArray<Vertex> vertices;
	uint64_t n_vertices;
	uint64_t n_triangles;
	uint64_t current_triangle;
	uint64_t current_vertex;

	void init();
	void cacheVertices();
};

#endif // NX_PLYLOADER_H

