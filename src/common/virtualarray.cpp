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
#include <assert.h>
#include "virtualarray.h"
#include "error.h"
#include "pathutil.h"

using namespace std;

// Build the temporary file's name from a caller-supplied prefix.
// If 'prefix' is an absolute path, it is used as-is so callers can place the
// out-of-core cache in a directory of their choosing (e.g. a per-build temp
// folder that is cleaned up as a unit, on the same large volume as the output).
// Otherwise the prefix is created inside the system temporary directory,
// preserving the historical behaviour.
static std::string virtualMemoryPrefix(const std::string &prefix) {
	return nx::isAbsolutePath(prefix) ? prefix : nx::joinPath(nx::temporaryDirectory(), prefix);
}

VirtualMemory::VirtualMemory(const std::string &prefix):
	used_memory(0),
	max_memory(1<<28) {

	file.createTemporary(virtualMemoryPrefix(prefix));
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
			throw nx::Error("virtual memory error mapping block: " + file.lastError());

		mapped.push_front(index);
	}
	return cache[index];
}

void VirtualMemory::dropBlock(uint64_t index) {
	unmapBlock(index);
}

void VirtualMemory::resize(uint64_t n, uint64_t n_blocks) {
#ifndef _WIN32
	if(n < file.size())
		flush();
#else
	//a file cannot change size under the views of it
	flush();
#endif
	cache.resize(n_blocks, NULL);
	file.resize(n);
}

uint64_t VirtualMemory::addBlock(uint64_t length) {
#ifdef _WIN32
	//a file cannot change size under the views of it
	flush();
#endif
	file.resize(file.size() + length);
	cache.push_back(NULL);
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
	assert(offset + length <= file.size());
	cache[block] = file.map(offset, length);
	if(cache[block])
		used_memory += length;
	return cache[block];
}

void VirtualMemory::unmapBlock(uint64_t block) {
	assert(block < cache.size());
	assert(cache[block]);
	file.unmap(cache[block]);
	cache[block] = NULL;
	used_memory -= blockSize(block);
}
