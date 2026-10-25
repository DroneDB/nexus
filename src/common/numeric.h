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
#ifndef NX_NUMERIC_H
#define NX_NUMERIC_H

#include <algorithm>
#include <cmath>

namespace nx {

//true when a and b differ by less than a part in 100000 of the smaller one: the comparison of
//two floats that Qt has as qFuzzyCompare
inline bool fuzzyCompare(float a, float b) {
	return std::fabs(a - b) * 100000.f <= std::min(std::fabs(a), std::fabs(b));
}

} //namespace

#endif // NX_NUMERIC_H
