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

#include <memory>

#include "meshloader.h"
#include "../common/virtualarray.h"


#include <wrap/ply/plylib.h>


struct PlyFace;

class PlyLoader: public MeshLoader {
public:
	//cachePrefix: the scratch file of the vertices goes there (see VirtualMemory)
	PlyLoader(const std::string &file, const std::string &cachePrefix);
	~PlyLoader();

	void setMaxMemory(uint64_t max_memory);
	uint32_t getTriangles(uint32_t size, Triangle *buffer);
	uint32_t getVertices(uint32_t size, Splat *vertex);

	uint32_t nVertices() { return n_vertices; }
	uint32_t nTriangles() { return n_triangles; }
private:
	vcg::ply::PlyFile pf;
	bool double_coords = false;
	bool has_vertex_tex_coords = false;
	bool has_face_tex_coords = false;
	int64_t vertices_element;
	int64_t faces_element;

	VirtualArray<Vertex> vertices;
	uint64_t n_vertices;
	uint64_t n_triangles;
	uint64_t current_triangle;
	uint64_t current_vertex;

	//the face being cut into triangles, and the vertex the next of them starts from
	std::unique_ptr<PlyFace> face;
	int face_corner = 0;

	void init();
	void cacheVertices();
	void readFace();
	//reads the next element of the current ply element type, throws if the file ends or is malformed
	void readElement(void *element, const char *name, uint64_t index, uint64_t total);
};

#endif // NX_PLYLOADER_H

