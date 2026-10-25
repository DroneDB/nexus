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
#ifndef NX_TEXTFILE_H
#define NX_TEXTFILE_H

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace nx {

//Sequential reading of a file, by lines or by blocks, with a position that can be saved and
//restored (the loaders read the same OBJ or TS file several times). Buffered: a line costs
//no system call.
class InputFile {
public:
	//false when the file cannot be opened
	bool open(const std::string &path);
	void close();
	bool isOpen() const { return stream_.is_open(); }

	//offset of the next byte to be read
	uint64_t pos() const { return bufferOffset_ + begin_; }
	void seek(uint64_t offset);

	//reads up to length bytes; fewer only at the end of the file
	size_t read(void *where, size_t length);

	//The next line, without its "\n" or "\r\n"; false at the end of the file. The line is
	//terminated by a zero byte, so the C parsing functions can be used on it, and valid until
	//the next call. A line of any length is returned whole.
	bool readLine(const char *&line, size_t &length);

private:
	bool fill();
	void restoreTerminator();

	std::ifstream stream_;
	std::vector<char> buffer_;  //the byte after the data is always writable
	size_t begin_ = 0;          //next byte to read
	size_t end_ = 0;            //end of the data
	uint64_t bufferOffset_ = 0; //file offset of buffer_[0]
	//the byte replaced by the zero that ends the last line returned, put back before the
	//buffer is used again (a seek can read the line a second time)
	size_t terminatorAt_ = 0;
	char terminator_ = 0;
	bool terminatorPatched_ = false;
};

//the whole content of a file; throws nx::Error when it cannot be read
std::vector<char> readFile(const std::string &path);

//Writing of a file from the start, with seeking back to patch what was written before.
//Every failure throws nx::Error.
class OutputFile {
public:
	OutputFile() = default;
	OutputFile(const OutputFile &) = delete;
	OutputFile &operator=(const OutputFile &) = delete;
	~OutputFile();

	//creates the file, or empties it
	void open(const std::string &path);
	bool isOpen() const { return stream_.is_open(); }
	const std::string &path() const { return path_; }

	void write(const void *data, size_t length);
	void write(const std::string &text) { write(text.data(), text.size()); }
	//offset of the next byte to be written
	uint64_t pos() const { return pos_; }
	//beyond the end of the file the gap reads as zeros once something is written after it
	void seek(uint64_t offset);
	//zero bytes up to offset, which must not be behind pos()
	void padTo(uint64_t offset);
	void close();

private:
	std::ofstream stream_;
	std::string path_;
	uint64_t pos_ = 0;
};

} //namespace

#endif // NX_TEXTFILE_H
