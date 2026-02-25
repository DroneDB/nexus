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
#ifndef NX_VIRTUALARRAY_H
#define NX_VIRTUALARRAY_H

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <iostream>
#include <memory>

#include "mmap_file.h"

/*
  blocks are 64 bit memory aligned!
*/

class VirtualMemory {
public:
	VirtualMemory(const std::string &prefix);
	virtual ~VirtualMemory();

	uint64_t memoryUsed() { return used_memory; }
	uint64_t maxMemory() { return max_memory; }
	void setMaxMemory(uint64_t max_memory);

	//careful: if unload is false, memory is valid until another call to a function of this class
	unsigned char *getBlock(uint64_t block, bool unload = false);
	void dropBlock(uint64_t block);
	uint64_t addBlock(uint64_t length);         //return index of added block

	uint64_t nBlocks() { return cache.size(); }
	void resize(uint64_t size, uint64_t n_blocks);
	void flush();

protected:

	virtual uint64_t blockOffset(uint64_t block) = 0;
	virtual uint64_t blockSize(uint64_t block) = 0;

	unsigned char *mapBlock(uint64_t block);
	void unmapBlock(uint64_t block);
	void makeRoom();

	std::unique_ptr<nx::TempMappedFile> tempFile_;

private:
	uint64_t used_memory;
	uint64_t max_memory;
	std::vector<unsigned char *> cache;   //1 pointer per block
	std::deque<uint64_t> mapped;
};


template<class ITEM> class VirtualArray: public VirtualMemory {
public:
	VirtualArray(const std::string &prefix): VirtualMemory(prefix), n_elements(0), elements_per_block(1<<16) {
		block_size = elements_per_block * sizeof(ITEM);
	}
	~VirtualArray() { flush(); }

	void setElementsPerBlock(uint64_t n) {
		elements_per_block = n;
		block_size = elements_per_block * sizeof(ITEM);
	}
	uint64_t size() {
		return n_elements;
	}
	void resize(uint64_t s) {   //number of elements, not memory
		//need to round up s
		uint64_t blocks = (s + elements_per_block -1) / elements_per_block;
		n_elements = s;
		if(blocks != VirtualMemory::nBlocks())
			VirtualMemory::resize(blocks*block_size, blocks);
	}
	ITEM &operator[](uint64_t n) {
		//find block
		uint64_t block = n / elements_per_block;
		uint64_t offset = n - block * elements_per_block;
		unsigned char *buffer = getBlock(block);
		return *(ITEM *)(buffer + offset * sizeof(ITEM));
	}

protected:
	uint64_t n_elements;           //number of elements
	uint64_t elements_per_block;   //default 1<<16
	uint64_t block_size;           //in bytes

	uint64_t blockOffset(uint64_t block) { return block * block_size; }
	uint64_t blockSize(uint64_t /*block*/) { return block_size; }
};

class VirtualChunks: public VirtualMemory {
public:
	VirtualChunks(const std::string &prefix): VirtualMemory(prefix), padding(64) {
		offsets.push_back(0);
	}
	~VirtualChunks() { flush(); }
	void setPadding(uint32_t p) { padding = p; }

	uint64_t addChunk(uint64_t size) {
		//pad size:
		size = pad(size);
		offsets.push_back(offsets.back() + size);
		addBlock(size);
		return offsets.size() -2;
	}
	unsigned char *getChunk(uint64_t chunk, bool unload = false) { return getBlock(chunk, unload); }
	void dropChunk(uint64_t chunk) { unmapBlock(chunk); }

	uint64_t chunkSize(uint64_t chunk) { return blockSize(chunk); }
protected:
	uint32_t padding; //must be a power of 2
	std::vector<uint64_t> offsets;

	uint64_t blockOffset(uint64_t block) { return offsets[block]; }
	uint64_t blockSize(uint64_t block) { return offsets[block+1] - offsets[block]; }

	uint64_t pad(uint64_t size) {
		assert(size != 0);
		uint64_t m = (size-1) & ~(padding -1);
		return m + padding;
	}
};

#endif // NX_VIRTUALARRAY_H
