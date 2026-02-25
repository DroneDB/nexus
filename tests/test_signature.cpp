/*
 * Tests for nx::Signature, nx::Attribute, nx::Element
 */
#include <gtest/gtest.h>
#include "common/signature.h"

TEST(AttributeTest, DefaultIsNull) {
	nx::Attribute attr;
	EXPECT_TRUE(attr.isNull());
	EXPECT_EQ(attr.size(), 0);
}

TEST(AttributeTest, TypeAndSize) {
	// FLOAT x 3 = 12 bytes (typical for position)
	nx::Attribute a(nx::Attribute::FLOAT, 3);
	EXPECT_FALSE(a.isNull());
	EXPECT_EQ(a.size(), 12);

	// UNSIGNED_SHORT x 1 = 2 bytes
	nx::Attribute b(nx::Attribute::UNSIGNED_SHORT, 1);
	EXPECT_EQ(b.size(), 2);

	// DOUBLE x 2 = 16 bytes
	nx::Attribute c(nx::Attribute::DOUBLE, 2);
	EXPECT_EQ(c.size(), 16);

	// BYTE x 4 = 4 bytes (color)
	nx::Attribute d(nx::Attribute::BYTE, 4);
	EXPECT_EQ(d.size(), 4);
}

TEST(VertexElementTest, HasComponents) {
	nx::VertexElement v;
	EXPECT_FALSE(v.hasNormals());
	EXPECT_FALSE(v.hasColors());
	EXPECT_FALSE(v.hasTextures());

	// Set coord (FLOAT x 3)
	v.setComponent(nx::VertexElement::COORD, nx::Attribute(nx::Attribute::FLOAT, 3));
	EXPECT_EQ(v.size(), 12);

	// Add normals (SHORT x 3 = 6 bytes)
	v.setComponent(nx::VertexElement::NORM, nx::Attribute(nx::Attribute::SHORT, 3));
	EXPECT_TRUE(v.hasNormals());
	EXPECT_EQ(v.size(), 18);

	// Add colors (UNSIGNED_BYTE x 4 = 4 bytes)
	v.setComponent(nx::VertexElement::COLOR, nx::Attribute(nx::Attribute::UNSIGNED_BYTE, 4));
	EXPECT_TRUE(v.hasColors());
	EXPECT_EQ(v.size(), 22);

	// Add textures (FLOAT x 2 = 8 bytes)
	v.setComponent(nx::VertexElement::TEX, nx::Attribute(nx::Attribute::FLOAT, 2));
	EXPECT_TRUE(v.hasTextures());
	EXPECT_EQ(v.size(), 30);
}

TEST(FaceElementTest, HasComponents) {
	nx::FaceElement f;
	EXPECT_FALSE(f.hasIndex());
	EXPECT_FALSE(f.hasNormals());

	f.setComponent(nx::FaceElement::INDEX, nx::Attribute(nx::Attribute::UNSIGNED_SHORT, 3));
	EXPECT_TRUE(f.hasIndex());
	EXPECT_EQ(f.size(), 6);
}

TEST(SignatureTest, DefaultFlags) {
	nx::Signature sig;
	EXPECT_EQ(sig.flags, 0u);
	EXPECT_FALSE(sig.isCompressed());
}

TEST(SignatureTest, SetFlags) {
	nx::Signature sig;

	sig.setFlag(nx::Signature::CORTO);
	EXPECT_TRUE(sig.isCompressed());
	EXPECT_EQ(sig.flags & nx::Signature::CORTO, static_cast<uint32_t>(nx::Signature::CORTO));

	sig.unsetFlag(nx::Signature::CORTO);
	EXPECT_FALSE(sig.isCompressed());

	sig.setFlag(nx::Signature::MECO);
	EXPECT_TRUE(sig.isCompressed());
}

TEST(SignatureTest, DeepzoomFlag) {
	nx::Signature sig;
	EXPECT_FALSE(sig.isDeepzoom());
	sig.setFlag(nx::Signature::DEEPZOOM);
	EXPECT_TRUE(sig.isDeepzoom());
}

TEST(SignatureTest, VertexAndFace) {
	nx::Signature sig;
	sig.vertex.setComponent(nx::VertexElement::COORD, nx::Attribute(nx::Attribute::FLOAT, 3));
	sig.vertex.setComponent(nx::VertexElement::NORM, nx::Attribute(nx::Attribute::SHORT, 3));
	sig.face.setComponent(nx::FaceElement::INDEX, nx::Attribute(nx::Attribute::UNSIGNED_SHORT, 3));

	EXPECT_TRUE(sig.vertex.hasNormals());
	EXPECT_TRUE(sig.face.hasIndex());
	// Total vertex size: 12 (coord) + 6 (norm) = 18
	EXPECT_EQ(sig.vertex.size(), 18);
}
