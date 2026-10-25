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
#include "tsploader.h"

#include "../common/error.h"

#include <iostream>
using namespace std;


TspLoader::TspLoader(const std::string &filename) {

	has_colors = true;
	has_normals = has_textures = false;

	if(!file.open(filename))
		throw nx::Error("could not open file " + filename);
}

Triangle readTriangle(float *tmp) {
	Triangle triangle;
	unsigned int color_offset = 3 * 3 * 2; //vertices and normals
	for (int i = 0; i < 3; i++) {
		Vertex &vertex = triangle.vertices[i];
		for (int k = 0; k < 3; k++) {
			vertex.v[k] = tmp[i * 3 + k];

			vertex.c[k] = 255 * tmp[color_offset + i * 3 + k];
		}
		vertex.c[3] = 255;
	}
	triangle.node = 0;
	return triangle;
}

uint32_t TspLoader::getTriangles(uint32_t triangle_no, Triangle *buffer) {

	unsigned int vertex_size = 9 * sizeof(float);
	unsigned int triangle_size = 3 * vertex_size;
	unsigned int size = triangle_no * triangle_size;
	float *tmp = new float[size];
	uint32_t readed = file.read((char *)tmp, size) / triangle_size;

	float *pos = tmp;
	int count = 0;
	for (unsigned int i = 0; i < readed; i++) {
		Triangle &triangle = buffer[count];
		triangle = readTriangle(pos);
		pos += 3 * 9;
		if (triangle.isDegenerate()) continue;
		current_triangle++;
		count++;
	}
	delete[]tmp;

	return count;
}
