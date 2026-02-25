#ifndef TSLOADER_H
#define TSLOADER_H

#include "meshloader.h"
#include "colormap.h"
#include "../common/virtualarray.h"

#include <fstream>
#include <string>
#include <cstdint>

class TsLoader: public MeshLoader {
public:
	TsLoader(std::string file);
	~TsLoader();

	bool useColormapFor(const std::string &_property, const std::string &palette);
	void setMaxMemory(uint64_t max_memory);
	uint32_t getTriangles(uint32_t size, Triangle *buffer);
	uint32_t getVertices(uint32_t size, Splat *vertex);

private:

	void cacheVertices();

	std::ifstream file;
	VirtualArray<Vertex> vertices;
	uint64_t n_vertices;
	uint64_t n_triangles;
	uint64_t current_triangle;
	uint64_t current_vertex;
	int64_t  current_tri_pos = 0;
	std::string property;
	int property_position = -1;
	Colormap colormap;
};
#endif // TSLOADER_H
