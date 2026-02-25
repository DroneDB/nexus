#ifndef STLLOADER_H
#define STLLOADER_H

#include "meshloader.h"
#include <fstream>
#include <cstdint>

class STLLoader: public MeshLoader {
public:
	STLLoader(std::string filename);

	void setMaxMemory(uint64_t /*max_memory*/) { /* ignore, here no memory needed */ }
	uint32_t getTriangles(uint32_t size, Triangle *buffer) {
		if(ascii)
			return getTrianglesAscii(size, buffer);
		else
			return getTrianglesBinary(size, buffer);
	}

	uint32_t getVertices(uint32_t size, Splat *vertex);

	uint32_t nVertices() { return (uint32_t)n_vertices; }
	uint32_t nTriangles() { return (uint32_t)n_triangles; }
private:
	uint32_t getTrianglesAscii(uint32_t size, Triangle *buffer);
	uint32_t getTrianglesBinary(uint32_t size, Triangle *buffer);

	std::ifstream file;

	bool ascii;
	uint64_t n_vertices;
	uint64_t n_triangles;
	uint64_t current_triangle;
	uint64_t current_vertex;
};

#endif // STLLOADER_H

