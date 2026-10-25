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
#ifndef NX_PATHUTIL_H
#define NX_PATHUTIL_H

#include <filesystem>
#include <string>
#include <system_error>

#include "error.h"

//File names are UTF-8 std::string everywhere in Nexus. They are turned into a
//std::filesystem::path (which is UTF-16 on Windows, so any script can be opened) only where a
//file is touched, and every path that comes out of here is UTF-8 with '/' separators.

namespace nx {

inline std::filesystem::path pathFromUtf8(const std::string &utf8) {
	//u8path is deprecated only from C++20, where the char8_t constructor takes its place
	return std::filesystem::u8path(utf8);
}

inline std::string pathToUtf8(const std::filesystem::path &path) {
	const auto utf8 = path.generic_u8string();
	return std::string(utf8.begin(), utf8.end());
}

inline bool pathExists(const std::string &path) {
	std::error_code ec;
	return std::filesystem::exists(pathFromUtf8(path), ec);
}

inline bool isAbsolutePath(const std::string &path) {
	return pathFromUtf8(path).is_absolute();
}

//name inside dir; a name that is absolute stays as it is
inline std::string joinPath(const std::string &dir, const std::string &name) {
	return pathToUtf8(pathFromUtf8(dir) / pathFromUtf8(name));
}

//the folder part of a path, "." when there is none
inline std::string parentPath(const std::string &path) {
	const auto parent = pathFromUtf8(path).parent_path();
	return parent.empty() ? std::string(".") : pathToUtf8(parent);
}

//the folder part of a path as an absolute path, with "." and ".." resolved
inline std::string absoluteParentPath(const std::string &path) {
	std::error_code ec;
	const auto absolute = std::filesystem::absolute(pathFromUtf8(path), ec);
	if(ec)
		throw Error("could not resolve the path " + path + ": " + ec.message());
	return pathToUtf8(absolute.lexically_normal().parent_path());
}

//the file name without its last extension
inline std::string completeBaseName(const std::string &path) {
	return pathToUtf8(pathFromUtf8(path).stem());
}

inline std::string temporaryDirectory() {
	std::error_code ec;
	const auto dir = std::filesystem::temp_directory_path(ec);
	if(ec)
		throw Error("could not find the temporary directory: " + ec.message());
	return pathToUtf8(dir);
}

//case sensitive, like the extension tests Nexus has always done
inline bool endsWith(const std::string &text, const std::string &suffix) {
	return text.size() >= suffix.size() &&
	       text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

//the same, ignoring the case of ASCII letters
inline bool endsWithNoCase(const std::string &text, const std::string &suffix) {
	if(text.size() < suffix.size())
		return false;
	for(size_t i = 0; i < suffix.size(); i++) {
		const char a = text[text.size() - suffix.size() + i];
		const char b = suffix[i];
		if((a >= 'A' && a <= 'Z' ? a + ('a' - 'A') : a) != (b >= 'A' && b <= 'Z' ? b + ('a' - 'A') : b))
			return false;
	}
	return true;
}

} //namespace

#endif // NX_PATHUTIL_H
