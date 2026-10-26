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
#ifndef NX_IMAGEIO_H
#define NX_IMAGEIO_H

#include <cstddef>
#include <string>
#include <vector>

#include "image.h"

namespace nx {

//Reads a texture file: JPEG, PNG, TIFF, WebP, BMP, GIF or TGA, told apart by their
//content, not by the extension. The alpha channel of an image is dropped and its colors kept
//as they are stored (a pixel that is transparent keeps the color it has in the file).
//Returns a null image, and the reason in error (UTF-8), when the file cannot be opened or read
//as an image.
Image loadImage(const std::string &path, std::string &error);

//JPEG decoding of data already in memory (the same decoder loadImage uses for JPEG files)
Image decodeJpeg(const unsigned char *data, size_t size, std::string &error);

//The image as a JPEG file: 4:2:0 chroma, 100 dpi, quality 0..100 (a negative one is 75).
//optimize chooses the Huffman tables for the image and progressive writes it in several
//scans. Throws nx::Error when the encoder fails.
std::vector<unsigned char> encodeJpeg(const Image &image, int quality, bool optimize, bool progressive);

//The image as a lossy WebP file of the given quality (0..100), without alpha, at encoder effort
//2 (of 0..6). Throws nx::Error when the encoder fails.
std::vector<unsigned char> encodeWebp(const Image &image, int quality);

} //namespace

#endif // NX_IMAGEIO_H
