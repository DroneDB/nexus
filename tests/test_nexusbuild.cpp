/*
 * Tests for nexusBuild / nexusBuildEx  (exercises doNexusBuild via public API).
 *
 * A remote ZIP archive with a textured OBJ (PNG textures + MTL) is used.
 * TestFS handles download, caching and extraction of the archive.
 * Output files are written inside the TestFS temp folder that is
 * automatically cleaned up on destruction.
 *
 * NOTE: TestFS is created with setCurrentDirectory=true so that the
 * nexus cache files (written to CWD by the builder) land inside the
 * temp folder and get cleaned up automatically.
 */
#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <string>

#include "testfs.h"
#include "common/nexusdata.h"
#include "common/nxs.h"

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Remote archive URL
// ---------------------------------------------------------------------------
static const std::string TEXTURED_OBJ_ARCHIVE =
    "https://github.com/DroneDB/test_data/raw/refs/heads/master/brighton/odm_texturing.zip";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

/// Recursively find the first file with the given extension inside @p root.
fs::path findFileByExtension(const fs::path& root, const std::string& ext) {
    for (auto& entry : fs::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() && entry.path().extension() == ext)
            return entry.path();
    }
    return {};
}

/// Build a unique output path inside @p folder with the given @p filename.
fs::path outputPath(const std::string& folder, const std::string& filename) {
    fs::path p = fs::path(folder) / filename;
    fs::create_directories(p.parent_path());
    return p;
}

} // namespace

// ===================================================================
// Suite: NexusBuildTexturedObj — textured mesh (PNG + MTL)
// ===================================================================

class NexusBuildTexturedObj : public ::testing::Test {
protected:
    void SetUp() override {
        // setCurrentDirectory = true: CWD → temp folder so cache files
        // written by the builder are co-located and cleaned up.
        testFs = std::make_unique<TestFS>(TEXTURED_OBJ_ARCHIVE,
                                          "NexusBuildTests", /*setCurrentDirectory=*/true);
        objFile = findFileByExtension(testFs->testFolder, ".obj");
        ASSERT_FALSE(objFile.empty()) << "No .obj found in textured archive";
    }

    void TearDown() override { testFs.reset(); }

    /// Return a NexusBuildOptions pre-configured for fast test execution.
    static NexusBuildOptions fastOpts() {
        NexusBuildOptions o;
        o.ram_buffer_mb = 2000;     // 2 GB – enough but keeps allocation sane
        o.node_faces    = 4096;     // smaller nodes → faster build
        o.top_node_faces = 1024;
        o.scaling        = 0.5f;
        return o;
    }

    std::unique_ptr<TestFS> testFs;
    fs::path objFile;
};

TEST_F(NexusBuildTexturedObj, BuildNxsDefaultOptions) {
    fs::path nxsOut = outputPath(testFs->testFolder, "output.nxs");

    auto opts = fastOpts();

    char errMsg[512] = {};
    NXSErr err = nexusBuildEx(objFile.string().c_str(),
                              nxsOut.string().c_str(),
                              opts, errMsg, sizeof(errMsg));

    ASSERT_EQ(err, NXSERR_NONE) << "nexusBuildEx failed: " << errMsg;
    ASSERT_TRUE(fs::exists(nxsOut));
    EXPECT_GT(fs::file_size(nxsOut), 0u);

    // Open and validate header
    nx::NexusData nexus;
    ASSERT_TRUE(nexus.open(nxsOut.string().c_str()));
    EXPECT_GT(nexus.header.nvert, 0u);
    EXPECT_GT(nexus.header.nface, 0u);
    EXPECT_GT(nexus.header.n_nodes, 0u);
    // Textured mesh → at least one texture expected
    EXPECT_GT(nexus.header.n_textures, 0u);
    nexus.close();
}

TEST_F(NexusBuildTexturedObj, BuildNxzWithCortoCompression) {
    fs::path nxzOut = outputPath(testFs->testFolder, "output.nxz");

    auto opts = fastOpts();
    opts.compress_lib = "corto";
    opts.enable_compression = true;

    char errMsg[512] = {};
    NXSErr err = nexusBuildEx(objFile.string().c_str(),
                              nxzOut.string().c_str(),
                              opts, errMsg, sizeof(errMsg));

    ASSERT_EQ(err, NXSERR_NONE) << "nexusBuildEx (.nxz corto) failed: " << errMsg;
    ASSERT_TRUE(fs::exists(nxzOut));
    EXPECT_GT(fs::file_size(nxzOut), 0u);

    nx::NexusData nexus;
    ASSERT_TRUE(nexus.open(nxzOut.string().c_str()));
    EXPECT_GT(nexus.header.nvert, 0u);
    EXPECT_GT(nexus.header.nface, 0u);
    EXPECT_TRUE(nexus.header.signature.isCompressed());
    EXPECT_TRUE(nexus.header.signature.flags & nx::Signature::CORTO);
    nexus.close();
}

TEST_F(NexusBuildTexturedObj, BuildExWithCustomOptions) {
    fs::path nxsOut = outputPath(testFs->testFolder, "custom.nxs");

    auto opts = fastOpts();
    opts.texture_quality = 80;

    char errMsg[512] = {};
    NXSErr err = nexusBuildEx(objFile.string().c_str(),
                              nxsOut.string().c_str(),
                              opts, errMsg, sizeof(errMsg));

    ASSERT_EQ(err, NXSERR_NONE) << "nexusBuildEx (custom opts) failed: " << errMsg;
    ASSERT_TRUE(fs::exists(nxsOut));
    EXPECT_GT(fs::file_size(nxsOut), 0u);

    nx::NexusData nexus;
    ASSERT_TRUE(nexus.open(nxsOut.string().c_str()));
    EXPECT_GT(nexus.header.nvert, 0u);
    EXPECT_GT(nexus.header.nface, 0u);
    nexus.close();
}

TEST_F(NexusBuildTexturedObj, DisableTexcoordsRemovesTextures) {
    fs::path nxsOut = outputPath(testFs->testFolder, "notex.nxs");

    auto opts = fastOpts();
    opts.disable_texcoords = true;

    char errMsg[512] = {};
    NXSErr err = nexusBuildEx(objFile.string().c_str(),
                              nxsOut.string().c_str(),
                              opts, errMsg, sizeof(errMsg));

    ASSERT_EQ(err, NXSERR_NONE) << "nexusBuildEx (no texcoords) failed: " << errMsg;

    nx::NexusData nexus;
    ASSERT_TRUE(nexus.open(nxsOut.string().c_str()));
    EXPECT_EQ(nexus.header.n_textures, 0u);
    nexus.close();
}

// ===================================================================
// Suite: NexusBuildValidation — input validation (no downloads)
// ===================================================================

TEST(NexusBuildValidation, NullInputReturnsError) {
    char errMsg[256] = {};
    NXSErr err = nexusBuild(nullptr, "out.nxs", errMsg, sizeof(errMsg));
    EXPECT_EQ(err, NXSERR_INVALID_INPUT);
    EXPECT_GT(std::strlen(errMsg), 0u);
}

TEST(NexusBuildValidation, NullOutputReturnsError) {
    char errMsg[256] = {};
    NXSErr err = nexusBuild("input.obj", nullptr, errMsg, sizeof(errMsg));
    EXPECT_EQ(err, NXSERR_INVALID_INPUT);
    EXPECT_GT(std::strlen(errMsg), 0u);
}

TEST(NexusBuildValidation, NonexistentInputReturnsError) {
    char errMsg[256] = {};
    NXSErr err = nexusBuild("nonexistent_file.obj", "out.nxs", errMsg, sizeof(errMsg));
    EXPECT_EQ(err, NXSERR_INVALID_INPUT);
    EXPECT_TRUE(std::strstr(errMsg, "does not exist") != nullptr);
}

TEST(NexusBuildValidation, ErrorBufferNullDoesNotCrash) {
    NXSErr err = nexusBuild(nullptr, "out.nxs", nullptr, 0);
    EXPECT_EQ(err, NXSERR_INVALID_INPUT);
}

TEST(NexusBuildValidation, BothNullReturnsError) {
    char errMsg[256] = {};
    NXSErr err = nexusBuild(nullptr, nullptr, errMsg, sizeof(errMsg));
    EXPECT_EQ(err, NXSERR_INVALID_INPUT);
}
