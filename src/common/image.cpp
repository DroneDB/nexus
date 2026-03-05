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

// We define the stb implementations in this translation unit only.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE2_IMPLEMENTATION

#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_image_resize2.h>

// For JPEG in-memory compression we use libjpeg-turbo (higher quality than stb for JPEG)
#include <jpeglib.h>
#include <csetjmp>

// WebP decoding support
#include <webp/decode.h>

#include "image.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <filesystem>

namespace nx {

// ---------------------------------------------------------------------------
// Construction / copy / move
// ---------------------------------------------------------------------------

Image::Image() = default;

Image::Image(int width, int height)
	: width_(width), height_(height), pixels_(width * height * channels_, 0) {}

Image::Image(const Image &other) = default;
Image &Image::operator=(const Image &other) = default;
Image::Image(Image &&other) noexcept
	: width_(other.width_), height_(other.height_), pixels_(std::move(other.pixels_)) {
	other.width_ = 0;
	other.height_ = 0;
}
Image &Image::operator=(Image &&other) noexcept {
	if (this != &other) {
		width_ = other.width_;
		height_ = other.height_;
		pixels_ = std::move(other.pixels_);
		other.width_ = 0;
		other.height_ = 0;
	}
	return *this;
}

// ---------------------------------------------------------------------------
// Scanline access
// ---------------------------------------------------------------------------

unsigned char *Image::scanLine(int y) {
	return pixels_.data() + static_cast<size_t>(y) * width_ * channels_;
}

const unsigned char *Image::scanLine(int y) const {
	return pixels_.data() + static_cast<size_t>(y) * width_ * channels_;
}

// ---------------------------------------------------------------------------
// Fill
// ---------------------------------------------------------------------------

void Image::fill(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	for (int y = 0; y < height_; y++) {
		unsigned char *row = scanLine(y);
		for (int x = 0; x < width_; x++) {
			row[x * 4 + 0] = r;
			row[x * 4 + 1] = g;
			row[x * 4 + 2] = b;
			row[x * 4 + 3] = a;
		}
	}
}

// ---------------------------------------------------------------------------
// Load from file
// ---------------------------------------------------------------------------

bool Image::load(const std::string &filename) {
	// Check if file is WebP by extension
	std::string ext = std::filesystem::path(filename).extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
	if (ext == ".webp" || ext == ".web") {
		std::ifstream f(filename, std::ios::binary | std::ios::ate);
		if (!f) return false;
		size_t size = static_cast<size_t>(f.tellg());
		f.seekg(0);
		std::vector<unsigned char> buf(size);
		f.read(reinterpret_cast<char*>(buf.data()), size);
		if (!f) return false;
		return loadWebP(buf.data(), size);
	}

	int w = 0, h = 0, comp = 0;
	unsigned char *data = stbi_load(filename.c_str(), &w, &h, &comp, 4); // force RGBA
	if (!data)
		return false;
	width_ = w;
	height_ = h;
	pixels_.assign(data, data + static_cast<size_t>(w) * h * 4);
	stbi_image_free(data);
	return true;
}

// ---------------------------------------------------------------------------
// Load from memory
// ---------------------------------------------------------------------------

bool Image::loadFromData(const unsigned char *buf, size_t size) {
	// Try WebP first if the buffer starts with RIFF....WEBP signature
	if (size >= 12 && buf[0] == 'R' && buf[1] == 'I' && buf[2] == 'F' && buf[3] == 'F'
	    && buf[8] == 'W' && buf[9] == 'E' && buf[10] == 'B' && buf[11] == 'P') {
		return loadWebP(buf, size);
	}

	int w = 0, h = 0, comp = 0;
	unsigned char *data = stbi_load_from_memory(buf, static_cast<int>(size), &w, &h, &comp, 4);
	if (!data)
		return false;
	width_ = w;
	height_ = h;
	pixels_.assign(data, data + static_cast<size_t>(w) * h * 4);
	stbi_image_free(data);
	return true;
}

// ---------------------------------------------------------------------------
// WebP decoding helper
// ---------------------------------------------------------------------------

bool Image::loadWebP(const unsigned char *buf, size_t size) {
	int w = 0, h = 0;
	// WebPDecodeRGBA returns a malloc'd buffer with RGBA pixels
	unsigned char *data = WebPDecodeRGBA(buf, size, &w, &h);
	if (!data)
		return false;
	width_ = w;
	height_ = h;
	pixels_.assign(data, data + static_cast<size_t>(w) * h * 4);
	WebPFree(data);
	return true;
}

// ---------------------------------------------------------------------------
// Save to file
// ---------------------------------------------------------------------------

bool Image::save(const std::string &filename, int quality) const {
	if (isNull()) return false;

	std::string ext = std::filesystem::path(filename).extension().string();
	// lowercase
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	if (ext == ".png") {
		return stbi_write_png(filename.c_str(), width_, height_, 4, pixels_.data(), width_ * 4) != 0;
	} else if (ext == ".jpg" || ext == ".jpeg") {
		// Convert RGBA -> RGB for JPEG (stb_image_write doesn't handle alpha in JPEG)
		std::vector<unsigned char> rgb(width_ * height_ * 3);
		for (int i = 0; i < width_ * height_; i++) {
			rgb[i * 3 + 0] = pixels_[i * 4 + 0];
			rgb[i * 3 + 1] = pixels_[i * 4 + 1];
			rgb[i * 3 + 2] = pixels_[i * 4 + 2];
		}
		return stbi_write_jpg(filename.c_str(), width_, height_, 3, rgb.data(), quality) != 0;
	} else if (ext == ".bmp") {
		return stbi_write_bmp(filename.c_str(), width_, height_, 4, pixels_.data()) != 0;
	}
	return false;
}

// ---------------------------------------------------------------------------
// Save to memory (JPEG via libjpeg-turbo for quality control)
// ---------------------------------------------------------------------------

bool Image::saveToMemory(std::vector<unsigned char> &outBuffer, int quality) const {
	if (isNull()) return false;

	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	unsigned char *jpegBuf = nullptr;
	unsigned long jpegSize = 0;
	jpeg_mem_dest(&cinfo, &jpegBuf, &jpegSize);

	cinfo.image_width = width_;
	cinfo.image_height = height_;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;

	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);
	jpeg_simple_progression(&cinfo);
	jpeg_start_compress(&cinfo, TRUE);

	// Convert RGBA -> RGB row by row
	std::vector<unsigned char> row(width_ * 3);
	while (cinfo.next_scanline < cinfo.image_height) {
		const unsigned char *src = scanLine(cinfo.next_scanline);
		for (int x = 0; x < width_; x++) {
			row[x * 3 + 0] = src[x * 4 + 0];
			row[x * 3 + 1] = src[x * 4 + 1];
			row[x * 3 + 2] = src[x * 4 + 2];
		}
		unsigned char *rowPtr = row.data();
		jpeg_write_scanlines(&cinfo, &rowPtr, 1);
	}

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	outBuffer.assign(jpegBuf, jpegBuf + jpegSize);
	free(jpegBuf);
	return true;
}

// ---------------------------------------------------------------------------
// Copy (crop) a rectangular region
// ---------------------------------------------------------------------------

Image Image::copy(const Rect &region) const {
	int sx = std::max(0, region.x);
	int sy = std::max(0, region.y);
	int ex = std::min(width_, region.x + region.w);
	int ey = std::min(height_, region.y + region.h);
	int rw = ex - sx;
	int rh = ey - sy;
	if (rw <= 0 || rh <= 0) return Image();

	Image result(rw, rh);
	for (int y = 0; y < rh; y++) {
		const unsigned char *srcRow = scanLine(sy + y) + sx * channels_;
		unsigned char *dstRow = result.scanLine(y);
		std::memcpy(dstRow, srcRow, rw * channels_);
	}
	return result;
}

// ---------------------------------------------------------------------------
// Scale
// ---------------------------------------------------------------------------

Image Image::scaled(int newWidth, int newHeight) const {
	if (isNull() || newWidth <= 0 || newHeight <= 0) return Image();

	Image result(newWidth, newHeight);
	stbir_resize_uint8_linear(
		pixels_.data(), width_, height_, width_ * channels_,
		result.data(), newWidth, newHeight, newWidth * channels_,
		STBIR_RGBA
	);
	return result;
}

// ---------------------------------------------------------------------------
// Mirror (vertical flip)
// ---------------------------------------------------------------------------

Image Image::mirrored() const {
	if (isNull()) return Image();

	Image result(width_, height_);
	size_t rowBytes = static_cast<size_t>(width_) * channels_;
	for (int y = 0; y < height_; y++) {
		const unsigned char *src = scanLine(y);
		unsigned char *dst = result.scanLine(height_ - 1 - y);
		std::memcpy(dst, src, rowBytes);
	}
	return result;
}

// ---------------------------------------------------------------------------
// Blit (composite one image onto another)
// ---------------------------------------------------------------------------

void Image::blit(int destX, int destY, const Image &src, const Rect &srcRect) {
	int sx = std::max(0, srcRect.x);
	int sy = std::max(0, srcRect.y);
	int sw = std::min(srcRect.w, src.width() - sx);
	int sh = std::min(srcRect.h, src.height() - sy);

	// Clip to destination bounds
	if (destX < 0) { sx -= destX; sw += destX; destX = 0; }
	if (destY < 0) { sy -= destY; sh += destY; destY = 0; }
	sw = std::min(sw, width_ - destX);
	sh = std::min(sh, height_ - destY);
	if (sw <= 0 || sh <= 0) return;

	for (int y = 0; y < sh; y++) {
		const unsigned char *srcRow = src.scanLine(sy + y) + sx * channels_;
		unsigned char *dstRow = scanLine(destY + y) + destX * channels_;
		std::memcpy(dstRow, srcRow, sw * channels_);
	}
}

void Image::blit(int destX, int destY, const Image &src) {
	blit(destX, destY, src, Rect(0, 0, src.width(), src.height()));
}

} // namespace nx
