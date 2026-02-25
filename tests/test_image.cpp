/*
 * Tests for nx::Image
 */
#include <gtest/gtest.h>
#include <filesystem>
#include <cstring>
#include "common/image.h"

namespace fs = std::filesystem;

TEST(ImageTest, DefaultConstructor) {
	nx::Image img;
	EXPECT_TRUE(img.isNull());
	EXPECT_EQ(img.width(), 0);
	EXPECT_EQ(img.height(), 0);
}

TEST(ImageTest, SizedConstructor) {
	nx::Image img(64, 32);
	EXPECT_FALSE(img.isNull());
	EXPECT_EQ(img.width(), 64);
	EXPECT_EQ(img.height(), 32);
	EXPECT_EQ(img.dataSize(), static_cast<size_t>(64 * 32 * 4));
}

TEST(ImageTest, Fill) {
	nx::Image img(4, 4);
	img.fill(255, 0, 128, 200);

	const unsigned char *row = img.scanLine(0);
	// RGBA order
	EXPECT_EQ(row[0], 255);
	EXPECT_EQ(row[1], 0);
	EXPECT_EQ(row[2], 128);
	EXPECT_EQ(row[3], 200);

	// Last pixel
	const unsigned char *lastRow = img.scanLine(3);
	int lastPx = 3 * 4;
	EXPECT_EQ(lastRow[lastPx + 0], 255);
	EXPECT_EQ(lastRow[lastPx + 3], 200);
}

TEST(ImageTest, CopyConstructor) {
	nx::Image a(8, 8);
	a.fill(10, 20, 30, 40);

	nx::Image b(a);
	EXPECT_EQ(b.width(), 8);
	EXPECT_EQ(b.height(), 8);

	// Data must be independent
	b.fill(0, 0, 0, 0);
	EXPECT_EQ(a.scanLine(0)[0], 10);
	EXPECT_EQ(b.scanLine(0)[0], 0);
}

TEST(ImageTest, MoveConstructor) {
	nx::Image a(16, 16);
	a.fill(50, 60, 70, 80);

	nx::Image b(std::move(a));
	EXPECT_EQ(b.width(), 16);
	EXPECT_EQ(b.height(), 16);
	EXPECT_TRUE(a.isNull());  // moved-from
}

TEST(ImageTest, CropCopy) {
	nx::Image img(10, 10);
	img.fill(100, 150, 200, 255);

	// Write a distinct pixel at (2,3)
	unsigned char *row3 = img.scanLine(3);
	row3[2 * 4 + 0] = 1;
	row3[2 * 4 + 1] = 2;
	row3[2 * 4 + 2] = 3;
	row3[2 * 4 + 3] = 4;

	nx::Rect r(1, 2, 5, 5);
	nx::Image cropped = img.copy(r);
	EXPECT_EQ(cropped.width(), 5);
	EXPECT_EQ(cropped.height(), 5);

	// The pixel at (2,3) in original -> (1,1) in cropped
	const unsigned char *crow = cropped.scanLine(1);
	EXPECT_EQ(crow[1 * 4 + 0], 1);
	EXPECT_EQ(crow[1 * 4 + 1], 2);
	EXPECT_EQ(crow[1 * 4 + 2], 3);
	EXPECT_EQ(crow[1 * 4 + 3], 4);
}

TEST(ImageTest, Scaled) {
	nx::Image img(100, 100);
	img.fill(128, 128, 128, 255);

	nx::Image small = img.scaled(50, 50);
	EXPECT_EQ(small.width(), 50);
	EXPECT_EQ(small.height(), 50);
	EXPECT_FALSE(small.isNull());
}

TEST(ImageTest, Mirrored) {
	nx::Image img(4, 4);
	img.fill(0, 0, 0, 255);

	// Set first row pixel to red
	unsigned char *row0 = img.scanLine(0);
	row0[0] = 255; // R

	nx::Image flipped = img.mirrored();
	// First row's pixel should now be on last row
	const unsigned char *lastRow = flipped.scanLine(3);
	EXPECT_EQ(lastRow[0], 255);
	// New first row should be black
	const unsigned char *firstRow = flipped.scanLine(0);
	EXPECT_EQ(firstRow[0], 0);
}

TEST(ImageTest, Blit) {
	nx::Image dst(10, 10);
	dst.fill(0, 0, 0, 255);

	nx::Image src(3, 3);
	src.fill(200, 100, 50, 255);

	dst.blit(2, 2, src);

	// Check pixel at (2,2)
	const unsigned char *row = dst.scanLine(2);
	EXPECT_EQ(row[2 * 4 + 0], 200);
	EXPECT_EQ(row[2 * 4 + 1], 100);
	EXPECT_EQ(row[2 * 4 + 2], 50);

	// Check pixel at (0,0) is still black
	const unsigned char *row0 = dst.scanLine(0);
	EXPECT_EQ(row0[0], 0);
}

TEST(ImageTest, SaveAndLoadJPEG) {
	auto path = (fs::temp_directory_path() / "nexus_test_img.jpg").string();

	nx::Image img(32, 32);
	img.fill(128, 64, 200, 255);
	ASSERT_TRUE(img.save(path, 90));

	nx::Image loaded;
	ASSERT_TRUE(loaded.load(path));
	EXPECT_EQ(loaded.width(), 32);
	EXPECT_EQ(loaded.height(), 32);

	// JPEG is lossy, but values should be close
	const unsigned char *p = loaded.scanLine(16);
	EXPECT_NEAR(p[0], 128, 15);
	EXPECT_NEAR(p[1], 64, 15);
	EXPECT_NEAR(p[2], 200, 15);

	fs::remove(path);
}

TEST(ImageTest, SaveAndLoadPNG) {
	auto path = (fs::temp_directory_path() / "nexus_test_img.png").string();

	nx::Image img(16, 16);
	img.fill(10, 20, 30, 255);
	ASSERT_TRUE(img.save(path));

	nx::Image loaded;
	ASSERT_TRUE(loaded.load(path));
	EXPECT_EQ(loaded.width(), 16);
	EXPECT_EQ(loaded.height(), 16);

	// PNG is lossless
	const unsigned char *p = loaded.scanLine(0);
	EXPECT_EQ(p[0], 10);
	EXPECT_EQ(p[1], 20);
	EXPECT_EQ(p[2], 30);
	EXPECT_EQ(p[3], 255);

	fs::remove(path);
}

TEST(ImageTest, SaveToMemoryJPEG) {
	nx::Image img(8, 8);
	img.fill(100, 200, 50, 255);

	std::vector<unsigned char> buf;
	ASSERT_TRUE(img.saveToMemory(buf, 80));
	EXPECT_GT(buf.size(), 0u);

	// Verify it's a valid JPEG (starts with FFD8)
	ASSERT_GE(buf.size(), 2u);
	EXPECT_EQ(buf[0], 0xFF);
	EXPECT_EQ(buf[1], 0xD8);

	// Load it back
	nx::Image loaded;
	ASSERT_TRUE(loaded.loadFromData(buf.data(), buf.size()));
	EXPECT_EQ(loaded.width(), 8);
	EXPECT_EQ(loaded.height(), 8);
}

TEST(ImageTest, LoadNonExistent) {
	nx::Image img;
	EXPECT_FALSE(img.load("nonexistent_file_qwerty.png"));
	EXPECT_TRUE(img.isNull());
}
