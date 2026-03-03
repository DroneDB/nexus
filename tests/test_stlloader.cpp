/*
 * Tests for STLLoader — mesh loading from .stl files (ASCII and binary).
 *
 * These tests use TestArea (single file download) and TestFS (zip archive)
 * to fetch test assets from remote URLs.
 *
 */
#include <gtest/gtest.h>
#include <filesystem>
#include <string>

#include "testarea.h"
#include "testfs.h"
#include "nxsbuild/stlloader.h"
#include "nxsbuild/trianglesoup.h"

namespace fs = std::filesystem;


// ASCII .stl file
static const std::string STL_ASCII_URL   = "https://github.com/DroneDB/test_data/raw/refs/heads/master/3d/stl_ascii.stl";
// Binary .stl file
static const std::string STL_BINARY_URL  = "https://github.com/DroneDB/test_data/raw/refs/heads/master/3d/stl_binary.stl";

// Expected values (update after providing real assets)
static const uint32_t EXPECTED_STL_MIN_TRIANGLES = 1;

// ---------------------------------------------------------------------------
// Fixture: downloads a single .stl via TestArea
// ---------------------------------------------------------------------------
class STLLoaderTestArea : public ::testing::Test {
protected:
	void SetUp() override {
		area = std::make_unique<TestArea>("stl_loader_test");
	}

	void TearDown() override {
		area.reset();
	}

	std::unique_ptr<TestArea> area;
};

// ---------------------------------------------------------------------------
// ASCII STL tests
// ---------------------------------------------------------------------------

TEST_F(STLLoaderTestArea, ConstructFromAsciiStl) {
	fs::path stlPath = area->downloadTestAsset(STL_ASCII_URL, "test_ascii.stl");
	ASSERT_TRUE(fs::exists(stlPath));
	ASSERT_GT(fs::file_size(stlPath), 0u);

	EXPECT_NO_THROW({
		STLLoader loader(stlPath.string());
	});
}

TEST_F(STLLoaderTestArea, ReadAsciiTriangles) {
	fs::path stlPath = area->downloadTestAsset(STL_ASCII_URL, "test_ascii.stl");
	STLLoader loader(stlPath.string());

	const uint32_t bufSize = 4096;
	std::vector<Triangle> buffer(bufSize);

	uint32_t totalTris = 0;
	uint32_t read = 0;
	do {
		read = loader.getTriangles(bufSize, buffer.data());
		totalTris += read;
	} while (read > 0);

	EXPECT_GE(totalTris, EXPECTED_STL_MIN_TRIANGLES)
		<< "Expected at least " << EXPECTED_STL_MIN_TRIANGLES << " triangles from ASCII STL";
}

// ---------------------------------------------------------------------------
// Binary STL tests
// ---------------------------------------------------------------------------

TEST_F(STLLoaderTestArea, ConstructFromBinaryStl) {
	fs::path stlPath = area->downloadTestAsset(STL_BINARY_URL, "test_binary.stl");
	ASSERT_TRUE(fs::exists(stlPath));
	ASSERT_GT(fs::file_size(stlPath), 0u);

	EXPECT_NO_THROW({
		STLLoader loader(stlPath.string());
	});
}

TEST_F(STLLoaderTestArea, BinaryTriangleCount) {
	fs::path stlPath = area->downloadTestAsset(STL_BINARY_URL, "test_binary.stl");
	STLLoader loader(stlPath.string());

	// Binary STL knows the triangle count upfront
	EXPECT_GE(loader.nTriangles(), EXPECTED_STL_MIN_TRIANGLES);
}

TEST_F(STLLoaderTestArea, ReadBinaryTriangles) {
	fs::path stlPath = area->downloadTestAsset(STL_BINARY_URL, "test_binary.stl");
	STLLoader loader(stlPath.string());

	const uint32_t bufSize = 4096;
	std::vector<Triangle> buffer(bufSize);

	uint32_t totalTris = 0;
	uint32_t read = 0;
	do {
		read = loader.getTriangles(bufSize, buffer.data());
		totalTris += read;
	} while (read > 0);

	EXPECT_GE(totalTris, EXPECTED_STL_MIN_TRIANGLES)
		<< "Expected at least " << EXPECTED_STL_MIN_TRIANGLES << " triangles from binary STL";
}

TEST_F(STLLoaderTestArea, BoundingBoxIsValid) {
	fs::path stlPath = area->downloadTestAsset(STL_BINARY_URL, "test_binary.stl");
	STLLoader loader(stlPath.string());

	const uint32_t bufSize = 4096;
	std::vector<Triangle> buffer(bufSize);
	while (loader.getTriangles(bufSize, buffer.data()) > 0) {}

	auto &box = loader.box;
	EXPECT_LE(box.min.X(), box.max.X());
	EXPECT_LE(box.min.Y(), box.max.Y());
	EXPECT_LE(box.min.Z(), box.max.Z());
}

TEST_F(STLLoaderTestArea, StlHasNoColorsNormalsTextures) {
	fs::path stlPath = area->downloadTestAsset(STL_BINARY_URL, "test_binary.stl");
	STLLoader loader(stlPath.string());

	// STL format does not carry per-vertex colors, normals (as loader attributes), or textures
	EXPECT_FALSE(loader.hasColors());
	EXPECT_FALSE(loader.hasNormals());
	EXPECT_FALSE(loader.hasTextures());
}

TEST_F(STLLoaderTestArea, TrianglesAreNotDegenerate) {
	fs::path stlPath = area->downloadTestAsset(STL_BINARY_URL, "test_binary.stl");
	STLLoader loader(stlPath.string());

	const uint32_t bufSize = 1024;
	std::vector<Triangle> buffer(bufSize);
	uint32_t read = loader.getTriangles(bufSize, buffer.data());

	uint32_t degenerateCount = 0;
	for (uint32_t i = 0; i < read; ++i) {
		if (buffer[i].isDegenerate())
			++degenerateCount;
	}

	EXPECT_LT(degenerateCount, read)
		<< "All triangles are degenerate — loader may be broken";
}