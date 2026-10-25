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
#ifndef NX_MAPPEDFILE_H
#define NX_MAPPEDFILE_H

#include <cstdint>
#include <string>
#include <unordered_map>

namespace nx {

//A file whose byte ranges can be mapped into memory. It is either a file that already exists,
//opened read only, or a temporary file: created next to a name the caller picks, read and
//write, growing with resize(), and gone when the object is destroyed (or, on Linux, the
//process is killed). This is the only place of Nexus that talks to the operating system's
//memory mapping.
//
//Ranges need not be aligned: the mapping is made from the enclosing page boundary and the
//pointer returned points at the byte asked for. Whatever is still mapped when the object is
//destroyed is unmapped.
class MappedFile {
public:
	MappedFile() = default;
	~MappedFile();
	MappedFile(const MappedFile &) = delete;
	MappedFile &operator=(const MappedFile &) = delete;

	//throws nx::Error when the file cannot be opened
	void openReadOnly(const std::string &path);
	//creates a new file named after prefix (a ".XXXXXX"-like random suffix is appended); a prefix
	//without a folder lives in the working directory, so callers pass an absolute one.
	//Throws nx::Error when no file can be created.
	void createTemporary(const std::string &prefix);
	bool isOpen() const;

	const std::string &path() const { return path_; }
	uint64_t size() const { return size_; }
	//sets the file size, zero-filling when it grows; throws nx::Error. The file must have
	//no mapped range that the new size would cut off (on Windows: no mapped range at all).
	void resize(uint64_t size);

	//maps length bytes from offset; nullptr (and lastError() set) when the system refuses.
	//The range must lie inside the file.
	unsigned char *map(uint64_t offset, uint64_t length);
	//false when mapped is not the start of a range this object mapped
	bool unmap(const void *mapped);
	//text of the last failure of the system
	const std::string &lastError() const { return error_; }

private:
	struct View {
		void *base;
		uint64_t length;
	};

	void close();
	void remember(const std::string &what);

	std::string path_;
	std::string error_;
	uint64_t size_ = 0;
	bool writable_ = false;
	std::unordered_map<const void *, View> views_;
#ifdef _WIN32
	void *handle_ = nullptr;  //HANDLE
#else
	int fd_ = -1;
#endif
};

} //namespace

#endif // NX_MAPPEDFILE_H
