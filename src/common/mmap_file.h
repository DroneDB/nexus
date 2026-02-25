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
#ifndef NX_MMAP_FILE_H
#define NX_MMAP_FILE_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <filesystem>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <unistd.h>
#endif

namespace nx {

/// Cross-platform file with memory-mapping support.
/// Replaces QFile with map/unmap and QTemporaryFile.
class MappedFile {
public:
	MappedFile();
	~MappedFile();

	MappedFile(const MappedFile &) = delete;
	MappedFile &operator=(const MappedFile &) = delete;

	/// Set the file path (does not open it).
	void setFileName(const std::string &path);
	std::string fileName() const;

	/// Open mode flags.
	enum OpenMode {
		ReadOnly  = 1,
		WriteOnly = 2,
		ReadWrite = ReadOnly | WriteOnly,
		Truncate  = 4,
	};

	bool open(int mode);
	void close();
	bool isOpen() const;

	/// Read / write at current position.
	int64_t read(char *dest, size_t length);
	int64_t write(const char *src, size_t length);

	/// Seek to absolute position.
	bool seek(uint64_t pos);
	uint64_t pos() const;

	/// File size.
	uint64_t size() const;

	/// Resize the file.
	bool resize(uint64_t newSize);

	/// Memory-map a region. Returns nullptr on failure.
	unsigned char *map(uint64_t offset, uint64_t length);

	/// Unmap a previously mapped region.
	bool unmap(unsigned char *ptr);

	/// Error description.
	std::string errorString() const;

private:
#ifdef _WIN32
	HANDLE hFile_ = INVALID_HANDLE_VALUE;
	// We keep track of each mapping -> (mapHandle, basePtr, viewOffset, viewLen)
	struct MapInfo {
		HANDLE mapHandle;
		void  *basePtr;
		uint64_t offset;
		uint64_t length;
	};
	std::vector<MapInfo> maps_;
#else
	int fd_ = -1;
	struct MapInfo {
		void    *basePtr;
		uint64_t alignedOffset;
		uint64_t mappedLength; // includes alignment padding
		uint64_t userOffset;
		uint64_t userLength;
	};
	std::vector<MapInfo> maps_;
#endif
	std::string path_;
	std::string error_;
	bool opened_ = false;
	int openMode_ = 0;
};


/// A temporary file with memory-mapping support.
/// Automatically deleted on destruction. Replaces QTemporaryFile.
class TempMappedFile : public MappedFile {
public:
	/// Creates and opens a temp file in the system temp directory.
	/// @param prefix  Filename prefix (e.g. "cache_chunks").
	explicit TempMappedFile(const std::string &prefix = "nx_tmp");
	~TempMappedFile();

	TempMappedFile(const TempMappedFile &) = delete;
	TempMappedFile &operator=(const TempMappedFile &) = delete;

private:
	std::filesystem::path tempPath_;
};

} // namespace nx

#endif // NX_MMAP_FILE_H
