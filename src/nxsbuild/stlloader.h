#ifndef STLLOADER_H
#define STLLOADER_H

#include "meshloader.h"
#include "../common/textfile.h"

class STLLoader: public MeshLoader {
public:
	STLLoader(const std::string &filename);

	void setMaxMemory(uint64_t /*max_memory*/) { /* ignore, here no memory needed */ }
	uint32_t getTriangles(uint32_t size, Triangle *buffer) {
		if(ascii)
			return getTrianglesAscii(size, buffer);
		else
			return getTrianglesBinary(size, buffer);
	}

	uint32_t getVertices(uint32_t size, Splat *vertex);

	uint32_t nVertices() { return n_vertices; }
	uint32_t nTriangles() { return n_triangles; }
private:
	uint32_t getTrianglesAscii(uint32_t size, Triangle *buffer);
	uint32_t getTrianglesBinary(uint32_t size, Triangle *buffer);

	nx::InputFile file;

	bool ascii;
	uint64_t n_vertices = 0;
	uint64_t n_triangles = 0;
	uint64_t current_triangle = 0;
	uint64_t current_vertex = 0;
};

#endif // STLLOADER_H

