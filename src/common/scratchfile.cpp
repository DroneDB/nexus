/*
Nexus

Copyright (C) 2026 Digipa

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
#include "scratchfile.h"

#include <algorithm>
#include <cstring>

#include "error.h"

namespace nx {

namespace {
//the file grows by at least this much at a time, so appending many small pieces does not
//resize it for each of them
const uint64_t GrowthStep = uint64_t(64) << 20;
}

void ScratchFile::create(const std::string &prefix) {
	file_.createTemporary(prefix);
	size_ = 0;
}

void ScratchFile::reserve(uint64_t size) {
	if(!file_.isOpen())
		throw Error("the scratch file is not open");
	if(size > file_.size())
		file_.resize(std::max(size, file_.size() + GrowthStep));
}

uint64_t ScratchFile::append(const void *data, size_t length) {
	const uint64_t offset = size_;
	if(length == 0)
		return offset;
	reserve(offset + length);
	unsigned char *where = file_.map(offset, length);
	if(!where)
		throw Error("could not write the scratch file: " + file_.lastError());
	std::memcpy(where, data, length);
	file_.unmap(where);
	size_ += length;
	return offset;
}

void ScratchFile::padTo(uint64_t size) {
	if(size < size_)
		throw Error("the scratch file cannot be shortened");
	//what is not written yet reads as zeros
	reserve(size);
	size_ = size;
}

void ScratchFile::read(uint64_t offset, void *where, size_t length) {
	if(length == 0)
		return;
	if(offset + length > size_)
		throw Error("reading past the end of the scratch file");
	const unsigned char *mapped = file_.map(offset, length);
	if(!mapped)
		throw Error("could not read the scratch file: " + file_.lastError());
	std::memcpy(where, mapped, length);
	file_.unmap(mapped);
}

} //namespace
