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
#include "plyloader.h"
#include <math.h>
#include <algorithm>
#include "../common/error.h"
#include "../common/pathutil.h"
#include "../common/textparse.h"
using namespace vcg;
using namespace vcg::ply;

#include <iostream>
using namespace std;

//Ugly ply stuff from vcg.

//The most vertices (and the most u, v pairs) a face may have. plylib reads a list into f and t
//as long as the file says it is (an ASCII file, or a binary one whose lists have int lengths, can
//say anything), so the descriptors below tell it how much room there is and a longer list fails
//the read.
static const int MAX_FACE_VERTICES = 255;

struct PlyFace {
	uint32_t f[MAX_FACE_VERTICES];
	float t[2*MAX_FACE_VERTICES]; //a u, v pair per vertex
	uint32_t texNumber;
	int n;  //number of vertex indices in the file, read into f when it fits
	int nt; //number of texture coordinates in the file, read into t when it fits
};

struct PlyVertex {
	double dv[3];
	float v[3];
	float t[2]; //texture
	float n[3];
	unsigned char c[4]; //colors
};

//TODO add uv?
PropDescriptor plyprop1[15]= {
	{"vertex", "x",     T_FLOAT, T_FLOAT, offsetof(PlyVertex,v[0]),0,0,0,0,0,0},
	{"vertex", "y",     T_FLOAT, T_FLOAT, offsetof(PlyVertex,v[1]),0,0,0,0,0,0},
	{"vertex", "z",     T_FLOAT, T_FLOAT, offsetof(PlyVertex,v[2]),0,0,0,0,0,0},
	{"vertex", "red"  , T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[0]),0,0,0,0,0,0},
	{"vertex", "green", T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[1]),0,0,0,0,0,0},
	{"vertex", "blue" , T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[2]),0,0,0,0,0,0},
	{"vertex", "alpha", T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[3]),0,0,0,0,0,0},
	{"vertex", "nx",    T_FLOAT, T_FLOAT, offsetof(PlyVertex, n[0]),0,0,0,0,0,0},
	{"vertex", "ny",    T_FLOAT, T_FLOAT, offsetof(PlyVertex, n[1]),0,0,0,0,0,0},
	{"vertex", "nz",    T_FLOAT, T_FLOAT, offsetof(PlyVertex, n[2]),0,0,0,0,0,0},
	{"vertex", "diffuse_red",   T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[0]),0,0,0,0,0,0},
	{"vertex", "diffuse_green", T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[1]),0,0,0,0,0,0},
	{"vertex", "diffuse_blue" , T_UCHAR, T_UCHAR, offsetof(PlyVertex,c[2]),0,0,0,0,0,0},
	{"vertex", "s",    T_FLOAT, T_FLOAT, offsetof(PlyVertex, t[0]),0,0,0,0,0,0},
	{"vertex", "t",    T_FLOAT, T_FLOAT, offsetof(PlyVertex, t[1]),0,0,0,0,0,0}
};
PropDescriptor doublecoords[3] = {
	{"vertex", "x",     T_DOUBLE, T_DOUBLE, offsetof(PlyVertex,dv[0]),0,0,0,0,0,0},
	{"vertex", "y",     T_DOUBLE, T_DOUBLE, offsetof(PlyVertex,dv[1]),0,0,0,0,0,0},
	{"vertex", "z",     T_DOUBLE, T_DOUBLE, offsetof(PlyVertex,dv[2]),0,0,0,0,0,0}
};

PropDescriptor plyprop5[1]=	{
	{"face", "texnumber",T_INT,T_INT,offsetof(PlyFace, texNumber), 0,0,0,0,0,0}
};


//The face lists are read as the types the header declares, the lengths included: most files give
//the length as an uchar, but a list of any integer length type (or of any integer index type) is
//valid PLY. False when the face has no such list, or its items or length are not of the kind
//asked for (memtype says which: T_UINT for vertex indices, T_FLOAT for texture coordinates).
static bool addFaceList(PlyFile &pf, const char *name, int memtype, size_t offset, size_t offsetLength, size_t maxlist) {
	const auto isInteger = [](int type) {
		return type == T_CHAR || type == T_UCHAR || type == T_SHORT || type == T_USHORT || type == T_INT || type == T_UINT;
	};
	for(PlyElement &element: pf.elements) {
		if(element.name != "face")
			continue;
		PlyProperty *property = element.FindProp(name);
		if(!property || !property->islist || !isInteger(property->tipoindex))
			return false;
		const bool integerItems = isInteger(property->tipo);
		const bool realItems = property->tipo == T_FLOAT || property->tipo == T_DOUBLE;
		if(memtype == T_UINT ? !integerItems : !realItems)
			return false;
		return pf.AddToRead("face", name, property->tipo, memtype, offset, 1, 0,
		                    property->tipoindex, T_INT, offsetLength, maxlist) == vcg::ply::E_NOERROR;
	}
	return false;
}

PlyLoader::PlyLoader(const std::string &filename, const std::string &cachePrefix):
	vertices_element(-1),
	faces_element(-1),
	vertices(cachePrefix),
	n_vertices(0),
	n_triangles(0),
	current_triangle(0),
	current_vertex(0),
	face(new PlyFace()) {

	int val = pf.Open(nx::systemPath(filename).c_str(), PlyFile::MODE_READ);
	if(val == -1) {
		int error = pf.GetError();
		throw nx::Error("could not open file " + filename + ". Error: " + std::to_string(error));

	}
	init();
	for(int co = 0; co < int(pf.comments.size()); ++co) {
		std::string TFILE = "TEXTUREFILE";
		std::string &c = pf.comments[co];
		std::string bufstr,bufclean;
		int i,n;

		std::string start = c.substr(0,TFILE.length());
		for_each(start.begin(), start.end(), [](char& in){ in = ::toupper(in); });

		if( TFILE == start )
		{
			bufstr = c.size() > TFILE.length() ? c.substr(TFILE.length()+1) : std::string();
			n = static_cast<int>(bufstr.length());
			for(i=0;i<n;i++)
				if(bufstr[i]!='\t' && bufstr[i]>=32 && bufstr[i]<=126 )	bufclean.push_back(bufstr[i]);

			//a string, not a buffer: the comment and the name of the file are any length
			LoadTexture tex;
			tex.filename = nx::text::trimmed(ply::interpret_texture_name(bufclean, filename));
			texture_filenames.push_back(tex);
		}
	}
	if(has_textures && texture_filenames.size() == 0)
		has_textures = false;

	for(auto &tex: texture_filenames) {
		sanitizeTextureFilepath(tex.filename);
		resolveTextureFilepath(filename, tex.filename);
	}
}

PlyLoader::~PlyLoader() {
	pf.Destroy();
}

void PlyLoader::init() {
	bool has_faces = false;
	for(unsigned int i = 0; i < pf.elements.size(); i++) {

		if(!strcmp(pf.ElemName(i),"vertex")) {
			n_vertices = pf.ElemNumber(i);
			vertices_element = i;

		} else if( !strcmp(pf.ElemName(i),"face") ) {
			n_triangles = pf.ElemNumber(i);
			if (n_triangles) {
				faces_element = i;
				has_faces = true;
			}
		}
	}

	//testing for required vertex fields.
	if(pf.AddToRead(plyprop1[0])==-1 ||
			pf.AddToRead(plyprop1[1])==-1 ||
			pf.AddToRead(plyprop1[2])==-1) {

		if(pf.AddToRead(doublecoords[0])==-1 ||
				pf.AddToRead(doublecoords[1])==-1 ||
				pf.AddToRead(doublecoords[2])==-1) {
			throw nx::Error("ply file is missing xyz coords");
		} else {
			double_coords = true;
		}
	}

	//these calls will silently fail if no color is present
	int error = pf.AddToRead(plyprop1[3]);
	pf.AddToRead(plyprop1[4]);
	pf.AddToRead(plyprop1[5]);
	pf.AddToRead(plyprop1[6]);

	if(error ==  vcg::ply::E_NOERROR)
		has_colors = true;
	else {
		error = pf.AddToRead(plyprop1[10]);
		pf.AddToRead(plyprop1[11]);
		pf.AddToRead(plyprop1[12]);

		if(error ==  vcg::ply::E_NOERROR)
			has_colors = true;
	}

	error = pf.AddToRead(plyprop1[13]);
	pf.AddToRead(plyprop1[14]);

	if(error ==  vcg::ply::E_NOERROR) {
		has_textures = true;
		has_vertex_tex_coords = true;
	}

	//these calls will fail silently if no normal is present
	if(!has_faces) { //skip normals for triangle mesh
		error = pf.AddToRead(plyprop1[7]);
		pf.AddToRead(plyprop1[8]);
		pf.AddToRead(plyprop1[9]);
		if(error == vcg::ply::E_NOERROR)
			has_normals = true;
	}

	for(const char *name: { "vertex_index", "vertex_indices" })
		addFaceList(pf, name, T_UINT, offsetof(PlyFace, f[0]), offsetof(PlyFace, n), MAX_FACE_VERTICES);

	//fails silently if no texture is present
	if (addFaceList(pf, "texcoord", T_FLOAT, offsetof(PlyFace, t[0]), offsetof(PlyFace, nt), 2*MAX_FACE_VERTICES)) {
		has_textures = true;
		has_face_tex_coords = true;
	}

	if (pf.AddToRead(plyprop5[0]) == vcg::ply::E_NOERROR) { }

	pf.SetCurElement(vertices_element);
}

void PlyLoader::readElement(void *element, const char *name, uint64_t index, uint64_t total) {
	if(pf.Read(element) == -1)
		throw nx::Error(std::string("ply file is truncated or malformed: could not read ") + name + " " +
		                std::to_string(index) + " of " + std::to_string(total));
}

void PlyLoader::cacheVertices() {
	vertices.setElementsPerBlock(1<<20);
	vertices.resize(n_vertices);

	PlyVertex vertex;
	//caching vertices on temporary file
	for(uint64_t i = 0; i < n_vertices; i++) {
		Vertex &v = vertices[i];
		readElement(&vertex, "vertex", i, n_vertices);
		if(double_coords) {
			v.v[0] = (float)(vertex.dv[0] - origin[0])*scale[0];
			v.v[1] = (float)(vertex.dv[1] - origin[1])*scale[1];
			v.v[2] = (float)(vertex.dv[2] - origin[2])*scale[2];
		} else {
			v.v[0] = (float)(vertex.v[0] - origin[0])*scale[0];
			v.v[1] = (float)(vertex.v[1] - origin[1])*scale[1];
			v.v[2] = (float)(vertex.v[2] - origin[2])*scale[2];
		}
		if(has_colors) {
			v.c[0] = vertex.c[0];
			v.c[1] = vertex.c[1];
			v.c[2] = vertex.c[2];
			v.c[3] = vertex.c[3];
		}
		if(has_textures) {
			v.t[0] = vertex.t[0];
			v.t[1] = vertex.t[1];
		}

		if(quantization) {
			quantize(v.v[0]);
			quantize(v.v[1]);
			quantize(v.v[2]);
		}
	}

	pf.SetCurElement(faces_element);
}

void PlyLoader::setMaxMemory(uint64_t max_memory) {
	vertices.setMaxMemory(max_memory);
}

//reads the next face into face, checks it and starts its fan
void PlyLoader::readFace() {
	auto which = [&]() {
		return "ply face " + std::to_string(current_triangle) + " of " + std::to_string(n_triangles);
	};
	auto badIndexCount = [&]() {
		return nx::Error(which() + " has " + std::to_string(face->n) + " vertex indices; a face has 3 to " +
		                 std::to_string(MAX_FACE_VERTICES));
	};

	face->n = 0;
	face->nt = 0;
	const bool read = pf.Read(face.get()) != -1;

	//plylib stores the length of a list before it finds the list too long to read, so a face
	//of too many vertices is reported as that rather than as a malformed file. A vertex index
	//list shorter than 3 (or no index list at all) is not a face either.
	if(face->n < 0 || face->n > MAX_FACE_VERTICES || (read && face->n < 3))
		throw badIndexCount();
	if(face->nt < 0 || face->nt > 2*MAX_FACE_VERTICES)
		throw nx::Error(which() + " has " + std::to_string(face->nt) + " texture coordinates; a face has at most " +
		                std::to_string(2*MAX_FACE_VERTICES));
	if(!read)
		throw nx::Error("ply file is truncated or malformed: could not read " + which());

	for(int k = 0; k < face->n; k++)
		if(face->f[k] >= n_vertices)
			throw nx::Error("Bad index in triangle list.");

	//a u, v pair for each vertex, or none: a face without them gets 0, 0 (it used to get the
	//pairs of the face read before it)
	if(has_face_tex_coords && face->nt != 0 && face->nt != 2*face->n)
		throw nx::Error(which() + " has " + std::to_string(face->n) + " vertex indices and " + std::to_string(face->nt) +
		                " texture coordinates; expected none or " + std::to_string(2*face->n));
	if(face->nt == 0)
		std::fill(face->t, face->t + 2*face->n, 0.0f);

	current_triangle++;
	face_corner = 1;
}

uint32_t PlyLoader::getTriangles(uint32_t size, Triangle *buffer) {
	if(faces_element == -1)
		throw nx::Error("ply has no faces!");

	if(current_triangle == 0)
		cacheVertices();

	uint32_t count = 0;

	//a face of n vertices is cut into the n - 2 triangles of a fan around its first vertex,
	//like the OBJ faces; a face whose triangles do not all fit in buffer goes on in the next call
	while(count < size) {
		if(face_corner == 0) {
			if(current_triangle >= n_triangles)
				break;
			readFace();
		}

		const int fan[3] = { 0, face_corner, face_corner + 1 };
		face_corner = face_corner + 2 < face->n ? face_corner + 1 : 0;

		Triangle &current = buffer[count];
		for(int k = 0; k < 3; k++) {
			const int corner = fan[k];
			Vertex &vertex = vertices[face->f[corner]];
			if(!has_vertex_tex_coords) {
				vertex.t[0] = face->t[corner*2];
				vertex.t[1] = face->t[corner*2+1];
			}

			if (has_textures) {
				float n;
				if(vertex.t[0] != 1.0)
					vertex.t[0] = modf(vertex.t[0], &n);
				if(vertex.t[1] != 1.0)
					vertex.t[1] = modf(vertex.t[1], &n);
			}

			current.vertices[k] = vertex;
		}
		//TODO! detect complicated texture coordinates

		current.node = 0;
		current.tex = face->texNumber + texOffset;

		//ignore degenerate triangles
		if(current.isDegenerate())
			continue;

		count++;
	}
	return count;
}

uint32_t PlyLoader::getVertices(uint32_t size, Splat *splats) {
	if(current_triangle > n_triangles) return 0;

	PlyVertex vertex;

	Splat v;
	v.node = 0;
	v.c[3] = 255;
	uint32_t count = 0;
	for(uint32_t i = 0; i < size && current_vertex < n_vertices; i++) {

		readElement(&vertex, "vertex", current_vertex, n_vertices);

		Splat &v = splats[count++];
		current_vertex++;

		vcg::Point3d p;
		if(double_coords) {
			p = vcg::Point3d(vertex.dv);
		} else {
			p = vcg::Point3d(vertex.v[0], vertex.v[1], vertex.v[2]);
		}
		p -= origin;
		p[0] *= scale[0];
		p[1] *= scale[1];
		p[2] *= scale[2];
		box.Add(p);
		v.v[0] = (float)p[0];
		v.v[1] = (float)p[1];
		v.v[2] = (float)p[2];

		if(has_colors) {
			v.c[0] = vertex.c[0];
			v.c[1] = vertex.c[1];
			v.c[2] = vertex.c[2];
			v.c[3] = vertex.c[3];
		}
		if(has_textures) {
			v.t[0] = vertex.t[0];
			v.t[1] = vertex.t[1];
		}
		if(has_normals) {
			v.n[0] = vertex.n[0];
			v.n[1] = vertex.n[1];
			v.n[2] = vertex.n[2];
		}

		if(quantization) {
			quantize(v.v[0]);
			quantize(v.v[1]);
			quantize(v.v[2]);
		}
	}
	return count;
}
