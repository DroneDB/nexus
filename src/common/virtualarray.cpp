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
#include <cassert>
#include <stdexcept>
#include <iostream>
#include <filesystem>

#include "virtualarray.h"

using namespace std;

VirtualMemory::VirtualMemory(const std::string &prefix):
	tempFile_(std::make_unique<nx::TempMappedFile>(prefix)),
	used_memory(0),
	max_memory(1<<28) {
	// TempMappedFile constructor already creates & opens the file
}

VirtualMemory::~VirtualMemory() {
	flush();
}

void VirtualMemory::setMaxMemory(uint64_t n) {
	max_memory = n;
}

unsigned char *VirtualMemory::getBlock(uint64_t index, bool prevent_unload) {

	assert(index < cache.size());
	if(cache[index] == NULL) { //not mapped.
		if(!prevent_unload)
			makeRoom();
		mapBlock(index);
		if(!cache[index])
			throw std::runtime_error("virtual memory error mapping block: " + tempFile_->errorString());

		mapped.push_front(index);
	}
	return cache[index];
}

void VirtualMemory::dropBlock(uint64_t index) {
	unmapBlock(index);
}

void VirtualMemory::resize(uint64_t n, uint64_t n_blocks) {
#ifndef _WIN32
	if(n < tempFile_->size())
		flush();
#else
	flush();
#endif
	cache.resize(n_blocks, NULL);
	tempFile_->resize(n);
}

uint64_t VirtualMemory::addBlock(uint64_t length) {
#ifdef _WIN32
	flush();
#endif
	cache.push_back(NULL);
	tempFile_->resize(tempFile_->size() + length);
	return cache.size()-1;
}

void VirtualMemory::flush() {
	for(uint32_t i = 0; i < cache.size(); i++) {
		if(cache[i])
			unmapBlock(i);
	}
	mapped.clear();
	used_memory = 0;
}

void VirtualMemory::makeRoom() {
	while(used_memory > max_memory) {
		assert(mapped.size());
		uint64_t block = mapped.back();
		if(cache[block])
			unmapBlock(block);
		mapped.pop_back();
	}
}

unsigned char *VirtualMemory::mapBlock(uint64_t block) {
	uint64_t offset = blockOffset(block);
	uint64_t length = blockSize(block);
	assert(offset + length <= tempFile_->size());
	cache[block] = tempFile_->map(offset, length);
	used_memory += length;
	return cache[block];
}

void VirtualMemory::unmapBlock(uint64_t block) {
	assert(block < cache.size());
	assert(cache[block]);
	tempFile_->unmap(cache[block]);
	cache[block] = NULL;
	used_memory -= blockSize(block);
}


