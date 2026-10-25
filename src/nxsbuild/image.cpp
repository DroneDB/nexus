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
#include "image.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace nx {

Image::Image(int width, int height) {
	if(width <= 0 || height <= 0)
		throw std::invalid_argument("an image needs a positive size");
	pixels_.reset(new uint32_t[size_t(width) * size_t(height)]);
	width_ = width;
	height_ = height;
}

void Image::fill(uint32_t pixel) {
	if(pixels_)
		std::fill_n(pixels_.get(), size_t(width_) * size_t(height_), pixel);
}

Image Image::copy(int x, int y, int width, int height) const {
	if(x < 0 || y < 0 || width <= 0 || height <= 0 || x + int64_t(width) > width_ || y + int64_t(height) > height_)
		throw std::out_of_range("the part of an image to copy lies outside of it");
	Image result(width, height);
	for(int row = 0; row < height; row++)
		std::memcpy(result.scanLine(row), scanLine(y + row) + x, size_t(width) * sizeof(uint32_t));
	return result;
}

Image Image::mirrored() const {
	if(isNull())
		return Image();
	Image result(width_, height_);
	for(int row = 0; row < height_; row++)
		std::memcpy(result.scanLine(height_ - 1 - row), scanLine(row), size_t(width_) * sizeof(uint32_t));
	return result;
}

void Image::draw(int x, int y, const Image &source, int sx, int sy, int width, int height) {
	if(isNull() || source.isNull())
		return;
	if(sx < 0 || sy < 0 || width < 0 || height < 0 ||
	   sx + int64_t(width) > source.width_ || sy + int64_t(height) > source.height_)
		throw std::out_of_range("the part of an image to draw lies outside of it");
	//clip against this image
	if(x < 0) { sx -= x; width += x; x = 0; }
	if(y < 0) { sy -= y; height += y; y = 0; }
	width = int(std::min<int64_t>(width, width_ - int64_t(x)));
	height = int(std::min<int64_t>(height, height_ - int64_t(y)));
	if(width <= 0 || height <= 0)
		return;
	for(int row = 0; row < height; row++)
		std::memmove(scanLine(y + row) + x, source.scanLine(sy + row) + sx, size_t(width) * sizeof(uint32_t));
}

namespace {

//How one axis of a nearest neighbour resize reads its source: target pixel i takes the source
//pixel (base + i * step) >> 16. The numbers are those of Qt's FastTransformation scaling of a
//32 bit image (16.16 fixed point, the step truncated), so a resized texture keeps the pixels it
//always had, including the last target pixel that Qt leaves black when the walk would end
//outside of the source.
struct ScaleAxis {
	uint64_t base;
	uint64_t step;
	int count;       //target pixels that read the source; the ones after them stay black

	ScaleAxis(int source, int target) {
		const double scale = double(target) / double(source);
		const double covered = double(source) * scale;           //size of the resized image
		const double ratio = double(source) / covered;           //source pixels per target pixel
		step = uint64_t(65536.0 * ratio);
		count = std::min(int(std::floor(covered + 0.5)), target);
		base = uint64_t(std::ceil(0.5 * ratio * 65536.0)) - 1;
		//Qt drops the last pixel when it would read past the source; this drops as many as
		//needed, which is the same for every size it has been seen to work with, and never
		//reads out of the image
		const uint64_t end = uint64_t(source) << 16;
		if(base >= end)
			count = 0;
		else if(step != 0)
			count = int(std::min<uint64_t>(uint64_t(count), (end - 1 - base) / step + 1));
	}
};

} //namespace

Image Image::scaled(int width, int height) const {
	if(isNull() || width <= 0 || height <= 0)
		return Image();
	if(width == width_ && height == height_)
		return *this;

	Image result(width, height);
	if(width_ == 1 && height_ == 1) {
		//Qt paints a single pixel as a flat color
		result.fill(pixels_[0]);
		return result;
	}

	const ScaleAxis horizontal(width_, width), vertical(height_, height);
	if(horizontal.count < width || vertical.count < height)
		result.fill(0);
	uint64_t row = vertical.base;
	for(int y = 0; y < vertical.count; y++, row += vertical.step) {
		const uint32_t *from = scanLine(int(row >> 16));
		uint32_t *to = result.scanLine(y);
		uint64_t at = horizontal.base;
		for(int x = 0; x < horizontal.count; x++, at += horizontal.step)
			to[x] = from[at >> 16];
	}
	return result;
}

Image Image::scaledToFit(int width, int height) const {
	if(isNull() || width <= 0 || height <= 0)
		return Image();
	int64_t fitWidth = width, fitHeight = height;
	const int64_t proportionalWidth = int64_t(height) * width_ / height_;
	if(proportionalWidth <= width) {
		fitWidth = proportionalWidth;
	} else {
		fitHeight = int64_t(width) * height_ / width_;
	}
	return scaled(int(std::max<int64_t>(fitWidth, 1)), int(std::max<int64_t>(fitHeight, 1)));
}

} //namespace
