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
#include "image.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

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

//The source pixels one target pixel of a box filtered resize covers along an axis, with the
//share of each one: target pixel i spans [i * ratio, (i + 1) * ratio) of the source.
struct BoxSpan {
	int first = 0;
	std::vector<float> weights;   //of source pixels first, first + 1, ... summing to 1
};

std::vector<BoxSpan> boxSpans(int source, int target) {
	std::vector<BoxSpan> spans(static_cast<size_t>(target));
	const double ratio = double(source) / double(target);
	for(int i = 0; i < target; i++) {
		const double start = i * ratio;
		const double end = std::min(double(source), (i + 1) * ratio);
		BoxSpan &span = spans[size_t(i)];
		span.first = int(std::floor(start));
		for(int s = span.first; s < source && s < end; s++) {
			const double covered = std::min(end, s + 1.0) - std::max(start, double(s));
			span.weights.push_back(float(covered / (end - start)));
		}
	}
	return spans;
}

uint32_t packPixel(const float *rgb) {
	auto channel = [](float v) { return unsigned(std::min(255.0f, std::max(0.0f, v + 0.5f))); };
	return Image::rgb(channel(rgb[0]), channel(rgb[1]), channel(rgb[2]));
}

} //namespace

Image Image::downscaled(int width, int height) const {
	if(isNull() || width <= 0 || height <= 0)
		return Image();
	if(width > width_ || height > height_)
		throw std::invalid_argument("an image is downscaled to a size larger than its own");
	if(width == width_ && height == height_)
		return *this;

	const std::vector<BoxSpan> columns = boxSpans(width_, width);
	const std::vector<BoxSpan> rows = boxSpans(height_, height);
	Image result(width, height);
	std::vector<float> row(size_t(width) * 3), sum(size_t(width) * 3);
	for(int y = 0; y < height; y++) {
		std::fill(sum.begin(), sum.end(), 0.0f);
		const BoxSpan &rowSpan = rows[size_t(y)];
		for(size_t r = 0; r < rowSpan.weights.size(); r++) {
			//the source row, filtered across to the target width
			const uint32_t *from = scanLine(rowSpan.first + int(r));
			for(int x = 0; x < width; x++) {
				const BoxSpan &column = columns[size_t(x)];
				float rgb[3] = {0, 0, 0};
				for(size_t c = 0; c < column.weights.size(); c++) {
					const uint32_t p = from[column.first + int(c)];
					const float w = column.weights[c];
					rgb[0] += w * float((p >> 16) & 0xff);
					rgb[1] += w * float((p >> 8) & 0xff);
					rgb[2] += w * float(p & 0xff);
				}
				for(int k = 0; k < 3; k++)
					row[size_t(x) * 3 + k] = rgb[k];
			}
			const float w = rowSpan.weights[r];
			for(size_t k = 0; k < sum.size(); k++)
				sum[k] += w * row[k];
		}
		uint32_t *to = result.scanLine(y);
		for(int x = 0; x < width; x++)
			to[x] = packPixel(&sum[size_t(x) * 3]);
	}
	return result;
}

} //namespace
