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
#ifndef NX_MESHSTREAM_H
#define NX_MESHSTREAM_H

#include <memory>
#include <string>
#include <vector>
#include <vcg/space/box3.h>

#include "trianglesoup.h"
#include "meshloader.h"

/* triangles are organized in levels, each one with half the triangle of the lower one
   we have a buffer for each level, when filled it gets written to disk.
   I need a vector keeping track of how much full each level is
   and a  vector of vectors holding which blocks belongs to which level
   */

class Stream {
public:

	vcg::Box3f box;
	bool has_colors;
	bool has_normals;
	bool has_textures;
	std::vector<LoadTexture> textures;
	vcg::Point3d origin = vcg::Point3d(0, 0, 0);
	vcg::Point3d scale = vcg::Point3d(1, 1, 1);
	std::vector<std::string> colormap; //used to convert a value into a color, .ts only: the property and the palette

	//cachePrefix: where the loaders that need a scratch file put it (see VirtualMemory), the prefix
	//of the stream's own scratch file
	explicit Stream(const std::string &cachePrefix);
	virtual ~Stream() {}
	void setVertexQuantization(double q);
	vcg::Box3d getBox(const std::vector<std::string> &paths);
	void load(const std::vector<std::string> &paths, const std::string &material);
	void load(MeshLoader *loader); //texture filenames must have the correct (relative or absolute) path!


	//return a block of triangles. The buffer is valid until next call to getTriangles. Return null when finished
	void clear();
	virtual uint64_t size() = 0;
	virtual void setMaxMemory(uint64_t m) = 0;
	virtual bool hasColors() { return has_colors; }
	virtual bool hasNormals() { return has_normals; }
	virtual bool hasTextures() { return has_textures; }

protected:
	std::vector<std::vector<uint64_t> > levels; //for each level the list of blocks
	std::vector<uint64_t> order;          //order of the block for streaming

	std::string cache_prefix;
	double vertex_quantization; //a power of 2.
	uint64_t current_triangle;   //used both for loading and streaming
	uint64_t current_block;

	std::unique_ptr<MeshLoader> getLoader(const std::string &file, const std::string &material);
		
	virtual void flush() = 0;
	virtual void loadMesh(MeshLoader *loader) = 0;
	virtual void clearVirtual() = 0; //clear the virtualtrianglesoup or virtualtrianglebin
	virtual uint64_t addBlock(uint64_t level) = 0; //return index of block added

	uint64_t getLevel(int64_t index);
	void computeOrder();
};


class StreamSoup: public Stream, public VirtualTriangleSoup {
public:
	StreamSoup(const std::string &prefix);

	void pushTriangle(Triangle &triangle);
	//return a block of triangles. The buffer is valid until next call to getTriangles. Return null when finished
	Soup streamTriangles();
	uint64_t size() { return VirtualTriangleSoup::size(); }
	void setMaxMemory(uint64_t m) { return VirtualTriangleSoup::setMaxMemory(m); }
	//estimated number of distinct (position, texcoord, texture) corners in the loaded meshes (HyperLogLog, ~1% error)
	double estimatedUvVertices() const;

protected:    
	void flush() { VirtualTriangleSoup::flush(); }
	void loadMesh(MeshLoader *loader);
	void countUvVertex(const Vertex &v, int tex);
	//the top UvVerticesHllBits of the hash pick a register, which keeps the longest run of leading zeros seen in the remaining bits
	//error ~1.04/sqrt(registers), which for 14 is ~1%
	static constexpr auto UvVerticesHllBits = 14;
	//2^UvVerticesHllBits HyperLogLog registers
	std::vector<uint8_t> uv_vertices_hll = std::vector<uint8_t>(1<<UvVerticesHllBits, 0);
	void clearVirtual();
	uint64_t addBlock(uint64_t level); //return index of block added

};


class StreamCloud: public Stream, public VirtualVertexCloud {
public:
	StreamCloud(const std::string &prefix);

	void pushVertex(Splat &ertex);
	//return a block of triangles. The buffer is valid until next call to getTriangles. Return null when finished
	Cloud streamVertices();
	uint64_t size() { return VirtualVertexCloud::size(); }
	void setMaxMemory(uint64_t m) { return VirtualVertexCloud::setMaxMemory(m); }

protected:
	void flush() { VirtualVertexCloud::flush(); }
	void loadMesh(MeshLoader *loader);
	void clearVirtual();
	uint64_t addBlock(uint64_t level); //return index of block added

};

#endif // NX_MESHSTREAM_H
