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
#ifndef NX_DAG_H
#define NX_DAG_H
#ifdef _MSC_VER

typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;

#else
#include <stdint.h>
#endif

#include <vcg/space/point2.h>
#include <vcg/space/sphere3.h>
#include <vcg/space/ray3.h>

#include "cone.h"
#include "signature.h"

namespace nx {

#define NEXUS_PADDING 256
// Magic is: "Nxs "
struct Header {

	Header(): magic(0x4E787320), version(0), nvert(0), nface(0), n_nodes(0), n_patches(0), n_textures(0) {}
	uint32_t magic;
	uint32_t version;
	uint64_t nvert;            //total number of vertices (at full resolution)
	uint64_t nface;            //total number of faces
	Signature signature;
	uint32_t n_nodes;          //number of nodes in the dag
	uint32_t n_patches;        //number of links (patches) in the dag
	uint32_t n_textures;       //number of textures
	vcg::Sphere3f sphere;      //bounding sphere
};

struct Node {
	uint32_t offset;           //offset on disk (could be moved), granularity NEXUS_PADDING) //use the function
	uint16_t nvert;
	uint16_t nface;
	float error;
	nx::Cone3s cone;           //cone of normals
	vcg::Sphere3f sphere;      //saturated sphere (extraction)
	float tight_radius;        //tight sphere (frustum culling)
	uint32_t first_patch;      //index of the first patch in the array of patches.
	uint32_t last_patch() { return (this + 1)->first_patch; } //this node is ALWAYS inside an array.
	vcg::Sphere3f tightSphere() { return vcg::Sphere3f(sphere.Center(), tight_radius); }

	uint64_t getBeginOffset() { return (uint64_t)offset * (uint64_t) NEXUS_PADDING; }
	uint64_t getEndOffset() { return (this+1)->getBeginOffset(); }
	uint64_t getSize() { return getEndOffset() - getBeginOffset(); }
};

struct Patch {
	uint32_t node;             //destination node
	uint32_t triangle_offset;  //end of the triangles in the node triangle list. //begin from previous patch.
	uint32_t texture;          //index of the texture in the array of textures
};

struct Texture {
	uint32_t offset;
	float matrix[16];
	Texture(): offset(-1) {
		for(int i = 0; i < 16; i++)
			matrix[i] = 0;
	}

	uint64_t getBeginOffset() { return (uint64_t)offset * (uint64_t) NEXUS_PADDING; }
	uint64_t getEndOffset() { return (this+1)->getBeginOffset(); }
	uint64_t getSize() { return getEndOffset() - getBeginOffset(); }
};

/* Number of root nodes of a dag stored as nexus stores it: parents before children
   (roots first) and a sink node last. The sink is not a real node: it terminates the
   array so that last_patch() of the last real node can read its first_patch, and the
   leaves link to it. Roots are the nodes before the smallest node index any root links to.
   Only real nodes are visited, because last_patch() on the sink would read past the array. */
inline uint32_t countDagRoots(Node *nodes, Patch *patches, uint32_t n_nodes) {
	if(n_nodes == 0)
		return 0;
	uint32_t nroots = n_nodes - 1; //the sink is never a root
	for(uint32_t j = 0; j < nroots; j++) {
		for(uint32_t i = nodes[j].first_patch; i < nodes[j].last_patch(); i++)
			if(patches[i].node < nroots)
				nroots = patches[i].node;
	}
	return nroots;
}

}//
#endif // NX_DAG_H
