/*
 * Tests for PlyLoader — mesh loading from .ply files.
 *
 * These tests use TestArea (single file download) and TestFS (zip archive)
 * to fetch test assets from remote URLs.
 *
 * ============================================================================
 * TODO (team): Replace the placeholder URLs below with real URLs pointing to
 *              actual .ply / .zip test assets, then remove the DISABLED_
 *              prefix from the test names to activate them.
 * ============================================================================
 */
#include <gtest/gtest.h>
#include <filesystem>
#include <string>

#include "testarea.h"
#include "testfs.h"
#include "nxsbuild/plyloader.h"
#include "nxsbuild/trianglesoup.h"

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Placeholder URLs — replace with real URLs when test assets are available
// ---------------------------------------------------------------------------

// Single .ply file (binary or ASCII)
static const std::string PLY_URL         = "https://example.com/TODO_INSERT_URL/test_model.ply";
// Zip archive containing one or more .ply files
static const std::string PLY_ARCHIVE_URL = "https://example.com/TODO_INSERT_URL/test_ply_archive.zip";

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

TEST_F(PlyLoaderTestArea, DISABLED_ConstructFromDownloadedPly) {
	fs::path plyPath = area->downloadTestAsset(PLY_URL, "test_model.ply");
	ASSERT_TRUE(fs::exists(plyPath));
	ASSERT_GT(fs::file_size(plyPath), 0u);

	EXPECT_NO_THROW({
		PlyLoader loader(plyPath.string());
	});
}

TEST_F(PlyLoaderTestArea, DISABLED_VertexAndTriangleCounts) {
	fs::path plyPath = area->downloadTestAsset(PLY_URL, "test_model.ply");
	PlyLoader loader(plyPath.string());

	EXPECT_GE(loader.nVertices(), EXPECTED_PLY_MIN_VERTICES)
		<< "Expected at least " << EXPECTED_PLY_MIN_VERTICES << " vertices";
	EXPECT_GE(loader.nTriangles(), EXPECTED_PLY_MIN_TRIANGLES)
		<< "Expected at least " << EXPECTED_PLY_MIN_TRIANGLES << " triangles";
}

TEST_F(PlyLoaderTestArea, DISABLED_GetTriangles) {
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

TEST_F(PlyLoaderTestArea, DISABLED_GetVertices) {
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

TEST_F(PlyLoaderTestArea, DISABLED_BoundingBoxIsValid) {
	fs::path plyPath = area->downloadTestAsset(PLY_URL, "test_model.ply");
	PlyLoader loader(plyPath.string());

	const uint32_t bufSize = 4096;
	std::vector<Triangle> buffer(bufSize);
	while (loader.getTriangles(bufSize, buffer.data()) > 0) {}

	auto &box = loader.box;
	EXPECT_LE(box.min[0], box.max[0]);
	EXPECT_LE(box.min[1], box.max[1]);
	EXPECT_LE(box.min[2], box.max[2]);
}

TEST_F(PlyLoaderTestArea, DISABLED_TrianglesAreNotDegenerate) {
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

TEST_F(PlyLoaderTestArea, DISABLED_HasColorsOrNormals) {
	fs::path plyPath = area->downloadTestAsset(PLY_URL, "test_model.ply");
	PlyLoader loader(plyPath.string());

	// At least one of these should be true for a typical PLY
	bool hasExtra = loader.hasColors() || loader.hasNormals() || loader.hasTextures();
	// This is informational — not a hard failure
	if (!hasExtra) {
		std::cout << "[  INFO  ] PLY has no colors, normals, or textures\n";
	}
}

// ---------------------------------------------------------------------------
// TestFS-based tests (zip archive)
// ---------------------------------------------------------------------------

TEST(PlyLoaderTestFS, DISABLED_LoadFromZipArchive) {
	TestFS testFs(PLY_ARCHIVE_URL);

	fs::path plyFile;
	for (auto &entry : fs::recursive_directory_iterator(testFs.testFolder)) {
		if (entry.path().extension() == ".ply") {
			plyFile = entry.path();
			break;
		}
	}
	ASSERT_FALSE(plyFile.empty()) << "No .ply file found in archive";

	PlyLoader loader(plyFile.string());

	EXPECT_GT(loader.nVertices(), 0u);

	const uint32_t bufSize = 4096;
	std::vector<Triangle> buffer(bufSize);
	uint32_t total = 0;
	uint32_t read = 0;
	do {
		read = loader.getTriangles(bufSize, buffer.data());
		total += read;
	} while (read > 0);

	EXPECT_GT(total, 0u) << "Archive .ply should contain at least one triangle";
}
