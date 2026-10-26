#include <algorithm>
#include <assert.h>
#include <math.h>
#include <iostream>
#include "../common/error.h"
#include "../common/pathutil.h"
#include "imageio.h"
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
			Image img = texture.copy(sx, isy, wx, wy).mirrored();
			collection->addImg(TexAtlas::Index(tex, level, x + y*tilew), img);
		}
	}
}

Image TexLevel::read(int rx, int ry, int rw, int rh) {
	int side = collection->side;

	int sx = rx/side;
	int sy = ry/side;
	int ex = (rx + rw - 1)/side; //last pixel needed
	int ey = (ry + rh - 1)/side;

	Image image(rw, rh);

	for(int y = sy; y <= ey; y++) {
		for(int x = sx; x <= ex; x++) {
			int id = x + y * tilew;
			TexAtlas::Index index(tex, level, id);

			Image img = collection->getImg(index);

			int tx = std::max(0, x*side - rx); //target
			int ty = std::max(0, y*side - ry);
			int ox = std::max(0, rx - x*side); //origins
			int oy = std::max(0, ry - y*side);
			int w = std::min(rw -tx, side - ox);
			int h = std::min(rh -ty, side - oy);

			assert(w > 0   && h > 0);
			assert(tx >= 0 && ty >= 0);
			assert(ox >= 0 && oy >= 0);
			assert(w <= rw);
			assert(h <= rh);
			image.draw(tx, ty, img, ox, oy, w, h);
		}
	}
	collection->pruneCache();
	return image;
}
//TODO could we speed up this process?
//in theory we should be able to process the file just once
//reading line by line and fill all the levels at the same time
//keep 2 lines in float for averaging, for each first line of tiles of each level.
//in qimage reader use setscaledcliprect
void TexLevel::build(TexLevel &parent, float scale) {
	int side = collection->side;
	tex = parent.tex;
	//at least a pixel: a texture keeps halving for as many levels as the model has
	width = std::max(1, int(floor(parent.width * scale)));
	height = std::max(1, int(floor(parent.height * scale)));

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
			Image img = parent.read(sx, sy, sw, sh);
			img = img.downscaled(w, h);
			collection->addImg(TexAtlas::Index(tex, level, x + tilew*y), img);
		}
	}
}



//Only level zero: each level above it is built when the builder reaches it, at the scale that
//level needs (see NexusBuilder::create).
void TexPyramid::init(int tex, TexAtlas *c, Image &texture) {
	collection = c;
	levels.resize(1);
	levels[0].init(tex, collection, texture, 0);
}

bool TexPyramid::init(int tex, TexAtlas *c, LoadTexture &file) {
	std::string error;
	Image img = loadImage(file.filename, error);
	if(img.isNull())
		throw nx::Error("could not load texture: " + file.filename + ": " + error);
	//the kd-tree weighs each triangle by the texels it covers, from these sizes: left at 0 every
	//triangle weighs nothing and the nodes are cut by triangle count alone, however much texture
	//they hold
	file.width = uint32_t(img.width());
	file.height = uint32_t(img.height());
	init(tex, c, img);
	return true;
}

Image TexPyramid::read(int level, int x, int y, int w, int h) {
	if(level < 0 || level >= (int)levels.size()) return Image();
	return levels[level].read(x, y, w, h);
}

void TexPyramid::buildLevel(int level, float scale) {
	if(levels.size() > (size_t)level) return;
	if(levels.size() != (size_t)level)
		throw nx::Error("texture atlas cannot skip levels when building");
	levels.resize(level + 1);
	TexLevel &texlevel = levels.back();
	texlevel.level = level;
	texlevel.collection = collection;
	texlevel.build(levels[level-1], scale);
}



void TexAtlas::addTextures(std::vector<Image>& textures) {
	pyramids.resize(textures.size());
	for(size_t i = 0; i < pyramids.size(); i++) {
		TexPyramid &py = pyramids[i];
		py.init(i, this, textures[i]);
	}
}

bool TexAtlas::addTextures(std::vector<LoadTexture> &textures) {
	pyramids.resize(textures.size());
	for(size_t i = 0; i < pyramids.size(); i++) {
		TexPyramid &py = pyramids[i];
		bool ok = py.init(i, this, textures[i]);
		if(!ok) {
			throw nx::Error("could not load texture: " + textures[i].filename);
		}
	}
	return true;
}

Image TexAtlas::read(int tex, int level, int x, int y, int w, int h) {
	if(tex < 0 || tex >= (int)pyramids.size()) return Image();
	return pyramids[tex].read(level, x, y, w, h);
}

void TexAtlas::addImg(Index index, Image img) {
	//cout << "Adding: tex: " << index.tex << " level: " << index.level << " index: " << index.index << endl;
	std::lock_guard<std::mutex> locker(cache_lock);
	addImgLocked(index, img);
}

//assumes cache_lock is held
void TexAtlas::addImgLocked(Index index, const Image &img) {
	auto existing = ram.find(index);
	if(existing != ram.end()) {
		//decoded concurrently -> refresh in place
		cache_size -= 4*existing->second.image.width()*existing->second.image.height();
		cache_size += 4*img.width()*img.height();
		existing->second.image = img;
		lru_list.splice(lru_list.begin(), lru_list, existing->second.lru);
	} else {
		cache_size += 4*img.width()*img.height();
		lru_list.push_front(index);
		RamData d(img);
		d.lru = lru_list.begin();
		ram[index] = d;
	}
	pruneCacheLocked();
}

Image TexAtlas::getImg(Index index) {
	DiskData d;
	std::vector<unsigned char> bytes;

	{
		std::lock_guard<std::mutex> locker(cache_lock);
		const auto it = ram.find(index);
		if(it != ram.end()) {
			//mark most-recently-used
			lru_list.splice(lru_list.begin(), lru_list, it->second.lru);
			return it->second.image;
		}

		//cache miss, load from disk
		const auto dt = disk.find(index);
		if(dt == disk.end())
			throw nx::Error("unespected missing image in disk and ram");
		d = dt->second;
		bytes.resize(d.size);
		storage.read(d.offset, bytes.data(), bytes.size());
	}

	//expensive decode without lock
	std::string error;
	Image img = decodeJpeg(bytes.data(), bytes.size(), error);
	if(img.isNull() || uint32_t(img.width()) != d.w || uint32_t(img.height()) != d.h)
		throw nx::Error("could not read back a texture tile: " + error);

	{
		std::lock_guard<std::mutex> locker(cache_lock);
		//another thread may have won the race meanwhile
		auto it = ram.find(index);
		if(it != ram.end()) {
			lru_list.splice(lru_list.begin(), lru_list, it->second.lru);
			return it->second.image;
		}
		addImgLocked(index, img);
		return img;
	}
}



void TexAtlas::buildLevel(int level, float scale) {
	if(!pyramids.size()) return;
	for(auto &py: pyramids)
		py.buildLevel(level, scale);
}

//do not store it in temporary file, we are throwing away this level.
void TexAtlas::flush(int level) {
	std::lock_guard<std::mutex> locker(cache_lock);
	for (auto it = ram.cbegin(); it != ram.cend();) {
		if (it->first.level == level) {
			cache_size -= 4*(it->second.image.width())*(it->second.image.height());
			lru_list.erase(it->second.lru);
			it = ram.erase(it);
		} else
			++it;
	}
}

void TexAtlas::pruneCache() {
	std::lock_guard<std::mutex> locker(cache_lock);
	pruneCacheLocked();
}

//assumes cache_lock is held
void TexAtlas::pruneCacheLocked() {
	while(cache_size > cache_max && !lru_list.empty()) {
		Index index = lru_list.back();
		auto it = ram.find(index);
		if(it == ram.end()) {
			//should not happen, keep LRU consistent
			lru_list.pop_back();
			continue;
		}
		cache_size -= 4*(it->second.image.width())*(it->second.image.height());
		if(disk.find(index) == disk.end()) {
			if(!storage.isOpen())
				storage.create(storage_prefix.empty() ? joinPath(temporaryDirectory(), "nexus_textures") : storage_prefix);
			DiskData d;
			d.w = it->second.image.width();
			d.h = it->second.image.height();
			//TODO: this could be made not under the lock as well
			const std::vector<unsigned char> jpeg = encodeJpeg(it->second.image, quality, false, false);
			d.offset = storage.append(jpeg.data(), jpeg.size());
			d.size = jpeg.size();
			disk[index] = d;
		}
		ram.erase(it);
		lru_list.pop_back();
	}
}


