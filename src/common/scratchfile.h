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
#ifndef NX_SCRATCHFILE_H
#define NX_SCRATCHFILE_H

#include <cstddef>
#include <cstdint>
#include <string>

#include "mappedfile.h"

namespace nx {

//Scratch space on disk for data that is written once, at the end, and read back later: a
//temporary file (see MappedFile::createTemporary) that only grows. Nothing stays mapped
//between two calls, so the file can be extended at any time. Every failure throws nx::Error.
class ScratchFile {
public:
	//creates the file next to prefix; an absolute prefix is used as it is
	void create(const std::string &prefix);
	bool isOpen() const { return file_.isOpen(); }

	//number of bytes written so far
	uint64_t size() const { return size_; }
	//writes data at the end and returns the offset it was written at
	uint64_t append(const void *data, size_t length);
	//extends the content with zeros up to size, which must not be smaller than size()
	void padTo(uint64_t size);
	//copies length bytes from offset, which must lie inside the content
	void read(uint64_t offset, void *where, size_t length);

private:
	void reserve(uint64_t size);

	MappedFile file_;
	uint64_t size_ = 0;
};

} //namespace

#endif // NX_SCRATCHFILE_H
