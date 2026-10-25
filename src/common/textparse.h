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
#ifndef NX_TEXTPARSE_H
#define NX_TEXTPARSE_H

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>

//Small helpers for the loaders of text formats. Numbers are read with strtod/strtof, which
//follow the numeric locale of the process: the code that builds a Nexus file sets it to "C"
//for the duration of the build (see nexusBuild).

namespace nx {
namespace text {

inline bool isSpace(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

inline const char *skipSpaces(const char *p) {
	while(isSpace(*p))
		p++;
	return p;
}

//the line with leading and trailing white space removed and every run of it inside replaced
//by a single space
inline std::string simplified(const char *text, size_t length) {
	std::string out;
	out.reserve(length);
	bool pending = false;
	for(size_t i = 0; i < length; i++) {
		if(isSpace(text[i])) {
			pending = !out.empty();
			continue;
		}
		if(pending)
			out += ' ';
		pending = false;
		out += text[i];
	}
	return out;
}

inline std::string simplified(const std::string &text) {
	return simplified(text.data(), text.size());
}

inline std::string trimmed(const std::string &text) {
	size_t begin = 0;
	size_t end = text.size();
	while(begin < end && isSpace(text[begin]))
		begin++;
	while(end > begin && isSpace(text[end - 1]))
		end--;
	return text.substr(begin, end - begin);
}

//everything after the first word of a simplified line
inline std::string afterFirstWord(const std::string &simplifiedLine) {
	const size_t space = simplifiedLine.find(' ');
	return space == std::string::npos ? std::string() : simplifiedLine.substr(space + 1);
}

//the first word of a simplified line
inline std::string firstWord(const std::string &simplifiedLine) {
	return simplifiedLine.substr(0, simplifiedLine.find(' '));
}

inline bool equalsNoCase(const std::string &a, const char *b) {
	size_t i = 0;
	for(; i < a.size() && b[i]; i++)
		if(std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
			return false;
	return i == a.size() && !b[i];
}

//true when line starts with the word (followed by white space or the end of the line)
inline bool startsWithWord(const char *line, const char *word) {
	size_t n = 0;
	while(word[n]) {
		if(line[n] != word[n])
			return false;
		n++;
	}
	return line[n] == '\0' || isSpace(line[n]);
}

//reads a number after optional white space and moves p past it; false (p unchanged) when there
//is none or it is not finite
inline bool parseDouble(const char *&p, double &value) {
	char *end;
	const double v = std::strtod(p, &end);
	if(end == p || !std::isfinite(v))
		return false;
	value = v;
	p = end;
	return true;
}

inline bool parseFloat(const char *&p, float &value) {
	char *end;
	const float v = std::strtof(p, &end);
	if(end == p || !std::isfinite(v))
		return false;
	value = v;
	p = end;
	return true;
}

//reads an optionally signed decimal integer (no white space before it); false when there is
//none or it does not fit in 18 digits
inline bool parseInteger(const char *&p, int64_t &value) {
	const char *q = p;
	bool negative = false;
	if(*q == '-' || *q == '+') {
		negative = *q == '-';
		q++;
	}
	const char *digits = q;
	int64_t v = 0;
	while(*q >= '0' && *q <= '9') {
		v = v * 10 + (*q - '0');
		q++;
		if(q - digits > 18)
			return false;
	}
	if(q == digits)
		return false;
	value = negative ? -v : v;
	p = q;
	return true;
}

} //namespace
} //namespace

#endif // NX_TEXTPARSE_H
