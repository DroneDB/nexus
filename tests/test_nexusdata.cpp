/*
 * Tests for NexusData structures (Header, Node, Patch, Signature layout, NodeData)
 */
#include <gtest/gtest.h>
#include <cstring>
#include "common/nexusdata.h"

TEST(HeaderTest, DefaultValues) {
	nx::Header h;
	EXPECT_EQ(h.magic, 0x4E787320u);  // "Nxs "
	EXPECT_EQ(h.version, 0u);
	EXPECT_EQ(h.nvert, 0u);
	EXPECT_EQ(h.nface, 0u);
	EXPECT_EQ(h.n_nodes, 0u);
	EXPECT_EQ(h.n_patches, 0u);
	EXPECT_EQ(h.n_textures, 0u);
}

TEST(HeaderTest, MagicBytes) {
	nx::Header h;
	// Verify magic encodes "Nxs " in little-endian
	const char *bytes = reinterpret_cast<const char *>(&h.magic);
	EXPECT_EQ(bytes[0], ' ');
	EXPECT_EQ(bytes[1], 's');
	EXPECT_EQ(bytes[2], 'x');
	EXPECT_EQ(bytes[3], 'N');
}

TEST(NodeTest, OffsetCalculation) {
	// Create 2 nodes in an array (last_patch relies on this+1)
	nx::Node nodes[2];
	nodes[0].offset = 1;
	nodes[0].nvert = 100;
	nodes[0].nface = 200;
	nodes[0].first_patch = 0;
	nodes[1].offset = 3;
	nodes[1].first_patch = 5;

	EXPECT_EQ(nodes[0].getBeginOffset(), 1u * NEXUS_PADDING);
	EXPECT_EQ(nodes[0].getEndOffset(), 3u * NEXUS_PADDING);
	EXPECT_EQ(nodes[0].getSize(), 2u * NEXUS_PADDING);
	EXPECT_EQ(nodes[0].last_patch(), 5u);
}

TEST(PatchTest, Layout) {
	nx::Patch p;
	p.node = 42;
	p.triangle_offset = 128;
	p.texture = 3;

	EXPECT_EQ(p.node, 42u);
	EXPECT_EQ(p.triangle_offset, 128u);
	EXPECT_EQ(p.texture, 3u);
}

TEST(NodeDataTest, FacePointer) {
	// Build a minimal memory layout: positions only + face indices
	nx::Signature sig;
	sig.vertex.setComponent(nx::VertexElement::COORD, nx::Attribute(nx::Attribute::FLOAT, 3));
	sig.face.setComponent(nx::FaceElement::INDEX, nx::Attribute(nx::Attribute::UNSIGNED_SHORT, 3));

	uint32_t nvert = 4;
	size_t vertSize = nvert * sizeof(float) * 3;    // 48 bytes
	size_t faceSize = 2 * 3 * sizeof(uint16_t);      // 12 bytes

	std::vector<char> mem(vertSize + faceSize, 0);

	// Write face data
	uint16_t *facePtr = reinterpret_cast<uint16_t *>(mem.data() + vertSize);
	facePtr[0] = 0; facePtr[1] = 1; facePtr[2] = 2;
	facePtr[3] = 1; facePtr[4] = 2; facePtr[5] = 3;

	nx::NodeData nd;
	nd.memory = mem.data();

	uint16_t *result = nd.faces(sig, nvert);
	EXPECT_EQ(result[0], 0);
	EXPECT_EQ(result[1], 1);
	EXPECT_EQ(result[2], 2);
	EXPECT_EQ(result[3], 1);
	EXPECT_EQ(result[4], 2);
	EXPECT_EQ(result[5], 3);
}

TEST(NodeDataTest, CoordsPointer) {
	float positions[] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };
	nx::NodeData nd;
	nd.memory = reinterpret_cast<char *>(positions);

	vcg::Point3f *coords = nd.coords();
	EXPECT_FLOAT_EQ(coords[0][0], 1.0f);
	EXPECT_FLOAT_EQ(coords[0][1], 2.0f);
	EXPECT_FLOAT_EQ(coords[0][2], 3.0f);
	EXPECT_FLOAT_EQ(coords[1][0], 4.0f);
}

TEST(NexusDataTest, DefaultConstruction) {
	nx::NexusData data;
	EXPECT_EQ(data.header.magic, 0x4E787320u);
	EXPECT_EQ(data.nodes, nullptr);
	EXPECT_EQ(data.patches, nullptr);
	EXPECT_EQ(data.textures, nullptr);
}
