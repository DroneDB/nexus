#include "stlloader.h"

#include <iostream>
#include <cstring>
#include <vector>
#include <stdexcept>

using namespace std;

STLLoader::STLLoader(std::string filename) {

	has_colors = has_normals = has_textures = false;

	file.open(filename, ios::binary);
	if (!file.is_open())
		throw std::runtime_error("could not open file " + filename);

	char header[6] = {};
	file.read(header, 5);
	if (file.gcount() != 5)
		throw std::runtime_error("Unexpected end of file");
	ascii = (strncmp(header, "solid", 5) == 0);
	if (ascii) {
		// skip rest of first line
		std::string dummy;
		std::getline(file, dummy);
		n_triangles = 0;
		return;
	}
	// binary: read 80-byte header (we already read 5), then triangle count
	file.seekg(80, ios::beg);
	uint32_t nt = 0;
	file.read((char *)&nt, 4);
	n_triangles = nt;
}

uint32_t STLLoader::getTrianglesAscii(uint32_t size, Triangle *buffer) {
	uint32_t readed = 0;
	char line[1024];
	char dummy[1024];

	for (uint32_t i = 0; i < size; i++) {
		if (!file.getline(line, 1023)) return readed; // facet
		if (!file.getline(line, 1023)) return readed; // outer loop

		Triangle &tri = buffer[i];
		tri.node = 0;

		for (int k = 0; k < 3; k++) {
			float *v = tri.vertices[k].v;
			if (!file.getline(line, 1023)) return readed;

			vcg::Point3d d;
			int n = sscanf(line, "%s %lf %lf %lf", dummy, &d[0], &d[1], &d[2]);
			if (n != 4)
				throw std::runtime_error("Invalid STL file");
			d -= origin;
			d[0] *= scale[0];
			d[1] *= scale[1];
			d[2] *= scale[2];
			box.Add(d);

			v[0] = (float)(d[0]);
			v[1] = (float)(d[1]);
			v[2] = (float)(d[2]);
		}
		current_triangle++;
		readed++;

		if (!file.getline(line, 1023)) return readed; // endloop
		if (!file.getline(line, 1023)) return readed; // endfacet
	}
	return readed;
}

uint32_t STLLoader::getTrianglesBinary(uint32_t size, Triangle *buffer) {
	// each face is normal (float) + 3 vertices(float) + uint16 = 50 bytes.
	vector<char> tmp(50 * size);
	file.read(tmp.data(), 50 * size);
	int64_t nread = file.gcount() / 50;

	if (nread <= 0) return 0;

	char *start = tmp.data();
	for (int64_t i = 0; i < nread; i++) {
		float *pos = (float *)(start + 12); // skip normal
		Triangle &tri = buffer[i];
		for (int t = 0; t < 3; t++)
			for (int k = 0; k < 3; k++)
				tri.vertices[t].v[k] = (pos[t * 3 + k] - (float)origin[k]) * (float)scale[k];
		tri.node = 0;
		current_triangle++;
		start += 50;
	}
	return (uint32_t)nread;
}

uint32_t STLLoader::getVertices(uint32_t /*size*/, Splat * /*vertex*/) {
	throw std::runtime_error("STL getVertices: Unimplemented!");
}
