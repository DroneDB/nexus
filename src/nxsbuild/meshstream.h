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

#include <vector>
#include <string>
#include <cstdint>
#include <vcg/space/box3.h>

#include "trianglesoup.h"
#include "meshloader.h"

class Stream {
public:

	vcg::Box3f box;
	bool has_colors;
	bool has_normals;
	bool has_textures;
	std::vector<LoadTexture> textures;
	vcg::Point3d origin = vcg::Point3d(0, 0, 0);
	vcg::Point3d scale = vcg::Point3d(1, 1, 1);
	std::vector<std::string> colormap;

	Stream();
	virtual ~Stream() {}
	void setVertexQuantization(double q);
	vcg::Box3d getBox(std::vector<std::string> paths);
	void load(std::vector<std::string> paths, std::string material);
	void load(MeshLoader *loader);


	void clear();
	virtual uint64_t size() = 0;
	virtual void setMaxMemory(uint64_t m) = 0;
	virtual bool hasColors() { return has_colors; }
	virtual bool hasNormals() { return has_normals; }
	virtual bool hasTextures() { return has_textures; }

protected:
	std::vector<std::vector<uint64_t> > levels;
	std::vector<uint64_t> order;

	double vertex_quantization;
	uint64_t current_triangle;
	uint64_t current_block;

	MeshLoader *getLoader(std::string file, std::string material);

	virtual void flush() = 0;
	virtual void loadMesh(MeshLoader *loader) = 0;
	virtual void clearVirtual() = 0;
	virtual uint64_t addBlock(uint64_t level) = 0;

	uint64_t getLevel(int64_t index);
	void computeOrder();
};


class StreamSoup: public Stream, public VirtualTriangleSoup {
public:
	StreamSoup(std::string prefix);

	void pushTriangle(Triangle &triangle);
	Soup streamTriangles();
	uint64_t size() { return VirtualTriangleSoup::size(); }
	void setMaxMemory(uint64_t m) { return VirtualTriangleSoup::setMaxMemory(m); }

protected:
	void flush() { VirtualTriangleSoup::flush(); }
	void loadMesh(MeshLoader *loader);
	void clearVirtual();
	uint64_t addBlock(uint64_t level);

};


class StreamCloud: public Stream, public VirtualVertexCloud {
public:
	StreamCloud(std::string prefix);

	void pushVertex(Splat &ertex);
	Cloud streamVertices();
	uint64_t size() { return VirtualVertexCloud::size(); }
	void setMaxMemory(uint64_t m) { return VirtualVertexCloud::setMaxMemory(m); }

protected:
	void flush() { VirtualVertexCloud::flush(); }
	void loadMesh(MeshLoader *loader);
	void clearVirtual();
	uint64_t addBlock(uint64_t level);

};

#endif // NX_MESHSTREAM_H
