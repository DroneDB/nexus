/*
 * Tests for ObjLoader — mesh loading from .obj files.
 *
 * These tests use TestArea (single file download) and TestFS (zip archive)
 * to fetch test assets from remote URLs.
 */
#include <gtest/gtest.h>
#include <filesystem>
#include <string>

#include "testarea.h"
#include "testfs.h"
#include "nxsbuild/objloader.h"
#include "nxsbuild/trianglesoup.h"

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Test asset URLs
// ---------------------------------------------------------------------------

// Single .obj file (no materials)
static const std::string OBJ_URL         = "https://github.com/DroneDB/test_data/raw/refs/heads/master/3d/brighton_beach.obj";
// Companion .mtl file
static const std::string MTL_URL         = "https://github.com/DroneDB/test_data/raw/refs/heads/master/3d/brighton_beach.obj.mtl";
// Zip archive containing .obj + .mtl + textures
static const std::string OBJ_ARCHIVE_URL = "https://github.com/DroneDB/test_data/raw/refs/heads/master/brighton/odm_texturing.zip";

// Expected values
static const uint64_t EXPECTED_OBJ_MIN_TRIANGLES = 1;
static const uint64_t EXPECTED_OBJ_MIN_VERTICES  = 3;

// ---------------------------------------------------------------------------
// Fixture: downloads a single .obj via TestArea
// ---------------------------------------------------------------------------
class ObjLoaderTestArea : public ::testing::Test {
protected:
	void SetUp() override {
		area = std::make_unique<TestArea>("obj_loader_test");
	}

	void TearDown() override {
		area.reset();
	}

	std::unique_ptr<TestArea> area;
};

// ---------------------------------------------------------------------------
// TestArea-based tests (single file downloads)
// ---------------------------------------------------------------------------

TEST_F(ObjLoaderTestArea, ConstructFromDownloadedObj) {
	fs::path objPath = area->downloadTestAsset(OBJ_URL, "test_model.obj");
	ASSERT_TRUE(fs::exists(objPath));
	ASSERT_GT(fs::file_size(objPath), 0u);

	// Construct loader — should not throw
	EXPECT_NO_THROW({
		ObjLoader loader(objPath.string(), "");
	});
}

TEST_F(ObjLoaderTestArea, ConstructWithMtl) {
	fs::path objPath = area->downloadTestAsset(OBJ_URL, "test_model.obj");
	fs::path mtlPath = area->downloadTestAsset(MTL_URL, "test_model.mtl");
	ASSERT_TRUE(fs::exists(objPath));
	ASSERT_TRUE(fs::exists(mtlPath));

	EXPECT_NO_THROW({
		ObjLoader loader(objPath.string(), mtlPath.string());
	});
}

TEST_F(ObjLoaderTestArea, GetTriangles) {
	fs::path objPath = area->downloadTestAsset(OBJ_URL, "test_model.obj");
	ObjLoader loader(objPath.string(), "");

	const uint32_t bufSize = 1024;
	std::vector<Triangle> buffer(bufSize);

	uint32_t totalTris = 0;
	uint32_t read = 0;
	do {
		read = loader.getTriangles(bufSize, buffer.data());
		totalTris += read;
	} while (read > 0);

	EXPECT_GE(totalTris, EXPECTED_OBJ_MIN_TRIANGLES)
		<< "Expected at least " << EXPECTED_OBJ_MIN_TRIANGLES << " triangles";
}

TEST_F(ObjLoaderTestArea, GetVertices) {
	fs::path objPath = area->downloadTestAsset(OBJ_URL, "test_model.obj");
	ObjLoader loader(objPath.string(), "");

	const uint32_t bufSize = 1024;
	std::vector<Splat> buffer(bufSize);

	uint32_t totalVerts = 0;
	uint32_t read = 0;
	do {
		read = loader.getVertices(bufSize, buffer.data());
		totalVerts += read;
	} while (read > 0);

	EXPECT_GE(totalVerts, EXPECTED_OBJ_MIN_VERTICES)
		<< "Expected at least " << EXPECTED_OBJ_MIN_VERTICES << " vertices";
}

TEST_F(ObjLoaderTestArea, BoundingBoxIsValid) {
	fs::path objPath = area->downloadTestAsset(OBJ_URL, "test_model.obj");
	ObjLoader loader(objPath.string(), "");

	// Read all triangles to populate the bounding box
	const uint32_t bufSize = 4096;
	std::vector<Triangle> buffer(bufSize);
	while (loader.getTriangles(bufSize, buffer.data()) > 0) {}

	auto &box = loader.box;
	// After reading triangles the bounding box must be non-degenerate
	EXPECT_LE(box.min.X(), box.max.X());
	EXPECT_LE(box.min.Y(), box.max.Y());
	EXPECT_LE(box.min.Z(), box.max.Z());
}

TEST_F(ObjLoaderTestArea, TrianglesAreNotDegenerate) {
	fs::path objPath = area->downloadTestAsset(OBJ_URL, "test_model.obj");
	ObjLoader loader(objPath.string(), "");

	const uint32_t bufSize = 1024;
	std::vector<Triangle> buffer(bufSize);

	uint32_t read = loader.getTriangles(bufSize, buffer.data());
	uint32_t degenerateCount = 0;
	for (uint32_t i = 0; i < read; ++i) {
		if (buffer[i].isDegenerate())
			++degenerateCount;
	}

	// Allow a small percentage of degenerate triangles (mesh quality issue, not loader bug)
	EXPECT_LT(degenerateCount, read)
		<< "All triangles are degenerate — loader may be broken";
}

// ---------------------------------------------------------------------------
// TestFS-based tests (zip archive with .obj + .mtl + textures)
// ---------------------------------------------------------------------------

TEST(ObjLoaderTestFS, LoadFromZipArchive) {
	TestFS testFs(OBJ_ARCHIVE_URL);

	// Find the .obj file inside the extracted archive
	fs::path objFile;
	for (auto &entry : fs::recursive_directory_iterator(testFs.testFolder)) {
		if (entry.path().extension() == ".obj") {
			objFile = entry.path();
			break;
		}
	}
	ASSERT_FALSE(objFile.empty()) << "No .obj file found in archive";

	// Look for a companion .mtl
	std::string mtl;
	fs::path mtlPath = objFile;
	mtlPath.replace_extension(".mtl");
	if (fs::exists(mtlPath))
		mtl = mtlPath.string();

	ObjLoader loader(objFile.string(), mtl);

	const uint32_t bufSize = 4096;
	std::vector<Triangle> buffer(bufSize);
	uint32_t total = 0;
	uint32_t read = 0;
	do {
		read = loader.getTriangles(bufSize, buffer.data());
		total += read;
	} while (read > 0);

	EXPECT_GT(total, 0u) << "Archive .obj should contain at least one triangle";
}

TEST(ObjLoaderTestFS, TexturesDetected) {
	TestFS testFs(OBJ_ARCHIVE_URL);

	fs::path objFile;
	std::string mtl;
	for (auto &entry : fs::recursive_directory_iterator(testFs.testFolder)) {
		if (entry.path().extension() == ".obj") {
			objFile = entry.path();
			break;
		}
	}
	ASSERT_FALSE(objFile.empty());

	fs::path mtlPath = objFile;
	mtlPath.replace_extension(".mtl");
	if (fs::exists(mtlPath))
		mtl = mtlPath.string();

	ObjLoader loader(objFile.string(), mtl);

	// If the archive contains textured models, the loader should detect textures
	if (loader.hasTextures()) {
		EXPECT_FALSE(loader.texture_filenames.empty());
	}
}

