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
#include "textfile.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

#include "error.h"
#include "pathutil.h"

namespace nx {

namespace {
const size_t InitialBuffer = 1 << 20;
}

bool InputFile::open(const std::string &path) {
	close();
	stream_.open(nativePath(path), std::ios::in | std::ios::binary);
	if(!stream_.is_open())
		return false;
	buffer_.resize(InitialBuffer + 1);
	return true;
}

void InputFile::close() {
	terminatorPatched_ = false;
	stream_.close();
	stream_.clear();
	begin_ = end_ = 0;
	bufferOffset_ = 0;
}

void InputFile::restoreTerminator() {
	if(terminatorPatched_)
		buffer_[terminatorAt_] = terminator_;
	terminatorPatched_ = false;
}

void InputFile::seek(uint64_t offset) {
	restoreTerminator();
	if(offset >= bufferOffset_ && offset <= bufferOffset_ + end_) {
		begin_ = size_t(offset - bufferOffset_);  //still in the buffer
		return;
	}
	stream_.clear();
	stream_.seekg(std::streamoff(offset));
	bufferOffset_ = offset;
	begin_ = end_ = 0;
}

//reads more data after the end of the current data, keeping the unread bytes; false at the end
//of the file
bool InputFile::fill() {
	if(begin_ > 0) {
		std::memmove(buffer_.data(), buffer_.data() + begin_, end_ - begin_);
		bufferOffset_ += begin_;
		end_ -= begin_;
		begin_ = 0;
	}
	if(end_ == buffer_.size() - 1)  //a line longer than the buffer
		buffer_.resize(buffer_.size() * 2);

	stream_.read(buffer_.data() + end_, std::streamsize(buffer_.size() - 1 - end_));
	const size_t got = size_t(stream_.gcount());
	stream_.clear();  //the end of the file is not an error for a later seek
	end_ += got;
	return got > 0;
}

size_t InputFile::read(void *where, size_t length) {
	restoreTerminator();
	char *out = static_cast<char *>(where);
	size_t done = 0;
	while(done < length) {
		if(begin_ == end_) {
			const size_t left = length - done;
			if(left >= buffer_.size()) {  //big enough to skip the buffer
				bufferOffset_ += end_;
				begin_ = end_ = 0;
				stream_.read(out + done, std::streamsize(left));
				const size_t got = size_t(stream_.gcount());
				stream_.clear();
				bufferOffset_ += got;
				done += got;
				break;
			}
			if(!fill())
				break;
		}
		const size_t n = std::min(length - done, end_ - begin_);
		std::memcpy(out + done, buffer_.data() + begin_, n);
		begin_ += n;
		done += n;
	}
	return done;
}

bool InputFile::readLine(const char *&line, size_t &length) {
	restoreTerminator();
	size_t scanned = 0;  //bytes after begin_ known to hold no newline
	size_t lineEnd = 0;
	size_t next = 0;
	while(true) {
		const char *start = buffer_.data() + begin_;
		const char *newline = static_cast<const char *>(
				std::memchr(start + scanned, '\n', end_ - begin_ - scanned));
		if(newline) {
			lineEnd = size_t(newline - buffer_.data());
			next = lineEnd + 1;
			break;
		}
		scanned = end_ - begin_;
		if(!fill()) {
			if(begin_ == end_)
				return false;
			lineEnd = next = end_;  //the last line has no newline
			break;
		}
	}
	const size_t lineBegin = begin_;
	begin_ = next;
	if(lineEnd > lineBegin && buffer_[lineEnd - 1] == '\r')
		lineEnd--;
	terminatorAt_ = lineEnd;
	terminator_ = buffer_[lineEnd];
	terminatorPatched_ = true;
	buffer_[lineEnd] = '\0';
	line = buffer_.data() + lineBegin;
	length = lineEnd - lineBegin;
	return true;
}

std::vector<char> readFile(const std::string &path) {
	InputFile file;
	if(!file.open(path))
		throw Error("could not open file " + path);
	std::error_code ec;
	const auto size = std::filesystem::file_size(nativePath(path), ec);
	if(ec)
		throw Error("could not read file " + path + ": " + ec.message());
	std::vector<char> content(static_cast<size_t>(size));
	if(file.read(content.data(), content.size()) != content.size())
		throw Error("could not read file " + path);
	return content;
}

OutputFile::~OutputFile() {
	//a failure to close is reported by an explicit close()
	stream_.close();
}

void OutputFile::open(const std::string &path) {
	close();
	path_ = path;
	stream_.open(nativePath(path), std::ios::out | std::ios::binary | std::ios::trunc);
	if(!stream_.is_open())
		throw Error("could not open file " + path + " for writing");
	pos_ = 0;
}

void OutputFile::write(const void *data, size_t length) {
	if(!length)
		return;
	stream_.write(static_cast<const char *>(data), std::streamsize(length));
	if(!stream_)
		throw Error("could not write to file " + path_);
	pos_ += length;
}

void OutputFile::seek(uint64_t offset) {
	stream_.seekp(std::streamoff(offset));
	if(!stream_)
		throw Error("could not seek in file " + path_);
	pos_ = offset;
}

void OutputFile::padTo(uint64_t offset) {
	static const char zeros[4096] = {};
	while(pos_ < offset)
		write(zeros, size_t(std::min<uint64_t>(offset - pos_, sizeof(zeros))));
}

void OutputFile::close() {
	if(!stream_.is_open())
		return;
	stream_.close();
	if(!stream_)
		throw Error("could not write to file " + path_);
}

} //namespace
