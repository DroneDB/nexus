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
#include "mmap_file.h"

#include <cstring>
#include <algorithm>
#include <sstream>
#include <random>
#include <filesystem>

namespace fs = std::filesystem;

namespace nx {

// ---------------------------------------------------------------------------
// MappedFile
// ---------------------------------------------------------------------------

MappedFile::MappedFile() = default;

MappedFile::~MappedFile() {
	close();
}

void MappedFile::setFileName(const std::string &path) {
	path_ = path;
}

std::string MappedFile::fileName() const {
	return path_;
}

std::string MappedFile::errorString() const {
	return error_;
}

#ifdef _WIN32
// ========================== Windows implementation ==========================

bool MappedFile::open(int mode) {
	DWORD access = 0;
	DWORD creation = OPEN_EXISTING;
	DWORD share = FILE_SHARE_READ;

	if ((mode & ReadWrite) == ReadWrite) {
		access = GENERIC_READ | GENERIC_WRITE;
		creation = OPEN_ALWAYS;
	} else if (mode & WriteOnly) {
		access = GENERIC_WRITE;
		creation = OPEN_ALWAYS;
	} else {
		access = GENERIC_READ;
	}
	if (mode & Truncate) {
		creation = CREATE_ALWAYS;
	}

	hFile_ = CreateFileA(path_.c_str(), access, share, nullptr, creation, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile_ == INVALID_HANDLE_VALUE) {
		error_ = "CreateFile failed for: " + path_;
		return false;
	}
	opened_ = true;
	openMode_ = mode;
	return true;
}

void MappedFile::close() {
	for (auto &m : maps_) {
		if (m.basePtr) UnmapViewOfFile(m.basePtr);
		if (m.mapHandle) CloseHandle(m.mapHandle);
	}
	maps_.clear();
	if (hFile_ != INVALID_HANDLE_VALUE) {
		CloseHandle(hFile_);
		hFile_ = INVALID_HANDLE_VALUE;
	}
	opened_ = false;
	openMode_ = 0;
}

bool MappedFile::isOpen() const {
	return opened_;
}

int64_t MappedFile::read(char *dest, size_t length) {
	DWORD bytesRead = 0;
	if (!ReadFile(hFile_, dest, static_cast<DWORD>(length), &bytesRead, nullptr)) {
		error_ = "ReadFile failed";
		return -1;
	}
	return static_cast<int64_t>(bytesRead);
}

int64_t MappedFile::write(const char *src, size_t length) {
	DWORD bytesWritten = 0;
	if (!WriteFile(hFile_, src, static_cast<DWORD>(length), &bytesWritten, nullptr)) {
		error_ = "WriteFile failed";
		return -1;
	}
	return static_cast<int64_t>(bytesWritten);
}

bool MappedFile::seek(uint64_t p) {
	LARGE_INTEGER li;
	li.QuadPart = static_cast<LONGLONG>(p);
	if (!SetFilePointerEx(hFile_, li, nullptr, FILE_BEGIN)) {
		error_ = "SetFilePointerEx failed";
		return false;
	}
	return true;
}

uint64_t MappedFile::pos() const {
	LARGE_INTEGER li;
	li.QuadPart = 0;
	LARGE_INTEGER result;
	SetFilePointerEx(hFile_, li, &result, FILE_CURRENT);
	return static_cast<uint64_t>(result.QuadPart);
}

uint64_t MappedFile::size() const {
	LARGE_INTEGER li;
	if (!GetFileSizeEx(hFile_, &li))
		return 0;
	return static_cast<uint64_t>(li.QuadPart);
}

bool MappedFile::resize(uint64_t newSize) {
	LARGE_INTEGER li;
	li.QuadPart = static_cast<LONGLONG>(newSize);
	if (!SetFilePointerEx(hFile_, li, nullptr, FILE_BEGIN))
		return false;
	if (!SetEndOfFile(hFile_))
		return false;
	return true;
}

unsigned char *MappedFile::map(uint64_t offset, uint64_t length) {
	if (length == 0) return nullptr;

	DWORD protect;
	DWORD desiredAccess;
	if (openMode_ & WriteOnly) {
		protect = PAGE_READWRITE;
		desiredAccess = FILE_MAP_ALL_ACCESS;
	} else {
		protect = PAGE_READONLY;
		desiredAccess = FILE_MAP_READ;
	}

	// The mapping must cover offset+length
	uint64_t mapEnd = offset + length;
	HANDLE mapHandle = CreateFileMappingA(hFile_, nullptr, protect,
		static_cast<DWORD>(mapEnd >> 32), static_cast<DWORD>(mapEnd & 0xFFFFFFFF), nullptr);
	if (!mapHandle) {
		error_ = "CreateFileMapping failed";
		return nullptr;
	}

	// Align offset to allocation granularity
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	uint64_t alignedOffset = (offset / si.dwAllocationGranularity) * si.dwAllocationGranularity;
	uint64_t delta = offset - alignedOffset;

	void *basePtr = MapViewOfFile(mapHandle, desiredAccess,
		static_cast<DWORD>(alignedOffset >> 32), static_cast<DWORD>(alignedOffset & 0xFFFFFFFF),
		static_cast<SIZE_T>(length + delta));
	if (!basePtr) {
		CloseHandle(mapHandle);
		error_ = "MapViewOfFile failed";
		return nullptr;
	}

	MapInfo mi;
	mi.mapHandle = mapHandle;
	mi.basePtr = basePtr;
	mi.offset = offset;
	mi.length = length;
	maps_.push_back(mi);

	return static_cast<unsigned char *>(basePtr) + delta;
}

bool MappedFile::unmap(unsigned char *ptr) {
	for (auto it = maps_.begin(); it != maps_.end(); ++it) {
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		uint64_t alignedOffset = (it->offset / si.dwAllocationGranularity) * si.dwAllocationGranularity;
		uint64_t delta = it->offset - alignedOffset;
		unsigned char *expected = static_cast<unsigned char *>(it->basePtr) + delta;
		if (expected == ptr) {
			UnmapViewOfFile(it->basePtr);
			CloseHandle(it->mapHandle);
			maps_.erase(it);
			return true;
		}
	}
	error_ = "unmap: pointer not found";
	return false;
}

#else
// ========================== POSIX implementation ==========================

bool MappedFile::open(int mode) {
	int flags = 0;
	if ((mode & ReadWrite) == ReadWrite)
		flags = O_RDWR | O_CREAT;
	else if (mode & WriteOnly)
		flags = O_WRONLY | O_CREAT;
	else
		flags = O_RDONLY;

	if (mode & Truncate)
		flags |= O_TRUNC;

	fd_ = ::open(path_.c_str(), flags, 0644);
	if (fd_ < 0) {
		error_ = "open() failed for: " + path_;
		return false;
	}
	opened_ = true;
	openMode_ = mode;
	return true;
}

void MappedFile::close() {
	for (auto &m : maps_) {
		if (m.basePtr != MAP_FAILED && m.basePtr != nullptr)
			::munmap(m.basePtr, m.mappedLength);
	}
	maps_.clear();
	if (fd_ >= 0) {
		::close(fd_);
		fd_ = -1;
	}
	opened_ = false;
	openMode_ = 0;
}

bool MappedFile::isOpen() const {
	return opened_;
}

int64_t MappedFile::read(char *dest, size_t length) {
	ssize_t r = ::read(fd_, dest, length);
	if (r < 0) {
		error_ = "read() failed";
		return -1;
	}
	return static_cast<int64_t>(r);
}

int64_t MappedFile::write(const char *src, size_t length) {
	ssize_t w = ::write(fd_, src, length);
	if (w < 0) {
		error_ = "write() failed";
		return -1;
	}
	return static_cast<int64_t>(w);
}

bool MappedFile::seek(uint64_t p) {
	off_t result = ::lseek(fd_, static_cast<off_t>(p), SEEK_SET);
	if (result == (off_t)-1) {
		error_ = "lseek() failed";
		return false;
	}
	return true;
}

uint64_t MappedFile::pos() const {
	off_t result = ::lseek(fd_, 0, SEEK_CUR);
	return (result < 0) ? 0 : static_cast<uint64_t>(result);
}

uint64_t MappedFile::size() const {
	struct stat st;
	if (::fstat(fd_, &st) != 0)
		return 0;
	return static_cast<uint64_t>(st.st_size);
}

bool MappedFile::resize(uint64_t newSize) {
	if (::ftruncate(fd_, static_cast<off_t>(newSize)) != 0) {
		error_ = "ftruncate() failed";
		return false;
	}
	return true;
}

unsigned char *MappedFile::map(uint64_t offset, uint64_t length) {
	if (length == 0) return nullptr;

	long pageSize = sysconf(_SC_PAGESIZE);
	uint64_t alignedOffset = (offset / pageSize) * pageSize;
	uint64_t delta = offset - alignedOffset;
	uint64_t mappedLength = length + delta;

	int prot = (openMode_ & WriteOnly) ? (PROT_READ | PROT_WRITE) : PROT_READ;
	void *basePtr = ::mmap(nullptr, mappedLength, prot, MAP_SHARED, fd_, static_cast<off_t>(alignedOffset));
	if (basePtr == MAP_FAILED) {
		error_ = "mmap() failed";
		return nullptr;
	}

	MapInfo mi;
	mi.basePtr = basePtr;
	mi.alignedOffset = alignedOffset;
	mi.mappedLength = mappedLength;
	mi.userOffset = offset;
	mi.userLength = length;
	maps_.push_back(mi);

	return static_cast<unsigned char *>(basePtr) + delta;
}

bool MappedFile::unmap(unsigned char *ptr) {
	for (auto it = maps_.begin(); it != maps_.end(); ++it) {
		uint64_t delta = it->userOffset - it->alignedOffset;
		unsigned char *expected = static_cast<unsigned char *>(it->basePtr) + delta;
		if (expected == ptr) {
			::munmap(it->basePtr, it->mappedLength);
			maps_.erase(it);
			return true;
		}
	}
	error_ = "unmap: pointer not found";
	return false;
}

#endif // _WIN32

// ---------------------------------------------------------------------------
// TempMappedFile
// ---------------------------------------------------------------------------

TempMappedFile::TempMappedFile(const std::string &prefix) {
	// Generate a unique temp filename
	auto tmpDir = fs::temp_directory_path();

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<uint64_t> dist;

	for (int attempt = 0; attempt < 100; ++attempt) {
		std::ostringstream oss;
		oss << prefix << "_" << dist(gen);
		tempPath_ = tmpDir / oss.str();
		if (!fs::exists(tempPath_))
			break;
	}

	setFileName(tempPath_.string());
	if (!open(MappedFile::ReadWrite | MappedFile::Truncate)) {
		throw std::runtime_error("TempMappedFile: could not create temp file: " + tempPath_.string());
	}
}

TempMappedFile::~TempMappedFile() {
	close();
	std::error_code ec;
	fs::remove(tempPath_, ec);
}

} // namespace nx
