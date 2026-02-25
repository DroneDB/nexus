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
#include "objloader.h"

#include <filesystem>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <stdexcept>

namespace fs = std::filesystem;

#define RED(c) (c >> 24)
#define GREEN(c) ((c >> 16) & 0xff)
#define BLUE(c) ((c >> 8) & 0xff)
#define ALPHA(c) (c & 0xff)

// --- helpers ---------------------------------------------------------------

static std::string trim(const std::string &s) {
	auto b = s.find_first_not_of(" \t\r\n");
	if (b == std::string::npos) return {};
	auto e = s.find_last_not_of(" \t\r\n");
	return s.substr(b, e - b + 1);
}

static bool iStartsWith(const std::string &s, const std::string &prefix) {
	if (prefix.size() > s.size()) return false;
	for (size_t i = 0; i < prefix.size(); i++)
		if (tolower((unsigned char)s[i]) != tolower((unsigned char)prefix[i]))
			return false;
	return true;
}

static std::vector<std::string> splitWhitespace(const std::string &s) {
	std::vector<std::string> parts;
	std::istringstream iss(s);
	std::string tok;
	while (iss >> tok) parts.push_back(tok);
	return parts;
}

static std::string removeQuotes(const std::string &s) {
	std::string r = s;
	if (!r.empty() && r.front() == '"') r.erase(r.begin());
	if (!r.empty() && r.back() == '"') r.pop_back();
	return r;
}

// ---------------------------------------------------------------------------

ObjLoader::ObjLoader(std::string filename, std::string _mtl):
	vertices("cache_plyvertex"),
	n_vertices(0),
	n_triangles(0),
	current_vertex(0) {

	mtl = _mtl;
	filepath = filename;
	file.open(filename, std::ios::binary);
	if (!file.is_open())
		throw std::runtime_error("could not open file " + filename);

	readMTL();
}

ObjLoader::~ObjLoader() {
	file.close();
}

void ObjLoader::cacheTextureUV() {

	vtxtuv.reserve(n_vertices * 2);

	char buffer[1024];
	file.clear();
	file.seekg(0);
	int cnt = 0;
	while (file.getline(buffer, 1024)) {
		if (buffer[0] != 'v' || buffer[1] != 't')
			continue;

		if (buffer[2] == ' ') {
			float vt0 = 0.0f, vt1 = 0.0f;
			int n = sscanf(buffer, "vt %f %f", &vt0, &vt1);
			if (n != 2) throw std::runtime_error(std::string("error parsing vtxt line: ") + buffer);
			cnt++;
			vtxtuv.push_back(vt0);
			vtxtuv.push_back(vt1);
		}
	}
}

void ObjLoader::readMTL() {

	char buffer[1024];

	if (!mtl.empty()) {
		if (!fs::exists(mtl))
			throw std::runtime_error("Could not find .mtl file: " + mtl);
	}

	if (mtl.empty()) { //look for mtllib
		file.clear();
		file.seekg(0);
		while (file.getline(buffer, 1024)) {
			if (strncmp(buffer, "mtllib", 6) != 0)
				continue;

			std::string m = trim(std::string(buffer).substr(7));
			if (fs::exists(m))
				mtl = m;
			break;
		}
	}
	if (mtl.empty()) { //assume the name is the same
		fs::path info(filepath);
		mtl = (info.parent_path() / info.stem()).string() + ".mtl";
	}

	if (!fs::exists(mtl))
		return;

	std::ifstream f(mtl, std::ios::binary);
	if (!f.is_open())
		return;

	int cnt = 0;
	bool head_line_was_read = false;
	std::string line;

	while (true) {
		if (!head_line_was_read) {
			if (!std::getline(f, line))
				break;
			line = trim(line);
			if (line.empty() || line[0] == '#')
				continue;
		} else {
			head_line_was_read = false;
		}

		if (iStartsWith(line, "newmtl")) {
			std::string mtltag = trim(line.substr(6));
			std::string txtfname;
			int32_t R = (int32_t)0xff000000;
			int32_t G = 0x00ff0000;
			int32_t B = 0x0000ff00;
			int32_t A = 255;

			do {
				if (!std::getline(f, line))
					break;
				line = trim(line);

				if (iStartsWith(line, "newmtl")) {
					head_line_was_read = true;
					break;
				}
				if (iStartsWith(line, "d ")) {
					float d = 1.0f;
					int n = sscanf(line.c_str(), "d %f", &d);
					if (n == 1) A = (int32_t)(255 * d);
					continue;
				}
				if (iStartsWith(line, "Tr")) {
					float tr = 0.0f;
					int n = sscanf(line.c_str(), "Tr %f", &tr);
					if (n == 1) A = (int32_t)(255 * (1.0f - tr));
					continue;
				}
				if (iStartsWith(line, "Map_Kd")) {
					txtfname = removeQuotes(trim(line.substr(6)));
					continue;
				}
				if (iStartsWith(line, "Kd")) {
					float r, g, b;
					std::istringstream ss(line);
					std::string kd;
					ss >> kd >> r >> g >> b;
					if (ss) {
						R = ((int32_t)(255 * r) << 24) & (int32_t)0xff000000;
						G = ((int32_t)(255 * g) << 16) & 0x00ff0000;
						B = ((int32_t)(255 * b) << 8) & 0x0000ff00;
					}
					continue;
				}

			} while (true);

			int32_t color = R + G + B + A;
			colors_map[mtltag] = (uint32_t)color;

			if (!txtfname.empty()) {
				sanitizeTextureFilepath(txtfname);
				resolveTextureFilepath(filepath, txtfname);

				textures_map[mtltag] = txtfname;
				bool exists = false;
				for (auto &fn : texture_filenames)
					if (fn.filename == txtfname) {
						exists = true;
						break;
					}
				if (!exists)
					texture_filenames.push_back(LoadTexture(txtfname));
			}
			cnt++;
		}
	}
	if (texture_filenames.size() > 0)
		has_textures = true;
	if (cnt)
		has_colors = true;
}

void ObjLoader::cacheVertices() {
	vertices.setElementsPerBlock(1<<20);
	file.clear();
	file.seekg(0);
	char buffer[1024];
	int cnt = 0;

	while (file.getline(buffer, 1024)) {
		if (buffer[0] == 'v') {
			if (buffer[1] == ' ') {
				vertices.resize(n_vertices + 1);
				Vertex &vertex = vertices[n_vertices];
				n_vertices++;

				vcg::Point3d p;
				int n = sscanf(buffer, "v %lf %lf %lf", &p[0], &p[1], &p[2]);
				if (n != 3)
					throw std::runtime_error(std::string("error parsing vertex line while caching: ") + buffer);
				p -= origin;
				p[0] *= scale[0];
				p[1] *= scale[1];
				p[2] *= scale[2];
				box.Add(p);

				vertex.v[0] = (float)p[0];
				vertex.v[1] = (float)p[1];
				vertex.v[2] = (float)p[2];

				cnt++;
				if (quantization) {
					quantize(vertex.v[0]);
					quantize(vertex.v[1]);
					quantize(vertex.v[2]);
				}
			}
			continue;

		} else if (buffer[0] == 'm' && strncmp(buffer, "mtllib", 6) == 0) {
			if (!mtl.empty()) {
				fs::path info(filepath);
				std::string m = trim(std::string(buffer).substr(7));
				m = removeQuotes(m);
				mtl = (info.parent_path() / m).string();
			}
		}
	}
}

void ObjLoader::setMaxMemory(uint64_t max_memory) {
	vertices.setMaxMemory(max_memory);
}

uint32_t ObjLoader::getTriangles(uint32_t size, Triangle *faces) {

	if (n_triangles == 0) {
		cacheVertices();
		cacheTextureUV();
	}

	char buffer[1024];
	file.clear();
	file.seekg(current_tri_pos);

	int32_t R = (0x7f << 24);
	int32_t G = (0x7f << 16);
	int32_t B = (0x7f << 8);
	int32_t A = 255;
	uint32_t default_color = (uint32_t)(R + G + B + A);

	uint32_t count = 0;
	int64_t cpos = current_tri_pos;

	while (count < size) {
		cpos = (int64_t)file.tellg();
		if (!file.getline(buffer, 1024)) {
			cpos = (int64_t)file.tellg();
			break;
		}

		if (has_colors && buffer[0] == 'u') {
			std::string str = trim(std::string(buffer));
			// extract the material name after "usemtl "
			auto sp = str.find(' ');
			std::string matname = (sp != std::string::npos) ? trim(str.substr(sp + 1)) : "";

			auto cit = colors_map.find(matname);
			current_color = (cit != colors_map.end()) ? cit->second : default_color;

			current_texture_id = -1;
			auto tit = textures_map.find(matname);
			if (tit != textures_map.end() && !tit->second.empty()) {
				for (int i = 0; i < (int)texture_filenames.size(); i++) {
					if (texture_filenames[i].filename == tit->second)
						current_texture_id = i;
				}
			}
			continue;
		}

		if (buffer[0] != 'f')
			continue;

		std::string str = trim(buffer);
		auto list = splitWhitespace(str);
		// remove "f"
		if (!list.empty()) list.erase(list.begin());
		// remove trailing newline or comment tokens
		while (!list.empty() && (list.back()[0] == '\n' || list.back()[0] == '\r' || list.back()[0] == '#'))
			list.pop_back();

		int valence = (int)list.size();

		if (count + (valence - 2) >= size)
			break;

		if (valence >= 3) {
			int *face_ = new int[valence];
			int *normal_ = new int[valence];
			int *vtxt_ = new int[valence];
			for (int i = 0; i < valence; i++) {
				normal_[i] = -1;
				vtxt_[i] = -1;
			}

			int *face1 = new int[(valence - 2) * 3];
			int *normal1 = new int[(valence - 2) * 3];
			int *vtxt1 = new int[(valence - 2) * 3];

			for (int w = 0; w < valence; w++) {
				int n = (int)list[w].length();
				int rr[3]; rr[0] = rr[1] = rr[2] = 0;
				int rri = 0;
				int cntSlashes = 0;
				int cntConsecutiveSlashes = 0;
				bool lastCharWasSlash = false;
				for (int i = 0; i < n; i++) {
					char c = list[w][i];
					if (c == '/') {
						cntSlashes++;
						if (lastCharWasSlash) cntConsecutiveSlashes++;
						if (rri < 3) rri++;
						lastCharWasSlash = true;
					} else {
						rr[rri] = rr[rri] * 10 + (c - '0');
						lastCharWasSlash = false;
					}
				}
				face_[w] = rr[0] - 1;
				vtxt_[w] = rr[1] - 1;
				normal_[w] = rr[2] - 1;

				bool vertexIndicesFormat = (cntSlashes == 0) && (cntConsecutiveSlashes == 0);
				bool vertexTextureCoordIndicesFormat = (cntSlashes == 1) && (cntConsecutiveSlashes == 0);
				bool vertexNormalIndicesFormat = (cntSlashes == 2) && (cntConsecutiveSlashes == 0);
				bool vertexNormalIndicesWithoutTextureCoordIndicesFormat = (cntSlashes == 2) && (cntConsecutiveSlashes == 1);

				bool faceHasRelativeIndices = false;
				if (vertexIndicesFormat)
					faceHasRelativeIndices = (face_[w] < 0);
				else if (vertexTextureCoordIndicesFormat)
					faceHasRelativeIndices = (face_[w] < 0) || (vtxt_[w] < 0);
				else if (vertexNormalIndicesFormat)
					faceHasRelativeIndices = (face_[w] < 0) || (vtxt_[w] < 0) || (normal_[w] < 0);
				else if (vertexNormalIndicesWithoutTextureCoordIndicesFormat)
					faceHasRelativeIndices = (face_[w] < 0) || (normal_[w] < 0);

				if (faceHasRelativeIndices)
					throw std::runtime_error("Relative indexes in OBJ are not supported");
			}

			for (int j = 0; j < valence - 2; j++) {
				face1[j * 3 + 0] = face_[0];
				normal1[j * 3 + 0] = normal_[0];
				vtxt1[j * 3 + 0] = vtxt_[0];

				face1[j * 3 + 1] = face_[j + 1];
				normal1[j * 3 + 1] = normal_[j + 1];
				vtxt1[j * 3 + 1] = vtxt_[j + 1];

				face1[j * 3 + 2] = face_[j + 2];
				normal1[j * 3 + 2] = normal_[j + 2];
				vtxt1[j * 3 + 2] = vtxt_[j + 2];
			}

			for (int m = 0; m <= valence - 3; m++) {
				Triangle &current = faces[count];
				for (int k = 0; k < 3; k++) {
					current.vertices[k] = vertices[face1[m * 3 + k]];
					if (vtxt1[m * 3 + k] >= 0)
						for (int j = 0; j < 2; j++)
							current.vertices[k].t[j] = vtxtuv[vtxt1[m * 3 + k] * 2 + j];
				}
				current.tex = current_texture_id;
				if (has_colors && current_color) {
					current.vertices[0].c[0] = RED(current_color);
					current.vertices[0].c[1] = GREEN(current_color);
					current.vertices[0].c[2] = BLUE(current_color);
					current.vertices[0].c[3] = ALPHA(current_color);

					current.vertices[1].c[0] = RED(current_color);
					current.vertices[1].c[1] = GREEN(current_color);
					current.vertices[1].c[2] = BLUE(current_color);
					current.vertices[1].c[3] = ALPHA(current_color);

					current.vertices[2].c[0] = RED(current_color);
					current.vertices[2].c[1] = GREEN(current_color);
					current.vertices[2].c[2] = BLUE(current_color);
					current.vertices[2].c[3] = ALPHA(current_color);
				}
				current.node = 0;
				if (current.isDegenerate()) {
					continue;
				} else {
					count++;
					n_triangles++;
				}
			}
			delete[] face_;
			delete[] normal_;
			delete[] vtxt_;
			delete[] face1;
			delete[] normal1;
			delete[] vtxt1;
			cpos = (int64_t)file.tellg();

		} else {
			throw std::runtime_error(std::string("could not parse face: ") + buffer);
		}
	}

	current_tri_pos = cpos;
	return count;
}

uint32_t ObjLoader::getVertices(uint32_t size, Splat *vertices) {
	char buffer[1024];

	uint32_t count = 0;
	while (count < size) {
		if (!file.getline(buffer, 1024))
			return count;

		if (buffer[0] != 'v')
			continue;
		if (buffer[1] != ' ')
			continue;

		Splat &vertex = vertices[count];

		vcg::Point3d p;
		int n = sscanf(buffer, "v %lf %lf %lf", &p[0], &p[1], &p[2]);
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
