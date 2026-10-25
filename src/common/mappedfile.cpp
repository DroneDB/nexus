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
#include "mappedfile.h"

#include <random>
#include <system_error>
#include <vector>

#include "error.h"
#include "pathutil.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace nx {

namespace {

//the system's text for its last failed call
std::string systemError() {
#ifdef _WIN32
	return std::system_category().message(int(GetLastError()));
#else
	return std::generic_category().message(errno);
#endif
}

//a mapping starts on a multiple of this
uint64_t mapGranularity() {
#ifdef _WIN32
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwAllocationGranularity;
#else
	return uint64_t(sysconf(_SC_PAGESIZE));
#endif
}

std::string randomSuffix() {
	static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	std::random_device random;
	std::string suffix(6, 'x');
	for(char &c: suffix)
		c = letters[random() % (sizeof(letters) - 1)];
	return suffix;
}

} //namespace

MappedFile::~MappedFile() {
	close();
}

bool MappedFile::isOpen() const {
#ifdef _WIN32
	return handle_ != nullptr;
#else
	return fd_ != -1;
#endif
}

void MappedFile::remember(const std::string &what) {
	error_ = what + ": " + systemError();
}

void MappedFile::close() {
	for(const auto &view: views_) {
#ifdef _WIN32
		UnmapViewOfFile(view.second.base);
#else
		munmap(view.second.base, view.second.length);
#endif
	}
	views_.clear();
#ifdef _WIN32
	if(handle_) {
		CloseHandle(handle_);  //a temporary file is deleted by this
		handle_ = nullptr;
	}
#else
	if(fd_ != -1) {
		::close(fd_);
		fd_ = -1;
	}
#endif
	size_ = 0;
}

void MappedFile::openReadOnly(const std::string &path) {
	close();
	path_ = path;
	writable_ = false;
	const auto fsPath = nativePath(path);
	const auto native = fsPath.c_str();
#ifdef _WIN32
	HANDLE handle = CreateFileW(native, GENERIC_READ,
	                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
	                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if(handle == INVALID_HANDLE_VALUE)
		throw Error("could not open file " + path + ": " + systemError());
	LARGE_INTEGER size;
	if(!GetFileSizeEx(handle, &size)) {
		const std::string reason = systemError();
		CloseHandle(handle);
		throw Error("could not get the size of file " + path + ": " + reason);
	}
	handle_ = handle;
	size_ = uint64_t(size.QuadPart);
#else
	const int fd = ::open(native, O_RDONLY | O_CLOEXEC);
	if(fd == -1)
		throw Error("could not open file " + path + ": " + systemError());
	struct stat info;
	if(fstat(fd, &info) != 0) {
		const std::string reason = systemError();
		::close(fd);
		throw Error("could not open file " + path + ": " + reason);
	}
	if(!S_ISREG(info.st_mode)) {
		::close(fd);
		throw Error("could not open file " + path + ": not a regular file");
	}
	fd_ = fd;
	size_ = uint64_t(info.st_size);
#endif
}

void MappedFile::createTemporary(const std::string &prefix) {
	close();
	writable_ = true;
	//a name taken between the choosing and the creating is not an error: another one is picked
	for(int attempt = 0; attempt < 100; attempt++) {
		const std::string path = prefix + "." + randomSuffix();
		const auto fsPath = nativePath(path);
		const auto native = fsPath.c_str();
#ifdef _WIN32
		//deleted by the system when the last handle closes, even if the process is killed
		HANDLE handle = CreateFileW(native, GENERIC_READ | GENERIC_WRITE | DELETE,
		                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
		                            CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
		                            nullptr);
		if(handle != INVALID_HANDLE_VALUE) {
			handle_ = handle;
			path_ = path;
			return;
		}
		const DWORD code = GetLastError();
		if(code != ERROR_FILE_EXISTS && code != ERROR_ALREADY_EXISTS)
			throw Error("unable to create temporary file " + path + ": " + systemError());
#else
		const int fd = ::open(native, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
		if(fd != -1) {
			//the name is not needed any more: unlinked now, the file goes away with the last
			//descriptor even if the process is killed
			unlink(native);
			fd_ = fd;
			path_ = path;
			return;
		}
		if(errno != EEXIST)
			throw Error("unable to create temporary file " + path + ": " + systemError());
#endif
	}
	throw Error("unable to create a temporary file named " + prefix + ".*");
}

void MappedFile::resize(uint64_t size) {
	if(!writable_ || !isOpen())
		throw Error("cannot resize " + path_ + ": not open for writing");
#ifdef _WIN32
	LARGE_INTEGER position;
	position.QuadPart = LONGLONG(size);
	if(!SetFilePointerEx(handle_, position, nullptr, FILE_BEGIN) || !SetEndOfFile(handle_))
		throw Error("could not resize file " + path_ + ": " + systemError());
#else
	if(ftruncate(fd_, off_t(size)) != 0)
		throw Error("could not resize file " + path_ + ": " + systemError());
#endif
	size_ = size;
}

unsigned char *MappedFile::map(uint64_t offset, uint64_t length) {
	if(!isOpen() || length == 0 || offset + length > size_) {
		error_ = "range " + std::to_string(offset) + "+" + std::to_string(length) +
		         " is outside the file " + path_ + " of " + std::to_string(size_) + " bytes";
		return nullptr;
	}
	static const uint64_t granularity = mapGranularity();
	const uint64_t delta = offset % granularity;
	const uint64_t span = length + delta;
#ifdef _WIN32
	const uint64_t start = offset - delta;
	HANDLE section = CreateFileMappingW(handle_, nullptr, writable_ ? PAGE_READWRITE : PAGE_READONLY,
	                                    0, 0, nullptr);
	if(!section) {
		remember("could not map " + path_);
		return nullptr;
	}
	void *base = MapViewOfFile(section, writable_ ? FILE_MAP_WRITE : FILE_MAP_READ,
	                           DWORD(start >> 32), DWORD(start & 0xffffffffu), SIZE_T(span));
	if(!base)
		remember("could not map " + path_);
	CloseHandle(section);  //the view keeps the section alive
	if(!base)
		return nullptr;
#else
	void *base = mmap(nullptr, size_t(span), writable_ ? PROT_READ | PROT_WRITE : PROT_READ,
	                  MAP_SHARED, fd_, off_t(offset - delta));
	if(base == MAP_FAILED) {
		remember("could not map " + path_);
		return nullptr;
	}
#endif
	unsigned char *mapped = static_cast<unsigned char *>(base) + delta;
	views_[mapped] = View{base, span};
	return mapped;
}

bool MappedFile::unmap(const void *mapped) {
	const auto view = views_.find(mapped);
	if(view == views_.end())
		return false;
#ifdef _WIN32
	const bool done = UnmapViewOfFile(view->second.base) != 0;
#else
	const bool done = munmap(view->second.base, size_t(view->second.length)) == 0;
#endif
	views_.erase(view);
	return done;
}

} //namespace
