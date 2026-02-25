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
#ifndef NX_TSPLOADER_H
#define NX_TSPLOADER_H

#include "meshloader.h"
#include <fstream>
#include <cstdint>

class TspLoader: public MeshLoader {
public:
	TspLoader(std::string file);
	void setMaxMemory(uint64_t /*m*/) {  }
	uint32_t getTriangles(uint32_t size, Triangle *buffer);
	uint32_t getVertices(uint32_t /*size*/, Splat */*vertex*/) { assert(0); return 0; }

private:
	std::ifstream file;
	uint64_t current_triangle;
};

#endif // NX_TSPLOADER_H
