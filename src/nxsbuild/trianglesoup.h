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
#ifndef NX_TRIANGLESOUP_H
#define NX_TRIANGLESOUP_H

#include <cstdint>
#include <vector>
#include <cassert>
#include <vcg/space/point3.h>
#include "../common/signature.h"
#include "../common/virtualarray.h"

//16  bytes
struct Vertex {
	float v[3];
	unsigned char c[4]; //colors
	float t[2]; //texture

	bool operator==(const Vertex &p) const {
		return v[0] == p.v[0] && v[1] == p.v[1] && v[2] == p.v[2];
	}
};

struct Splat: public Vertex {
	uint32_t node;
	float n[3];
};

//52 bytes.
struct Triangle {
	Vertex vertices[3];
	uint32_t node;
	int tex;  //which tex this triangle refers to.
	bool isDegenerate() const {
		if(vertices[0] == vertices[1] || vertices[0] == vertices[2] || vertices[1] == vertices[2])
			return true;
		return false;
	}
};

template <class T> class Bin {
public:
	T *elements;
	uint32_t *_size;
	uint32_t capacity;

	Bin(): elements(0), _size(0), capacity(0) {}
	Bin(T *start, uint32_t *s, uint32_t c):
		elements(start), _size(s), capacity(c) {}

	uint32_t size() { if(!_size) return 0; return *_size; }
	void resize(uint32_t s) { *_size = s; }
	bool isFull() { return *_size == capacity; }
	T &operator[](uint32_t n) {
		assert(n < *_size);
		return elements[n];
	}

	void push_back(T &element) {
		assert(!isFull());
		elements[*_size] = element;
		(*_size)++;
	}
};

typedef Bin<Triangle> Soup;
typedef Bin<Splat> Cloud;

template <class T> class VirtualBin: protected VirtualMemory {
public:

	//Triangle soup is guaranteed valid only until another call of getSoup (or resize, or clear)
	//unless prevent_unload is true

	VirtualBin(const std::string &prefix):
		VirtualMemory(prefix),
		triangles_per_block(1<<15),
		block_size((1<<15) * sizeof(Triangle)) {
	}
	~VirtualBin() { flush(); }

	uint64_t memoryUsed() { return VirtualMemory::memoryUsed(); }
	void setMaxMemory(uint64_t m) { VirtualMemory::setMaxMemory(m); }
	uint64_t maxMemory() { return VirtualMemory::maxMemory(); }

	Bin<T> get(uint64_t n, bool prevent_unload = false) {
		unsigned char *memory = getBlock(n, prevent_unload);
		return Bin<T>((T *)memory, &occupancy[n], triangles_per_block);
	}

	void drop(uint64_t n) {
		unmapBlock(n);
	}

	uint64_t size() {
		uint64_t tot = 0;
		for(uint32_t i = 0; i < occupancy.size(); i++)
			tot += occupancy[i];
		return tot;
	}

	void clear() {
		resize(0, 0);
		occupancy.clear();
	}

	void setTrianglesPerBlock(uint64_t s) {
		triangles_per_block = s;
		block_size = triangles_per_block * sizeof(T);
	}

	uint64_t addBlock() {
		assert(occupancy.size() == nBlocks());
		uint64_t block = VirtualMemory::addBlock(block_size);
		occupancy.push_back(0);
		return block;
	}
	uint64_t nBlocks() { return VirtualMemory::nBlocks(); }

	bool isBlockFull(uint64_t block) {
		return occupancy[block] == triangles_per_block;
	}

	uint32_t blockUsed(uint64_t block) {
		return occupancy[block];
	}

protected:
	uint64_t triangles_per_block;
	uint64_t block_size;
	std::vector<uint32_t> occupancy;      //how many triangles per block

	virtual uint64_t blockOffset(uint64_t block) { return block * block_size; }
	virtual uint64_t blockSize(uint64_t /*block*/) { return block_size; }
};

typedef VirtualBin<Triangle> VirtualTriangleSoup;
typedef VirtualBin<Splat> VirtualVertexCloud;


#endif // NX_TRIANGLESOUP_H
