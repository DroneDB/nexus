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

#include <algorithm>
#include <cstring>
#include <iostream>

#include "../common/error.h"
#include "../common/pathutil.h"
#include "../common/textparse.h"

using namespace nx;

#define RED(c) (c >> 24) 
#define GREEN(c) ((c >> 16) & 0xff)
#define BLUE(c) ((c >> 8) & 0xff)
#define ALPHA(c) (c & 0xff)

namespace {

//a line of the file for an error message
std::string excerpt(const char *line) {
	const size_t max = 200;
	std::string text(line, std::min(std::strlen(line), max));
	if(std::strlen(line) > max)
		text += "...";
	return text;
}

bool endsToken(char c) {
	return c == '\0' || text::isSpace(c) || c == '#';
}

//255 times a value of the material, in 0..255
uint32_t channel(float value) {
	const float scaled = 255.0f * value;
	return scaled <= 0.0f ? 0u : scaled >= 255.0f ? 255u : uint32_t(scaled);
}

}

ObjLoader::ObjLoader(const std::string &filename, const std::string &_mtl):
	filename(filename),
	mtl(_mtl),
	vertices("cache_plyvertex"),
	n_vertices(0),
	n_triangles(0),
	current_vertex(0) {

	if(!file.open(filename))
		throw Error("could not open file " + filename);

	readMTL();
}

ObjLoader::~ObjLoader() {
	file.close();
}


//reads the texture coordinates (and counts the normals, which nothing reads but which a
//face may refer to)
void ObjLoader::cacheTextureUV() {

	vtxtuv.reserve(n_vertices * 2);

	file.seek(0);
	const char *line;
	size_t length;
	while (file.readLine(line, length)) {
		if (line[0] != 'v')            //skip all irrelevant staff
			continue;

		if (line[1] == 't' && text::isSpace(line[2])) {      //skip other properties
			//vt u [v [w]]: v is optional and 0 when missing, w is not used
			float vt0 = 0.0, vt1 = 0.0;
			const char *p = line + 3;
			if (!text::parseFloat(p, vt0))
				throw Error("error parsing vtxt  line: " + excerpt(line));
			p = text::skipSpaces(p);
			if (!endsToken(*p) && !text::parseFloat(p, vt1))
				throw Error("error parsing vtxt  line: " + excerpt(line));
			vtxtuv.push_back(vt0);
			vtxtuv.push_back(vt1);
		}//skipping other properties in OBJ
	}
}

void ObjLoader::readMTL() {

	const char *line;
	size_t length;

	if (!mtl.empty()) {
		if(!pathExists(mtl))
			throw Error("Could not find .mtl file: " + mtl);
	}

	if(mtl.empty()) { //look for mtllib
		while (file.readLine(line, length)) {
			if(strncmp(line, "mtllib", 6) != 0)
				continue;

			std::string m = text::trimmed(length > 7 ? std::string(line + 7) : std::string());
			m = joinPath(parentPath(filename), m);
			if (pathExists(m))
				mtl = m;
			break;
		}
	}
	if(mtl.empty()) { //assume the name is the same
		mtl = joinPath(parentPath(filename), completeBaseName(filename) + ".mtl");
	}

	if (!pathExists(mtl))
		return;

	InputFile f;
	if (!f.open(mtl))
		return;

	//the material being read; it is complete when the next one starts or the file ends
	struct Material {
		bool started = false;
		std::string tag;
		std::string texture;
		uint32_t R = 0xff000000, G = 0x00ff0000, B = 0x0000ff00, A = 255;
	} material;
	int cnt = 0;

	auto finish = [&]() {
		if (!material.started)
			return;
		colors_map[material.tag] = material.R + material.G + material.B + material.A;

		std::string txtfname = material.texture;
		if (txtfname.length() > 0) {
			sanitizeTextureFilepath(txtfname);
			//a texture name is relative to the .mtl that names it; when the .mtl is in
			//another folder and the name is not found next to it, the .obj's folder is
			//still tried, which is where every name was looked up before.
			std::string nextToMtl = joinPath(parentPath(mtl), txtfname);
			if (pathExists(nextToMtl))
				txtfname = nextToMtl;
			else
				resolveTextureFilepath(filename, txtfname);

			textures_map[material.tag] = txtfname;
			bool exists = false;
			for (const auto &fn : texture_filenames)
				if (fn.filename == txtfname) {
					exists = true;
					break;
				}
			if (!exists) {
				texture_filenames.push_back(LoadTexture(txtfname));
			}
		}
		cnt++;
		material = Material();
	};

	while (f.readLine(line, length)) {
		if (line[0] == '#')            //skip comments
			continue;

		const std::string str = text::simplified(line, length);
		const std::string word = text::firstWord(str);

		if (text::equalsNoCase(word, "newmtl")) {
			finish();
			material.started = true;
			material.tag = text::afterFirstWord(str);
			continue;
		}
		if (!material.started)
			continue;

		const char *args = str.c_str() + std::min(word.size() + 1, str.size());
		if (word == "d") {
			float d;
			if (text::parseFloat(args, d))
				material.A = channel(d);
		} else if (text::equalsNoCase(word, "map_kd")) {
			//the name, without the quotes some exporters put around it
			std::string name = text::trimmed(text::afterFirstWord(str));
			if (!name.empty() && name.front() == '"')
				name.erase(0, 1);
			if (!name.empty() && name.back() == '"')
				name.pop_back();
			material.texture = name;
		} else if (text::equalsNoCase(word, "kd")) {
			float r, g, b;
			if (text::parseFloat(args, r) && text::parseFloat(args, g) && text::parseFloat(args, b)) {
				material.R = channel(r) << 24;
				material.G = channel(g) << 16;
				material.B = channel(b) << 8;
			}
		}
	}
	finish();

	if (texture_filenames.size() > 0)
		has_textures = true;
	if (cnt)
		has_colors = true;
}

void ObjLoader::cacheVertices() {
	vertices.setElementsPerBlock(1<<20);
	file.seek(0);
	const char *line;
	size_t length;

	while(file.readLine(line, length)) {
		if(line[0] != 'v')
			continue;

		if(line[1] == 'n' && text::isSpace(line[2])) {
			n_normals++;
			continue;
		}
		if(!text::isSpace(line[1]))      //skip other properties
			continue;

		vertices.resize(n_vertices+1);
		Vertex &vertex = vertices[n_vertices];
		n_vertices++;

		vcg::Point3d p;
		const char *s = line + 2;
		if(!text::parseDouble(s, p[0]) || !text::parseDouble(s, p[1]) || !text::parseDouble(s, p[2]))
			throw Error("error parsing vertex line " + excerpt(line) + " while caching");
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
	}
}

void ObjLoader::setMaxMemory(uint64_t max_memory) {
	vertices.setMaxMemory(max_memory);
}

//An index of a face: 1 is the first element of the file, -1 the last one defined before the
//face. Returns it zero based.
int64_t ObjLoader::resolveIndex(int64_t index, uint64_t seen, uint64_t total, const char *what, const char *line) {
	int64_t resolved = -1;
	if(index > 0)
		resolved = index - 1;
	else if(index < 0)
		resolved = int64_t(seen) + index;
	if(index == 0 || resolved < 0 || uint64_t(resolved) >= total)
		throw Error(std::string("could not parse face: ") + what + " index " + std::to_string(index) +
		            " does not refer to an element of the file (it defines " + std::to_string(total) +
		            "): " + excerpt(line));
	return resolved;
}

void ObjLoader::parseFace(const char *line) {
	corners.clear();
	const char *p = line + 1; //'f'
	while(true) {
		p = text::skipSpaces(p);
		if(endsToken(*p))
			break;

		//v, v/vt, v/vt/vn or v//vn
		int64_t values[3] = {0, 0, 0};
		bool present[3] = {false, false, false};
		for(int k = 0; k < 3; k++) {
			if(k > 0) {
				if(*p != '/')
					break;
				p++;
			}
			if(endsToken(*p) || *p == '/') {
				if(k == 0)
					throw Error("could not parse face: " + excerpt(line));
				continue; //an empty texture coordinate or normal
			}
			if(!text::parseInteger(p, values[k]))
				throw Error("could not parse face: " + excerpt(line));
			present[k] = true;
		}
		if(!endsToken(*p)) //more than three indices, or something else after them
			throw Error("could not parse face: " + excerpt(line));

		Corner c;
		c.vertex = resolveIndex(values[0], seen_vertices, n_vertices, "vertex", line);
		c.uv = present[1] ? resolveIndex(values[1], seen_uvs, vtxtuv.size() / 2, "texture coordinate", line) : -1;
		c.normal = present[2] ? resolveIndex(values[2], seen_normals, n_normals, "normal", line) : -1;
		corners.push_back(c);
	}
	if(corners.size() < 3)
		throw Error("could not parse face: " + excerpt(line));
}


uint32_t ObjLoader::getTriangles(uint32_t size, Triangle *faces) {

	if (!cached) {
		cacheVertices();
		cacheTextureUV();
		cached = true;
	}

	file.seek(current_tri_pos);

	uint32_t R = (0x7fu << 24);
	uint32_t G = (0x7fu << 16);
	uint32_t B = (0x7fu << 8);

	uint32_t A = 255;
	uint32_t default_color = R + G + B + A;
	
	uint32_t count = 0;
	uint64_t cpos = current_tri_pos;
	const char *line;
	size_t length;

	while (count < size) {
		cpos = file.pos();
		if (!file.readLine(line, length)) {                     //end of file
			cpos = file.pos();
			break;
		}

		if (line[0] == 'v') {
			if (text::isSpace(line[1]))
				seen_vertices++;
			else if (line[1] == 't' && text::isSpace(line[2]))
				seen_uvs++;
			else if (line[1] == 'n' && text::isSpace(line[2]))
				seen_normals++;
			continue;
		}

		if (has_colors && text::startsWithWord(line, "usemtl")) {
			const std::string str = text::afterFirstWord(text::simplified(line, length));
			auto color = colors_map.find(str);
			current_color = color == colors_map.end() ? 0 : color->second;
			if (!current_color)
				current_color = default_color;

			current_texture_id = -1;
			auto texture = textures_map.find(str);
			if (texture != textures_map.end() && texture->second.length() > 0) {
				for (size_t i = 0; i < texture_filenames.size(); i++) {
					if (texture_filenames[i].filename == texture->second)
						current_texture_id = int32_t(i);
				}
			}
			continue;
		}

		if (!text::startsWithWord(line, "f"))
			continue;

		parseFace(line);

		//a face of n corners is cut into n - 2 triangles
		const size_t valence = corners.size();
		if (count + (valence - 2) >= size)
			break;

		for (size_t m = 0; m + 2 < valence; m++) {
			Triangle &current = faces[count];
			const Corner *fan[3] = { &corners[0], &corners[m + 1], &corners[m + 2] };

			for (int k = 0; k < 3; k++) {
				current.vertices[k] = vertices[fan[k]->vertex];
				if (fan[k]->uv >= 0)
					for (int j = 0; j < 2; j++)
						current.vertices[k].t[j] = vtxtuv[fan[k]->uv * 2 + j];
			}
			current.tex = current_texture_id;
			if (has_colors && current_color) {
				for (int k = 0; k < 3; k++) {
					current.vertices[k].c[0] = RED(current_color);
					current.vertices[k].c[1] = GREEN(current_color);
					current.vertices[k].c[2] = BLUE(current_color);
					current.vertices[k].c[3] = ALPHA(current_color);
				}
			}
			current.node = 0;
			if (current.isDegenerate()) {
				continue;
			}
			else {
				count++;
				n_triangles++;
			}
		}
		cpos = file.pos();
	}


	current_tri_pos = cpos;

	return count;
}


/*
 * Seeks all verteces in whole file
 */
uint32_t ObjLoader::getVertices(uint32_t size, Splat *vertices) {
	file.seek(current_vertex_pos);
	const char *line;
	size_t length;

	uint32_t count = 0;
	while(count < size) {
		if(!file.readLine(line, length))                     //end of file
			break;

		if(line[0] != 'v' || !text::isSpace(line[1]))            //skip comments, faces, other properties, etc
			continue;

		Splat &vertex = vertices[count];
		
		vcg::Point3d p;
		const char *s = line + 2;
		if(!text::parseDouble(s, p[0]) || !text::parseDouble(s, p[1]) || !text::parseDouble(s, p[2]))
			throw Error("error parsing vertex line " + excerpt(line));
		
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
	current_vertex_pos = file.pos();
	return count;
}
