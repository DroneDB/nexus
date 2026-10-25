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
#ifndef NX_ERROR_H
#define NX_ERROR_H

#include <stdexcept>
#include <string>

namespace nx {

//The one exception the Nexus code throws for a failure it reports to its caller (unreadable or
//malformed input, a file that cannot be written, ...). The message is UTF-8.
class Error: public std::runtime_error {
public:
	explicit Error(const std::string &message): std::runtime_error(message) {}
};

//A node of the model needs a texture too large to pack (see NexusBuilder::extractNodeTex): the
//input is fine, but the Nexus file cannot be built with its textures.
class TexturePackingError: public Error {
public:
	explicit TexturePackingError(const std::string &message): Error(message) {}
};

//A node of the model has more vertices than a node can hold once its texture seams are split (the
//texture parametrization is too fragmented for nodes of this size): the input is fine, and the
//build may succeed with fewer triangles per node.
class NodeTooLargeError: public Error {
public:
	explicit NodeTooLargeError(const std::string &message): Error(message) {}
};

} //namespace

#endif // NX_ERROR_H
