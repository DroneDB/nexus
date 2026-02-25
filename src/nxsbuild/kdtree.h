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
#ifndef NX_KDTREE_H
#define NX_KDTREE_H

#include <vcg/space/box3.h>
#include "partition.h"
#include "meshloader.h"
#include <vector>
#include <string>
#include <cstdint>

class Stream;
class StreamSoup;
class StreamCloud;
class Mesh;
class TMesh;

class KDCell {
public:
	vcg::Box3f box;
	int split;           //splitting axis: 0, 1,
	float middle;
	int children[2];     //first and second child
	int block;             //bin where triangle are stored.
	double weight = 0.0;

	KDCell(): split(-1), middle(0), block(-1) {
		children[0] = children[1] = -1;
	}
	bool isLeaf() { return children[0] < 0; }
};

class KDTree {
public:
	vcg::Point3f axes[3];
	float ratio;
	std::vector<KDCell> cells;
	std::vector<vcg::Box3f> block_boxes;      //bounding box associated to the blocks (leaf nodes)
	std::vector<LoadTexture> textures;

	KDTree(float adapt = 0.333);
	virtual ~KDTree() {}

	void load(Stream *stream);
	virtual void setMaxMemory(uint64_t m) = 0;
	virtual void clear() = 0;

	void setAxes(vcg::Point3f &x, vcg::Point3f &y, vcg::Point3f &z);
	void setAxesDiagonal();
	void setAxesOrthogonal();

	int nLeaves() { return block_boxes.size(); }
	void lock(Mesh &mesh, int block);
	void lock(TMesh &mesh, int block);
	bool isInNode(uint32_t node, vcg::Point3f &p);
	static bool isIn(vcg::Point3f *axes, vcg::Box3f &box, vcg::Point3f &p); //check if point is in the box (using axes) semiopen box.

protected:
	float adaptive; //from 0 (not adaptive, to 1) fully adaptive.

	virtual uint64_t addBlock() = 0;
	virtual void loadElements(Stream *stream) = 0;
	virtual void findRealMiddle(KDCell &node)  = 0;
	virtual void splitNode(KDCell &node, KDCell &child0, KDCell &child1) = 0;

	void split(int n);
	float findMiddle(KDCell &node);
	bool isIn(vcg::Box3f &box, vcg::Point3f &p); //check if point is in the box (using axes) semiopen box.
	vcg::Box3f computeBox(vcg::Box3f &b);
};

class KDTreeSoup: public VirtualTriangleSoup, public KDTree {
public:
	double current_weight = 0;
	double max_weight = 0;
	float texelWeight = 0.1;   //for computing max  weight per block
	KDTreeSoup(std::string prefix, float adapt = 0.333): VirtualTriangleSoup(prefix), KDTree(adapt) {}
	void setMaxMemory(uint64_t m) { return VirtualTriangleSoup::setMaxMemory(m); }
	void setMaxWeight(uint32_t weight) { max_weight = double(weight); }
	void clear();
	void pushTriangle(Triangle &t);
	double weight(Triangle &t);

protected:
	uint64_t addBlock() { return VirtualTriangleSoup::addBlock(); }
	void loadElements(Stream *stream);
	void findRealMiddle(KDCell &node);
	void splitNode(KDCell &node, KDCell &child0, KDCell &child1);
	static int assign(Triangle &t, uint32_t &mask, vcg::Point3f axis, float middle);
};

class KDTreeCloud: public VirtualVertexCloud, public KDTree {
public:
	KDTreeCloud(std::string prefix, float adapt = 0.333): VirtualVertexCloud(prefix), KDTree(adapt) {}
	void setMaxMemory(uint64_t m) { return VirtualVertexCloud::setMaxMemory(m); }
	void clear();
	void pushVertex(Splat &v);

protected:
	uint64_t addBlock() { return VirtualVertexCloud::addBlock(); }
	void load(StreamSoup &stream);
	void loadElements(Stream *stream);
	void findRealMiddle(KDCell &node);
	void splitNode(KDCell &node, KDCell &child0, KDCell &child1);
};


#endif // NX_KDTREE_H
