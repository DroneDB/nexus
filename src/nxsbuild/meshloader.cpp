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
#include "meshloader.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

void MeshLoader::quantize(float &value) {
	if(!quantization) return;
	value = quantization*(int)(value/quantization);
}

void MeshLoader::sanitizeTextureFilepath(std::string &textureFilepath) {
	std::replace(textureFilepath.begin(), textureFilepath.end(), '\\', '/');
}

void MeshLoader::resolveTextureFilepath(const std::string &modelFilepath, std::string &textureFilepath) {
	fs::path modelPath(modelFilepath);
	fs::path parentDir = modelPath.parent_path();
	textureFilepath = (parentDir / textureFilepath).string();
}
