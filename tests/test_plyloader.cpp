/*
 * Tests for PlyLoader — mesh loading from .ply files.
 *
 * These tests use TestArea (single file download) and TestFS (zip archive)
 * to fetch test assets from remote URLs.
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <string>

#include "testarea.h"
#include "testfs.h"
#include "nxsbuild/plyloader.h"
#include "nxsbuild/trianglesoup.h"

namespace fs = std::filesystem;

// Single .ply file (binary or ASCII)
static const std::string PLY_URL         = "https://github.com/DroneDB/test_data/raw/refs/heads/master/3d/brighton.ply";

// Expected values (update after providing real assets)
static const uint32_t EXPECTED_PLY_MIN_VERTICES  = 3;
static const uint32_t EXPECTED_PLY_MIN_TRIANGLES = 1;

// ---------------------------------------------------------------------------
// Fixture: downloads a single .ply via TestArea
// ---------------------------------------------------------------------------
class PlyLoaderTestArea : public ::testing::Test {
protected:
	void SetUp() override {
		area = std::make_unique<TestArea>("ply_loader_test");
	}

	void TearDown() override {
		area.reset();
	}

	std::unique_ptr<TestArea> area;
};

// ---------------------------------------------------------------------------
// TestArea-based tests
// ---------------------------------------------------------------------------

TEST_F(PlyLoaderTestArea, ConstructFromDownloadedPly) {
	fs::path plyPath = area->downloadTestAsset(PLY_URL, "test_model.ply");
	ASSERT_TRUE(fs::exists(plyPath));
	ASSERT_GT(fs::file_size(plyPath), 0u);

	EXPECT_NO_THROW({
		PlyLoader loader(plyPath.string());
	});
}

TEST_F(PlyLoaderTestArea, VertexAndTriangleCounts) {
	fs::path plyPath = area->downloadTestAsset(PLY_URL, "test_model.ply");
	PlyLoader loader(plyPath.string());

	EXPECT_GE(loader.nVertices(), EXPECTED_PLY_MIN_VERTICES)
		<< "Expected at least " << EXPECTED_PLY_MIN_VERTICES << " vertices";
	EXPECT_GE(loader.nTriangles(), EXPECTED_PLY_MIN_TRIANGLES)
		<< "Expected at least " << EXPECTED_PLY_MIN_TRIANGLES << " triangles";
}

TEST_F(PlyLoaderTestArea, GetTriangles) {
	fs::path plyPath = area->downloadTestAsset(PLY_URL, "test_model.ply");
	PlyLoader loader(plyPath.string());

	const uint32_t bufSize = 4096;
	std::vector<Triangle> buffer(bufSize);

	uint32_t totalTris = 0;
	uint32_t read = 0;
	do {
		read = loader.getTriangles(bufSize, buffer.data());
		totalTris += read;
	} while (read > 0);

	EXPECT_GE(totalTris, EXPECTED_PLY_MIN_TRIANGLES);
}

TEST_F(PlyLoaderTestArea, GetVertices) {
	fs::path plyPath = area->downloadTestAsset(PLY_URL, "test_model.ply");
	PlyLoader loader(plyPath.string());

	const uint32_t bufSize = 4096;
	std::vector<Splat> buffer(bufSize);

	uint32_t totalVerts = 0;
	uint32_t read = 0;
	do {
		read = loader.getVertices(bufSize, buffer.data());
		totalVerts += read;
	} while (read > 0);

	EXPECT_GE(totalVerts, EXPECTED_PLY_MIN_VERTICES);
}

TEST_F(PlyLoaderTestArea, BoundingBoxIsValid) {
	fs::path plyPath = area->downloadTestAsset(PLY_URL, "test_model.ply");
	PlyLoader loader(plyPath.string());

	const uint32_t bufSize = 4096;
	std::vector<Triangle> buffer(bufSize);
	while (loader.getTriangles(bufSize, buffer.data()) > 0) {}

	auto &box = loader.box;
	EXPECT_LE(box.min.X(), box.max.X());
	EXPECT_LE(box.min.Y(), box.max.Y());
	EXPECT_LE(box.min.Z(), box.max.Z());
}

TEST_F(PlyLoaderTestArea, TrianglesAreNotDegenerate) {
	fs::path plyPath = area->downloadTestAsset(PLY_URL, "test_model.ply");
	PlyLoader loader(plyPath.string());

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

TEST_F(PlyLoaderTestArea, HasColorsOrNormals) {
	fs::path plyPath = area->downloadTestAsset(PLY_URL, "test_model.ply");
	PlyLoader loader(plyPath.string());

	// At least one of these should be true for a typical PLY
	bool hasExtra = loader.hasColors() || loader.hasNormals() || loader.hasTextures();
	// This is informational — not a hard failure
	if (!hasExtra) {
		std::cout << "[  INFO  ] PLY has no colors, normals, or textures\n";
	}
}
