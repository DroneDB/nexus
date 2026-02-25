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
#ifndef NX_IMAGE_H
#define NX_IMAGE_H

#include <cstdint>
#include <vector>
#include <string>

namespace nx {

/// A simple RGBA image buffer. Replaces QImage for texture processing.
/// Channels are always 4 (RGBA) internally matching the old QImage::Format_RGB32 layout.
/// Pixel order: BGRA (matching QImage::Format_RGB32 on little-endian) — actually we
/// use RGBA throughout for simplicity with stb/libjpeg.
struct Rect {
	int x, y, w, h;
	Rect(): x(0), y(0), w(0), h(0) {}
	Rect(int _x, int _y, int _w, int _h): x(_x), y(_y), w(_w), h(_h) {}
	int width() const { return w; }
	int height() const { return h; }
};

class Image {
public:
	Image();
	Image(int width, int height);
	Image(const Image &other);
	Image &operator=(const Image &other);
	Image(Image &&other) noexcept;
	Image &operator=(Image &&other) noexcept;
	~Image() = default;

	/// Accessors
	int width() const { return width_; }
	int height() const { return height_; }
	bool isNull() const { return width_ <= 0 || height_ <= 0; }

	/// Raw scanline access (RGBA, 4 bytes per pixel).
	unsigned char *scanLine(int y);
	const unsigned char *scanLine(int y) const;

	/// Raw data pointer.
	unsigned char *data() { return pixels_.data(); }
	const unsigned char *data() const { return pixels_.data(); }
	size_t dataSize() const { return pixels_.size(); }

	/// Fill the entire image with a solid color.
	void fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

	/// Load from file (JPEG, PNG, etc. via stb_image).
	bool load(const std::string &filename);

	/// Load from memory buffer (e.g. compressed JPEG data).
	bool loadFromData(const unsigned char *data, size_t size);

	/// Save to file. Format detected from extension.
	/// @param quality  JPEG quality 0-100 (ignored for PNG).
	bool save(const std::string &filename, int quality = 95) const;

	/// Compress image to JPEG in memory.
	/// @param outBuffer  Output byte vector.
	/// @param quality    JPEG quality 0-100.
	bool saveToMemory(std::vector<unsigned char> &outBuffer, int quality = 95) const;

	/// Copy a rectangular region (crop). Returns a new image.
	Image copy(const Rect &region) const;

	/// Scale the image to new dimensions using stb_image_resize.
	Image scaled(int newWidth, int newHeight) const;

	/// Flip vertically (mirror).
	Image mirrored() const;

	/// Blit (copy) a source rectangle from `src` onto this image at `destX, destY`.
	void blit(int destX, int destY, const Image &src, const Rect &srcRect);

	/// Blit the entire `src` image onto this image at `destX, destY`.
	void blit(int destX, int destY, const Image &src);

private:
	int width_ = 0;
	int height_ = 0;
	static constexpr int channels_ = 4; // always RGBA
	std::vector<unsigned char> pixels_;
};

} // namespace nx

#endif // NX_IMAGE_H
