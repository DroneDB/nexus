#ifndef TEXPYRAMID_H
#define TEXPYRAMID_H

#include <list>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "../common/scratchfile.h"
#include "image.h"
#include "meshloader.h"

namespace nx {

class TexAtlas;

//split and store in temporary files large textures for better access
class TexLevel {
public:
	TexAtlas *collection;
	int tex, level;
	int width, height;
	int tilew, tileh;              //how many tiles to make up side.
	//std::vector<int64_t> offsets; //where each tile starts in the cache.

	void init(int tex, TexAtlas *c, Image& texture, int _level);
	Image read(int x, int y, int w, int h);
	//this level, scale times the size of parent
	void build(TexLevel &parent, float scale);
	//void build(Image img);
};

//manage a pyramid of splitted textured
class TexPyramid {
public:
	TexAtlas *collection;
	std::vector<TexLevel> levels;

	void init(int tex, TexAtlas *c, Image &texture);
	bool init(int tex, TexAtlas *c, LoadTexture &file);
	Image read(int level, int x, int y, int w, int h);
	void buildLevel(int level, float scale);
};

//Collection of tex pyramids, level 0 is biggest one (bottom).

class TexAtlas {
public:
	struct Index {
		int tex;
		int level;
		int index;
		Index() {}
		Index(int t, int l, int i): tex(t), level(l), index(i) {}
		bool operator<(const Index &i) const {
			if(tex == i.tex) {
				if(level == i.level)
					return index < i.index;
				return level < i.level;
			}
			return tex < i.tex;
		}
		bool operator==(const Index &i) {
			return level == i.level && index == i.index && tex == i.tex;
		}
	};
	struct RamData {
		Image image;
		std::list<Index>::iterator lru;   //position in the LRU list (front = most recent)
		RamData() {}
		RamData(Image img): image(img) {}
	};
	struct DiskData {
		uint64_t offset;
		uint64_t size;
		uint32_t w,h;
	};

	//the side of a tile: reading a rectangle loads, and decodes when spilled to disk, every tile it
	//touches, so the tiles stay small next to the few hundred texels a node reads
	const int side = 1024;
	std::vector<TexPyramid> pyramids;
	//the side of a level relative to the one below when the builder simplifies the triangles:
	//sqrt of the simplification ratio, so each triangle keeps its texels
	float scale = 0.70710678;
	int quality = 92;
	uint64_t cache_max = 2000000000;
	uint64_t cache_size = 0;

	TexAtlas() {}

	void addTextures(std::vector<Image>& textures);
	//will actually fill width and height information
	bool addTextures(std::vector<LoadTexture> &filenames);
	Image read(int tex, int level, int x, int y, int w, int h);
	//every texture at this level, scale times the size of the level below
	void buildLevel(int level, float scale);

	int width(int tex, int level) { 
		if(tex < 0 || tex >= (int)pyramids.size()) return 1;
		if(level < 0 || level >= (int)pyramids[tex].levels.size()) return 1;
		return pyramids[tex].levels[level].width; 
	}
	int height(int tex, int level) { 
		if(tex < 0 || tex >= (int)pyramids.size()) return 1;
		if(level < 0 || level >= (int)pyramids[tex].levels.size()) return 1;
		return pyramids[tex].levels[level].height; 
	}
	void flush(int level);
	void pruneCache();
	void addImg(Index index, Image img);
	Image getImg(Index index);

	std::map<Index, RamData> ram;
	std::map<Index, DiskData> disk;
	std::list<Index> lru_list;        //front = most-recently-used, back = LRU eviction candidate

	//The tiles that do not fit in RAM are kept here as JPEG images, in a temporary file created
	//next to storage_prefix (the system's temporary folder when it is empty) the first time
	//one is needed.
	nx::ScratchFile storage;
	std::string storage_prefix;

	//cache_lock guards ram/disk/lru_list/cache_size/storage so tiles can be
	//decoded concurrently by the texture-extraction worker threads.
	std::mutex cache_lock;

private:
	//helpers that assume cache_lock is already held by the caller
	void addImgLocked(Index index, const Image &img);
	void pruneCacheLocked();
};

} //namespace
#endif // TEXPYRAMID_H

