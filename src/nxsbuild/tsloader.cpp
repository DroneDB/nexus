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
#include "tsloader.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "../common/error.h"
#include "../common/textparse.h"

using namespace std;
using namespace nx;

namespace {

//the pieces of a line between single spaces, empty ones included
std::vector<std::string> splitSpaces(const std::string &line) {
	std::vector<std::string> parts;
	size_t start = 0;
	while(true) {
		const size_t space = line.find(' ', start);
		if(space == std::string::npos) {
			parts.push_back(line.substr(start));
			return parts;
		}
		parts.push_back(line.substr(start, space - start));
		start = space + 1;
	}
}

//the whole text as a number; false when it is not one
bool toFloat(const std::string &text, float &value) {
	const char *p = text.c_str();
	if(!text::parseFloat(p, value))
		return false;
	return *text::skipSpaces(p) == '\0';
}

//a number, or zero for text that is not one
float toFloatOrZero(const std::string &text) {
	float value = 0.0f;
	return toFloat(text, value) ? value : 0.0f;
}

}

TsLoader::TsLoader(const std::string &filename):
	vertices("cache_tsvertex"),
	n_vertices(0),
	n_triangles(0),
	current_vertex(0) {

	if(!file.open(filename))
		throw Error("could not open file " + filename);

	//search for properties, here an example:
	/*
	PROPERTY_CLASS_HEADER temperature {
	kind: Real Number
	unit: unitless
	pclip: 99
	low_clip: 9
	high_clip: 143.54482
	}
	*/
}

bool TsLoader::useColormapFor(const std::string &_property, const std::string &palette) {
	property = _property;

	bool inside_property = false;
	const char *text;
	size_t length;
	while(file.readLine(text, length)) {
		const std::string line = text::trimmed(std::string(text, length));
		if(line.empty())
			break;
		if(line.compare(0, 16, "PROPERTY_CLASSES") == 0) {
			std::vector<std::string> props = splitSpaces(line);
			auto found = std::find(props.begin(), props.end(), property);
			if(found == props.end()) {
				throw Error("Could not find property: " + property);
			}
			property_position = int(found - props.begin()) - 1;
		}

		if(line.compare(0, 22 + property.size(), "PROPERTY_CLASS_HEADER " + property) == 0) {
			inside_property = true;
		}

		if(inside_property) {
			if(line.compare(0, 8, "low_clip") == 0) {
				if(!toFloat(splitSpaces(line).back(), colormap.minValue)) throw Error("Proble parsing line: " + line);
			}
			if(line.compare(0, 9, "high_clip") == 0) {
				if(!toFloat(splitSpaces(line).back(), colormap.maxValue)) throw Error("Proble parsing line: " + line);
			}
			if(line[0] == '}') {
				break;
			}
		}
		if(line.compare(0, 5, "TFACE") == 0) {
			break;
		}
	}
	file.seek(0);

	bool found = colormap.setColormap(palette);

	if(!found) {
		throw Error("Could not find colormap: " + palette + " (values accepted are plasma, spectral and viridis)");
	}
	return true;
}

TsLoader::~TsLoader() {
	file.close();
}



void TsLoader::cacheVertices() {
	vertices.setElementsPerBlock(1<<20);
	file.seek(0);
	const char *buffer;
	size_t s;
	int cnt = 0;

	while(1) {

		if (!file.readLine(buffer, s)) {                     //end of file
			std::cout << "Vertices read: " << cnt << std::endl;
			break;

		}

		if(strncmp(buffer, "VRTX", 4) == 0 || strncmp(buffer, "PVRTX", 5) == 0) {

			vertices.resize(n_vertices+1);
			Vertex &vertex = vertices[n_vertices];
			n_vertices++;

			vcg::Point3d p;
			std::vector<std::string> parts = splitSpaces(text::trimmed(buffer));
			if(parts.size() < 5 || (property_position >= 0 && parts.size() <= size_t(5 + property_position)))
				throw Error(std::string("error parsing vertex line ") + buffer + " while caching");
			p[0] = toFloatOrZero(parts[2]);
			p[1] = toFloatOrZero(parts[3]);
			p[2] = toFloatOrZero(parts[4]);

			//std::string pattern = "%*s %*d %lf %lf %lf";
			//int n = sscanf(buffer, pattern.c_str(), &p[0], &p[1], &p[2]);

			p -= origin;
			p[0] *= scale[0];
			p[1] *= scale[1];
			p[2] *= scale[2];
			box.Add(p);

			vertex.v[0] = (float)p[0];
			vertex.v[1] = (float)p[1];
			vertex.v[2] = (float)p[2];

			if(property_position >= 0) {
				float value = toFloatOrZero(parts[5 + property_position]);
				std::array<unsigned char, 4> c = colormap.map(value);
				for(int k = 0; k < 4; k++)
					vertex.c[k] = c[k];
			}

			cnt++;
			if(quantization) {
				quantize(vertex.v[0]);
				quantize(vertex.v[1]);
				quantize(vertex.v[2]);
			}
		}
		if(strncmp(buffer, "ATOM", 4) == 0) {
			int id;
			int n = sscanf(buffer, "%*s %*d %d", &id);
			if(n != 1) throw Error(std::string("error parsing atom line ") + buffer + " while caching");
			if(id < 1 || uint64_t(id) > n_vertices) throw Error(std::string("atom line ") + buffer + " refers to a vertex that is not defined");
			vertices.resize(n_vertices+1);
			vertices[n_vertices] = vertices[id -1];
		}
	}
}

void TsLoader::setMaxMemory(uint64_t max_memory) {
	vertices.setMaxMemory(max_memory);
}

uint32_t TsLoader::getTriangles(uint32_t size, Triangle *faces) {

	if (n_triangles == 0) {
		cacheVertices();
	}

	const char *buffer;
	size_t s;
	file.seek(current_tri_pos);

	uint32_t count = 0;
	uint64_t cpos = current_tri_pos;

	while (count < size) {
		cpos = file.pos();
		if (!file.readLine(buffer, s)) {                     //end of file
			cpos = file.pos();
			break;
		}

		if (strncmp(buffer, "TRGL", 4) != 0)
			continue;

		Triangle &current = faces[count];
		int f[3];
		int n = sscanf(buffer, "%*s %d %d %d", f, f+1, f+2);
		if(n != 3) throw Error(std::string("error parsing triangle line ") + buffer + " while reading");
		for(int k = 0; k < 3; k++)
			if(f[k] < 1 || uint64_t(f[k]) > n_vertices)
				throw Error(std::string("triangle line ") + buffer + " refers to a vertex that is not defined");

		current.vertices[0] = vertices[f[0]-1];
		current.vertices[1] = vertices[f[1]-1];
		current.vertices[2] = vertices[f[2]-1];

		current.node = 0;
		count++;
		n_triangles++;
		cpos = file.pos();
	}

	current_tri_pos = cpos;

	if (count == 0)
		std::cout << "faces read: " << n_triangles << std::endl;

	return count;
}


/*
 * Seeks all verteces in whole file
 */
uint32_t TsLoader::getVertices(uint32_t size, Splat *vertices) {
	const char *buffer;
	size_t s;

	uint32_t count = 0;
	while(count < size) {
		if(!file.readLine(buffer, s))                     //end of file
			return count;

		if(strncmp(buffer, "VRTX", 4) != 0 || strncmp(buffer, "PVRTX", 5) != 0)
			continue;


		Splat &vertex = vertices[count];

		vcg::Point3d p;
		int n = sscanf(buffer, "%*s %*d %lf %lf %lf", &p[0], &p[1], &p[2]);
		if(n != 3) throw Error(std::string("error parsing vertex line ") + buffer);

		p -= origin;
		p[0] *= scale[0];
		p[1] *= scale[1];
		p[2] *= scale[2];
		box.Add(p);

		vertex.v[0] = (float)p[0];
		vertex.v[1] = (float)p[1];
		vertex.v[2] = (float)p[2];

		if(quantization) {
			quantize(vertex.v[0]);
			quantize(vertex.v[1]);
			quantize(vertex.v[2]);
		}
		n_vertices++;
		current_vertex++;
		count++;
	}
	return count;
}
