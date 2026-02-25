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

#include <filesystem>
#include <sstream>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <stdexcept>

namespace fs = std::filesystem;
using namespace std;

static std::string trim(const std::string &s) {
	auto b = s.find_first_not_of(" \t\r\n");
	if (b == std::string::npos) return {};
	auto e = s.find_last_not_of(" \t\r\n");
	return s.substr(b, e - b + 1);
}

static std::vector<std::string> splitWhitespace(const std::string &s) {
	std::vector<std::string> parts;
	std::istringstream iss(s);
	std::string tok;
	while (iss >> tok) parts.push_back(tok);
	return parts;
}

TsLoader::TsLoader(std::string filename):
	vertices("cache_tsvertex"),
	n_vertices(0),
	n_triangles(0),
	current_vertex(0) {

	file.open(filename, ios::binary);
	if (!file.is_open())
		throw std::runtime_error("could not open file " + filename);
}

bool TsLoader::useColormapFor(const std::string &_property, const std::string &palette) {
	property = _property;

	bool inside_property = false;
	std::string line;
	while (std::getline(file, line)) {
		line = trim(line);
		if (line.empty())
			break;

		if (line.find("PROPERTY_CLASSES") == 0) {
			auto props = splitWhitespace(line);
			int pos = -1;
			for (int i = 0; i < (int)props.size(); i++) {
				if (props[i] == property) { pos = i; break; }
			}
			if (pos == -1)
				throw std::runtime_error("Could not find property: " + property);
			property_position = pos - 1;
		}

		if (line.find("PROPERTY_CLASS_HEADER " + property) == 0) {
			inside_property = true;
		}

		if (inside_property) {
			if (line.find("low_clip") == 0) {
				auto parts = splitWhitespace(line);
				if (parts.size() < 2)
					throw std::runtime_error("Problem parsing line: " + line);
				colormap.minValue = std::stof(parts.back());
			}
			if (line.find("high_clip") == 0) {
				auto parts = splitWhitespace(line);
				if (parts.size() < 2)
					throw std::runtime_error("Problem parsing line: " + line);
				colormap.maxValue = std::stof(parts.back());
			}
			if (!line.empty() && line[0] == '}') {
				break;
			}
		}
		if (line.find("TFACE") == 0) {
			break;
		}
	}
	file.clear();
	file.seekg(0);

	bool found = colormap.setColormap(palette);

	if (!found) {
		throw std::runtime_error("Could not find colormap (values accepted are plasma, spectral and viridis)");
		return false;
	}
	return true;
}

TsLoader::~TsLoader() {
	file.close();
}

void TsLoader::cacheVertices() {
	vertices.setElementsPerBlock(1 << 20);
	file.clear();
	file.seekg(0);
	char buffer[1024];
	int cnt = 0;

	while (file.getline(buffer, 1024)) {
		if (strncmp(buffer, "VRTX", 4) == 0 || strncmp(buffer, "PVRTX", 5) == 0) {

			vertices.resize(n_vertices + 1);
			Vertex &vertex = vertices[n_vertices];
			n_vertices++;

			vcg::Point3d p;
			std::string line(buffer);
			auto parts = splitWhitespace(trim(line));
			if ((int)parts.size() < 5 || (int)parts.size() < property_position)
				throw std::runtime_error(std::string("error parsing vertex line while caching: ") + buffer);
			p[0] = std::stof(parts[2]);
			p[1] = std::stof(parts[3]);
			p[2] = std::stof(parts[4]);

			p -= origin;
			p[0] *= scale[0];
			p[1] *= scale[1];
			p[2] *= scale[2];
			box.Add(p);

			vertex.v[0] = (float)p[0];
			vertex.v[1] = (float)p[1];
			vertex.v[2] = (float)p[2];

			if (property_position >= 0) {
				float value = std::stof(parts[5 + property_position]);
				std::array<unsigned char, 4> c = colormap.map(value);
				for (int k = 0; k < 4; k++)
					vertex.c[k] = c[k];
			}

			cnt++;
			if (quantization) {
				quantize(vertex.v[0]);
				quantize(vertex.v[1]);
				quantize(vertex.v[2]);
			}
		}
		if (strncmp(buffer, "ATOM", 4) == 0) {
			int id;
			int n = sscanf(buffer, "%*s %*d %d", &id);
			if (n != 1)
				throw std::runtime_error(std::string("error parsing atom line while caching: ") + buffer);
			vertices.resize(n_vertices + 1);
			vertices[n_vertices] = vertices[id - 1];
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

	char buffer[1024];
	file.clear();
	file.seekg(current_tri_pos);

	uint32_t count = 0;
	int64_t cpos = current_tri_pos;

	while (count < size) {
		cpos = (int64_t)file.tellg();
		if (!file.getline(buffer, 1024)) {
			cpos = (int64_t)file.tellg();
			break;
		}

		if (strncmp(buffer, "TRGL", 4) != 0)
			continue;

		Triangle &current = faces[count];
		int f[3];
		int n = sscanf(buffer, "%*s %d %d %d", f, f + 1, f + 2);
		if (n != 3)
			throw std::runtime_error(std::string("error parsing triangle line while reading: ") + buffer);

		current.vertices[0] = vertices[f[0] - 1];
		current.vertices[1] = vertices[f[1] - 1];
		current.vertices[2] = vertices[f[2] - 1];

		current.node = 0;
		count++;
		n_triangles++;
		cpos = (int64_t)file.tellg();
	}

	current_tri_pos = cpos;

	if (count == 0)
		std::cout << "faces read: " << n_triangles << std::endl;

	return count;
}

uint32_t TsLoader::getVertices(uint32_t size, Splat *vertices) {
	char buffer[1024];

	uint32_t count = 0;
	while (count < size) {
		if (!file.getline(buffer, 1024))
			return count;

		if (strncmp(buffer, "VRTX", 4) != 0 || strncmp(buffer, "PVRTX", 5) != 0)
			continue;

		Splat &vertex = vertices[count];

		vcg::Point3d p;
		int n = sscanf(buffer, "%*s %*d %lf %lf %lf", &p[0], &p[1], &p[2]);
		if (n != 3)
			throw std::runtime_error(std::string("error parsing vertex line ") + buffer);

		p -= origin;
		p[0] *= scale[0];
		p[1] *= scale[1];
		p[2] *= scale[2];
		box.Add(p);

		vertex.v[0] = (float)p[0];
		vertex.v[1] = (float)p[1];
		vertex.v[2] = (float)p[2];

		if (quantization) {
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
