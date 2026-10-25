#include "stlloader.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../common/error.h"
#include "../common/textparse.h"

#include <iostream>
using namespace std;

STLLoader::STLLoader(const std::string &filename) {

	has_colors = has_normals = has_textures = false;

	if(!file.open(filename))
		throw nx::Error("could not open file " + filename);

	char header[80];
	if(file.read(header, 5) != 5)
		throw nx::Error("Unexpected end of file");
	file.seek(0);
	ascii = std::memcmp(header, "solid", 5) == 0;
	if(ascii) {
		const char *line;
		size_t length;
		file.readLine(line, length);
		n_triangles = 0;
		return;
	}
	file.read(header, 80); //ignored
	uint32_t count = 0;
	file.read(&count, 4); //TODO support little endian in big-endiam systems
	n_triangles = count;

}


/*
facet normal nx ny nz
	outer loop
		vertex v1x v1y v1z
		vertex v2x v2y v2z
		vertex v3x v3y v3z
	endloop
endfacet */

uint32_t STLLoader::getTrianglesAscii(uint32_t size, Triangle *buffer) {
	uint32_t readed = 0;
	const char *line;
	size_t length;
	char dummy[1024];
	
	for(uint32_t i = 0; i < size; i++) {

		bool ok = file.readLine(line, length); //facet
		if(!ok) return readed;

		ok = file.readLine(line, length); //normal
		if(!ok) return readed;


		Triangle &tri = buffer[i];
		tri.node = 0;

		for(int k = 0; k < 3; k++) {
			float *v = tri.vertices[k].v;
			ok = file.readLine(line, length); //normal
			if(!ok) return readed;

			vcg::Point3d d;
			int n = sscanf(line, "%1023s %lf %lf %lf", dummy, &d[0], &d[1], &d[2]);
			if(n != 4)
				throw nx::Error("Invalid STL file");
			d -= origin;
			d[0] *= scale[0];
			d[1] *= scale[1];
			d[0] *= scale[2];
			box.Add(d);
					
			v[0] = (float)(d[0]);
			v[1] = (float)(d[1]);
			v[2] = (float)(d[2]);
		}
		current_triangle++;
		readed++;

		ok = file.readLine(line, length); //endloop
		if(!ok) return readed;

		ok = file.readLine(line, length); //endfacet
		if(!ok) return readed;
	}
	return readed;
}

uint32_t STLLoader::getTrianglesBinary(uint32_t size, Triangle *buffer) {
	//each face is normal (float), + 3 vertices(float) + uint16  = 50 bytes.
	vector<char> tmp(50*size);
	int64_t nread = file.read(tmp.data(), 50*size);

	nread /= 50;
	if(nread <= 0) return 0;

	//todo check the file is not truncated!


	char *start = tmp.data();
	for(int i = 0; i < nread; i++) {
		float *pos = (float *)(start + 12); //skip normal
		Triangle &tri = buffer[i];
		for(int t = 0; t < 3; t++)
			for(int k = 0; k < 3; k++)
				tri.vertices[t].v[k] = (pos[t*3 + k] - origin[k])*scale[k];
		tri.node = 0;
		current_triangle++;
		start += 50;
	}
	return nread;
}

uint32_t STLLoader::getVertices(uint32_t /*size*/, Splat */*vertex*/) {
	throw nx::Error("Unimplemented!");
}
