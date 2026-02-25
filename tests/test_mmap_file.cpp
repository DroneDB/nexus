/*
 * Tests for nx::MappedFile and nx::TempMappedFile
 */
#include <gtest/gtest.h>
#include <filesystem>
#include <cstring>
#include "common/mmap_file.h"

namespace fs = std::filesystem;

class MappedFileTest : public ::testing::Test {
protected:
	std::string testPath;

	void SetUp() override {
		testPath = (fs::temp_directory_path() / "nexus_test_mmap.bin").string();
		// clean up any leftover
		fs::remove(testPath);
	}
	void TearDown() override {
		fs::remove(testPath);
	}
};

TEST_F(MappedFileTest, DefaultState) {
	nx::MappedFile f;
	EXPECT_FALSE(f.isOpen());
	EXPECT_TRUE(f.fileName().empty());
}

TEST_F(MappedFileTest, SetFileName) {
	nx::MappedFile f;
	f.setFileName(testPath);
	EXPECT_EQ(f.fileName(), testPath);
	EXPECT_FALSE(f.isOpen());
}

TEST_F(MappedFileTest, CreateAndWriteRead) {
	nx::MappedFile f;
	f.setFileName(testPath);
	ASSERT_TRUE(f.open(nx::MappedFile::ReadWrite | nx::MappedFile::Truncate));
	EXPECT_TRUE(f.isOpen());

	const char data[] = "Hello Nexus!";
	auto written = f.write(data, sizeof(data));
	EXPECT_EQ(written, static_cast<int64_t>(sizeof(data)));

	ASSERT_TRUE(f.seek(0));
	EXPECT_EQ(f.pos(), 0u);

	char buf[64] = {};
	auto readBytes = f.read(buf, sizeof(data));
	EXPECT_EQ(readBytes, static_cast<int64_t>(sizeof(data)));
	EXPECT_STREQ(buf, "Hello Nexus!");

	f.close();
	EXPECT_FALSE(f.isOpen());
}

TEST_F(MappedFileTest, Resize) {
	nx::MappedFile f;
	f.setFileName(testPath);
	ASSERT_TRUE(f.open(nx::MappedFile::ReadWrite | nx::MappedFile::Truncate));

	ASSERT_TRUE(f.resize(4096));
	EXPECT_EQ(f.size(), 4096u);

	ASSERT_TRUE(f.resize(1024));
	EXPECT_EQ(f.size(), 1024u);

	f.close();
}

TEST_F(MappedFileTest, MapAndUnmap) {
	nx::MappedFile f;
	f.setFileName(testPath);
	ASSERT_TRUE(f.open(nx::MappedFile::ReadWrite | nx::MappedFile::Truncate));
	ASSERT_TRUE(f.resize(4096));

	unsigned char *ptr = f.map(0, 4096);
	ASSERT_NE(ptr, nullptr);

	// Write through mmap
	std::memset(ptr, 0xAB, 4096);
	EXPECT_EQ(ptr[0], 0xAB);
	EXPECT_EQ(ptr[4095], 0xAB);

	EXPECT_TRUE(f.unmap(ptr));
	f.close();

	// Re-open and verify data persisted
	f.setFileName(testPath);
	ASSERT_TRUE(f.open(nx::MappedFile::ReadWrite));
	unsigned char *ptr2 = f.map(0, 4096);
	ASSERT_NE(ptr2, nullptr);
	EXPECT_EQ(ptr2[0], 0xAB);
	EXPECT_EQ(ptr2[2048], 0xAB);
	EXPECT_TRUE(f.unmap(ptr2));
	f.close();
}

TEST_F(MappedFileTest, MapPartialRegion) {
	nx::MappedFile f;
	f.setFileName(testPath);
	ASSERT_TRUE(f.open(nx::MappedFile::ReadWrite | nx::MappedFile::Truncate));
	ASSERT_TRUE(f.resize(8192));

	// Map the second half
	unsigned char *ptr = f.map(4096, 4096);
	ASSERT_NE(ptr, nullptr);
	ptr[0] = 0xFF;
	ptr[4095] = 0xFE;
	EXPECT_TRUE(f.unmap(ptr));

	// Verify through read
	ASSERT_TRUE(f.seek(4096));
	char buf[2] = {};
	f.read(buf, 1);
	EXPECT_EQ(static_cast<unsigned char>(buf[0]), 0xFF);
	f.close();
}

TEST_F(MappedFileTest, OpenNonExistentReadOnly) {
	nx::MappedFile f;
	f.setFileName("nonexistent_file_xyz.bin");
	EXPECT_FALSE(f.open(nx::MappedFile::ReadOnly));
	EXPECT_FALSE(f.isOpen());
}

// --- TempMappedFile tests ---

TEST(TempMappedFileTest, CreatesAndOpens) {
	nx::TempMappedFile tmp("nextest");
	EXPECT_TRUE(tmp.isOpen());
	EXPECT_FALSE(tmp.fileName().empty());
	EXPECT_TRUE(fs::exists(tmp.fileName()));
}

TEST(TempMappedFileTest, WriteAndRead) {
	nx::TempMappedFile tmp("nextest");
	ASSERT_TRUE(tmp.isOpen());

	const char msg[] = "temp data";
	tmp.write(msg, sizeof(msg));
	tmp.seek(0);

	char buf[32] = {};
	tmp.read(buf, sizeof(msg));
	EXPECT_STREQ(buf, "temp data");
}

TEST(TempMappedFileTest, DeletedOnDestruction) {
	std::string savedPath;
	{
		nx::TempMappedFile tmp("nextest_del");
		savedPath = tmp.fileName();
		EXPECT_TRUE(fs::exists(savedPath));
	}
	EXPECT_FALSE(fs::exists(savedPath));
}
