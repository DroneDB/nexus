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
#ifndef NX_IMAGE_H
#define NX_IMAGE_H

#include <cstddef>
#include <cstdint>
#include <memory>

namespace nx {

//An opaque true color image: every pixel is 0xffRRGGBB. This is everything the texture code
//of the builder needs.
//
//Copying an Image is cheap: copies share their pixels. An image is therefore written only
//where it was just created (fill(), draw(), scanLine()); every other operation returns a
//new image and leaves its source alone.
class Image {
public:
	static constexpr uint32_t Opaque = 0xff000000u;

	static uint32_t rgb(unsigned r, unsigned g, unsigned b) {
		return Opaque | (uint32_t(r & 0xff) << 16) | (uint32_t(g & 0xff) << 8) | uint32_t(b & 0xff);
	}

	Image() = default;
	//the pixels are not initialised; throws std::bad_alloc when there is no memory for them
	Image(int width, int height);

	bool isNull() const { return !pixels_; }
	int width() const { return width_; }
	int height() const { return height_; }
	//memory taken by the pixels
	uint64_t byteCount() const { return uint64_t(width_) * uint64_t(height_) * sizeof(uint32_t); }

	uint32_t *scanLine(int y) { return pixels_.get() + size_t(y) * size_t(width_); }
	const uint32_t *scanLine(int y) const { return pixels_.get() + size_t(y) * size_t(width_); }

	void fill(uint32_t pixel);

	//The part of the image inside the rectangle, which must lie inside the image.
	Image copy(int x, int y, int width, int height) const;
	//Upside down.
	Image mirrored() const;
	//Writes the width x height pixels of source starting at (sx, sy) with their top left
	//corner at (x, y). What falls outside this image is dropped; the part of source read
	//must lie inside source.
	void draw(int x, int y, const Image &source, int sx, int sy, int width, int height);
	void draw(int x, int y, const Image &source) { draw(x, y, source, 0, 0, source.width_, source.height_); }

	//Resized to width x height, which is no larger than this image, each pixel the average of
	//the source area it covers (a box filter): a texture made smaller keeps the mean color of
	//its details instead of a scattering of the pixels that happened to be picked. A null image
	//when width or height is not positive; throws std::invalid_argument when either is larger.
	Image downscaled(int width, int height) const;

private:
	int width_ = 0;
	int height_ = 0;
	std::shared_ptr<uint32_t[]> pixels_;
};

} //namespace

#endif // NX_IMAGE_H
