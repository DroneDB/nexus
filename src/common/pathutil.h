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
#include <vector>

#include "error.h"

//File names are UTF-8 std::string everywhere in Nexus. They are turned into a
//std::filesystem::path (which is UTF-16 on Windows, so any script can be opened) only where a
//file is touched, through nativePath or systemPath so that the length of a path is no limit
//either, and every path that comes out of here is UTF-8 with '/' separators.

namespace nx {

inline std::filesystem::path pathFromUtf8(const std::string &utf8) {
	//u8path is deprecated only from C++20, where the char8_t constructor takes its place
	return std::filesystem::u8path(utf8);
}

inline std::string pathToUtf8(const std::filesystem::path &path) {
	const auto utf8 = path.generic_u8string();
	return std::string(utf8.begin(), utf8.end());
}

//The path in the form the Windows file functions take whatever its length: absolute, with "."
//and ".." resolved, '\' separators and the "\\?\" prefix ("\\?\UNC\server\share\..." for a
//network path). Without the prefix those functions, and the C and C++ library functions built
//on them, fail on a path of MAX_PATH (260) characters or more, which a scratch file deep in a
//build folder reaches. The prefix also turns off the resolving Windows does, hence the resolving
//here. A relative path is resolved against base, an absolute path; a path with the prefix
//already, or one naming a device ("\\.\"), is returned as it is, and so is a relative one when
//base is not absolute. It touches no file and is the same on every platform, so it can be tested
//anywhere.
inline std::string windowsLongPath(const std::string &path, const std::string &base) {
	const auto backslashed = [](std::string text) {
		for(char &c: text)
			if(c == '/')
				c = '\\';
		return text;
	};
	const auto isDrive = [](const std::string &text) {
		return text.size() >= 2 && text[1] == ':' &&
		       ((text[0] >= 'A' && text[0] <= 'Z') || (text[0] >= 'a' && text[0] <= 'z'));
	};
	//root: "C:" or "UNC\server\share"; rest: what follows it. False when text is not absolute.
	const auto splitRoot = [&](const std::string &text, std::string &root, std::string &rest) {
		if(text.size() >= 2 && text[0] == '\\' && text[1] == '\\') {
			const size_t server = text.find('\\', 2);
			const size_t share = server == std::string::npos ? server : text.find('\\', server + 1);
			root = "UNC\\" + text.substr(2, share == std::string::npos ? share : share - 2);
			rest = share == std::string::npos ? std::string() : text.substr(share + 1);
			return true;
		}
		if(isDrive(text) && text.size() >= 3 && text[2] == '\\') {
			root = text.substr(0, 2);
			rest = text.substr(3);
			return true;
		}
		return false;
	};

	const std::string target = backslashed(path);
	if(target.rfind("\\\\?\\", 0) == 0 || target.rfind("\\\\.\\", 0) == 0)
		return target;

	std::string root, rest;
	if(!splitRoot(target, root, rest)) {
		std::string baseRoot, baseRest;
		if(!splitRoot(backslashed(base), baseRoot, baseRest))
			return target;
		const bool sameDrive = isDrive(target) && isDrive(baseRoot) && (target[0] | 0x20) == (baseRoot[0] | 0x20);
		root = baseRoot;
		if(!target.empty() && target[0] == '\\') {
			rest = target;                         //"\x": from the root of base
		} else if(isDrive(target) && !sameDrive) {
			root = target.substr(0, 2);            //"D:x" on another drive: from the root of that drive
			rest = target.substr(2);
		} else {
			rest = baseRest + "\\" + (isDrive(target) ? target.substr(2) : target);
		}
	}

	std::vector<std::string> parts;
	size_t start = 0;
	while(start <= rest.size()) {
		size_t end = rest.find('\\', start);
		if(end == std::string::npos)
			end = rest.size();
		const std::string part = rest.substr(start, end - start);
		if(part == "..") {
			if(!parts.empty())
				parts.pop_back();
		} else if(!part.empty() && part != ".") {
			parts.push_back(part);
		}
		start = end + 1;
	}

	std::string result = "\\\\?\\" + root + "\\";
	for(size_t i = 0; i < parts.size(); i++)
		result += (i ? "\\" : "") + parts[i];
	return result;
}

//The path to give the file functions of the system, as UTF-8: on Windows windowsLongPath of it
//(a relative path is relative to the working directory), elsewhere the path as it is. Every
//file Nexus opens, creates, renames or removes goes through here or nativePath.
inline std::string systemPath(const std::string &utf8) {
#ifdef _WIN32
	std::error_code ec;
	const auto workingDirectory = std::filesystem::current_path(ec);
	return windowsLongPath(utf8, ec ? std::string() : pathToUtf8(workingDirectory));
#else
	return utf8;
#endif
}

//systemPath as a std::filesystem::path, for the standard library file functions
inline std::filesystem::path nativePath(const std::string &utf8) {
	return pathFromUtf8(systemPath(utf8));
}

inline bool pathExists(const std::string &path) {
	std::error_code ec;
	return std::filesystem::exists(nativePath(path), ec);
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
