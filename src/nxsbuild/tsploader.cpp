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

#include <iostream>
#include <stdexcept>

using namespace std;

TspLoader::TspLoader(std::string filename) {

	has_colors = true;
	has_normals = has_textures = false;

	file.open(filename, ios::binary);
	if (!file.is_open())
		throw std::runtime_error("could not open file " + filename);
}

static Triangle readTriangle(float *tmp) {
	Triangle triangle;
	uint32_t color_offset = 3 * 3 * 2; // vertices and normals
	for (int i = 0; i < 3; i++) {
		Vertex &vertex = triangle.vertices[i];
		for (int k = 0; k < 3; k++) {
			vertex.v[k] = tmp[i * 3 + k];
			vertex.c[k] = (unsigned char)(255 * tmp[color_offset + i * 3 + k]);
		}
		vertex.c[3] = 255;
	}
	triangle.node = 0;
	return triangle;
}

uint32_t TspLoader::getTriangles(uint32_t triangle_no, Triangle *buffer) {

	uint32_t vertex_size = 9 * sizeof(float);
	uint32_t triangle_size = 3 * vertex_size;
	uint32_t size = triangle_no * triangle_size;
	float *tmp = new float[size / sizeof(float)];
	file.read((char *)tmp, size);
	uint32_t readed = (uint32_t)(file.gcount() / triangle_size);

	float *pos = tmp;
	uint32_t count = 0;
	for (uint32_t i = 0; i < readed; i++) {
		Triangle &triangle = buffer[count];
		triangle = readTriangle(pos);
		pos += 3 * 9;
		if (triangle.isDegenerate()) continue;
		current_triangle++;
		count++;
	}
	delete[] tmp;

	return count;
}
