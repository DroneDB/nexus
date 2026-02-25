#include <assert.h>
#include <math.h>
#include <iostream>
#include <cstring>
#include <stdexcept>

#include "../common/logger.h"
#include "texpyramid.h"

using namespace nx;
using namespace std;

void TexLevel::init(int t, TexAtlas* c, Image& texture, int _level = 0) {
	tex = t;
	level = _level;
	collection = c;
	int side = collection->side;

	width = texture.width();
	height = texture.height();

	tilew = (width-1)/side +1;
	tileh = (height-1)/side +1;

	for(int y = 0; y < tileh; y++) {
		for(int x = 0; x < tilew; x++) {
			int sx = x*side;
			int wx = (sx + side > width)? width - sx : side;
			int sy = y*side;
			int wy = (sy + side > height)? height - sy : side;
			int isy = height - (sy + wy);
			Image img = texture.copy(Rect(sx, isy, wx, wy));
			img = img.mirrored();
			collection->addImg(TexAtlas::Index(tex, level, x + y*tilew), std::move(img));
		}
	}
}

bool TexLevel::init(int t, TexAtlas *c, LoadTexture &texture, int _level = 0) {
	tex = t;
	level = _level;
	collection = c;
	int side = collection->side;

	// Load full image to get dimensions and tile it
	Image fullImg;
	if(!fullImg.load(texture.filename)) return false;

	texture.width = fullImg.width();
	texture.height = fullImg.height();
	width = fullImg.width();
	height = fullImg.height();

	tilew = (width-1)/side +1;
	tileh = (height-1)/side +1;

	for(int y = 0; y < tileh; y++) {
		for(int x = 0; x < tilew; x++) {
			int sx = x*side;
			int wx = (sx + side > width)? width - sx : side;
			int sy = y*side;
			int wy = (sy + side > height)? height - sy : side;
			int isy = height - (sy + wy);

			Image img = fullImg.copy(Rect(sx, isy, wx, wy));
			img = img.mirrored();
			collection->addImg(TexAtlas::Index(tex, level, x + y*tilew), std::move(img));
		}
	}
	return true;
}

Image TexLevel::read(Rect region) {
	int side = collection->side;

	int sx = region.x/side;
	int sy = region.y/side;
	int ex = (region.x + region.w - 1)/side;
	int ey = (region.y + region.h - 1)/side;

	Image image(region.w, region.h);
	image.fill(0, 0, 0, 255);

	for(int y = sy; y <= ey; y++) {
		for(int x = sx; x <= ex; x++) {
			int id = x + y * tilew;
			TexAtlas::Index index(tex, level, id);

			Image img = collection->getImg(index);

			int tx = std::max(0, x*side - region.x);
			int ty = std::max(0, y*side - region.y);
			int ox = std::max(0, region.x - x*side);
			int oy = std::max(0, region.y - y*side);
			int w = std::min(region.w - tx, side - ox);
			int h = std::min(region.h - ty, side - oy);

			assert(w > 0   && h > 0);
			assert(tx >= 0 && ty >= 0);
			assert(ox >= 0 && oy >= 0);
			assert(w <= region.w);
			assert(h <= region.h);
			Image cropped = img.copy(Rect(ox, oy, w, h));
			image.blit(tx, ty, cropped);
		}
	}
	collection->pruneCache();
	return image;
}

void TexLevel::build(TexLevel &parent) {
	int side = collection->side;
	float scale = collection->scale;
	tex = parent.tex;
	width = (int)floor(parent.width * scale);
	height = (int)floor(parent.height * scale);

	tilew = 1 + (width-1)/side;
	tileh = 1 + (height-1)/side;

	int oside = (int)(side/scale);

	for(int y = 0; y < tileh; y++) {
		for(int x = 0; x < tilew; x++) {
			int w = (x*side + side > width)? width - x*side : side;
			int h = (y*side + side > height)? height - y*side : side;
			int sx = x*oside;
			int sy = y*oside;
			int sw = (sx + oside > parent.width) ? parent.width - sx: oside;
			int sh = (sy + oside > parent.height) ? parent.height - sy: oside;
			Rect region(sx, sy, sw, sh);
			Image img = parent.read(region);
			img = img.scaled(w, h);
			collection->addImg(TexAtlas::Index(tex, level, x + tilew*y), std::move(img));
		}
	}
}


void TexPyramid::init(int tex, TexAtlas *c, Image &texture) {
	collection = c;
	int size = std::max(texture.width(), texture.height());
	int count = 1;
	while(size > collection->side) {
		size /= 2;
		count++;
	}
	levels.resize(count);
	for(int i = 0; i < (int)levels.size(); i++) {
		TexLevel &level = levels[i];
		level.init(tex, collection, texture, i);
		texture = texture.scaled((int)round(texture.width()*collection->scale),
								 (int)round(texture.height()*collection->scale));
	}
}

bool TexPyramid::init(int tex, TexAtlas *c, LoadTexture &file) {
	Image img;
	bool success = img.load(file.filename);
	if(!success)
		return false;
	init(tex, c, img);
	return true;

	// unreachable - kept for reference
	collection = c;
	levels.resize(1);
	TexLevel &level = levels.back();
	return level.init(tex, collection, file);
}

Image TexPyramid::read(int level, Rect region) {
	if(level < 0 || level >= (int)levels.size()) return Image();
	return levels[level].read(region);
}

void TexPyramid::buildLevel(int level) {
	if(levels.size() > (size_t)level) return;
	if(levels.size() != (size_t)level)
		throw std::runtime_error("texture atlas cannot skip levels when building");
	levels.resize(level + 1);
	TexLevel &texlevel = levels.back();
	texlevel.level = level;
	texlevel.collection = collection;
	texlevel.build(levels[level-1]);
}



void TexAtlas::addTextures(std::vector<Image>& textures) {
	pyramids.resize(textures.size());
	for(size_t i = 0; i < pyramids.size(); i++) {
		TexPyramid &py = pyramids[i];
		py.init((int)i, this, textures[i]);
	}
}

bool TexAtlas::addTextures(std::vector<LoadTexture> &textures) {
	pyramids.resize(textures.size());
	for(size_t i = 0; i < pyramids.size(); i++) {
		TexPyramid &py = pyramids[i];
		bool ok = py.init((int)i, this, textures[i]);
		if(!ok) {
			throw std::runtime_error("could not load texture: " + textures[i].filename);
		}
	}
	return true;
}

Image TexAtlas::read(int tex, int level, Rect region) {
	if(tex < 0 || tex >= (int)pyramids.size()) return Image();
	return pyramids[tex].read(level, region);
}

void TexAtlas::addImg(Index index, Image img) {
	cache_size += img.width()*img.height()*4;
	ram[index] = RamData(std::move(img), (uint32_t)access++);
	pruneCache();
}

Image TexAtlas::getImg(Index index) {
	auto it = ram.find(index);
	if(it != ram.end())
		return it->second.image;

	auto dt = disk.find(index);
	if(dt == disk.end())
		throw std::runtime_error("unexpected missing image in disk and ram");

	// Read JPEG data from storage temp file
	std::vector<unsigned char> jpegData(dt->second.size);
	storage.seek(dt->second.offset);
	storage.read(reinterpret_cast<char*>(jpegData.data()), dt->second.size);

	Image img;
	img.loadFromData(jpegData.data(), jpegData.size());
	addImg(index, img);
	return img;
}



void TexAtlas::buildLevel(int level) {
	if(!pyramids.size()) return;
	for(auto &py: pyramids)
		py.buildLevel(level);
}

void TexAtlas::flush(int level) {
	for (auto it = ram.cbegin(); it != ram.cend();) {
		if (it->first.level == level) {
			cache_size -= 4*(it->second.image.width())*(it->second.image.height());
			it = ram.erase(it);
		} else
			++it;
	}
}

void TexAtlas::pruneCache() {
	while(cache_size > cache_max) {
		Index index;
		uint32_t oldest = (uint32_t)access;
		for (auto it = ram.cbegin(); it != ram.cend(); it++) {
			if(it->second.access < oldest) {
				index = it->first;
				oldest = it->second.access;
			}
		}
		auto it = ram.find(index);
		cache_size -= 4*(it->second.image.width())*(it->second.image.height());
		if(disk.find(index) == disk.end()) {
			DiskData d;
			d.offset = storage.size();
			d.w = it->second.image.width();
			d.h = it->second.image.height();

			// Save image as JPEG to memory then write to temp storage
			std::vector<unsigned char> jpegBuf;
			it->second.image.saveToMemory(jpegBuf, quality);

			storage.write(reinterpret_cast<const char*>(jpegBuf.data()), jpegBuf.size());
			d.size = jpegBuf.size();
			disk[index] = d;
		}
		ram.erase(it);
	}
}


