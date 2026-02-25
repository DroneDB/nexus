/*
 * Tests for VirtualMemory / VirtualArray / VirtualChunks
 */
#include <gtest/gtest.h>
#include <cstring>
#include "common/virtualarray.h"

TEST(VirtualChunksTest, Construction) {
	VirtualChunks vm("nextest_va");
	EXPECT_EQ(vm.nBlocks(), 0u);
}

TEST(VirtualChunksTest, AddChunk) {
	VirtualChunks vm("nextest_va");

	uint64_t idx = vm.addChunk(64);
	EXPECT_EQ(idx, 0u);
	EXPECT_EQ(vm.nBlocks(), 1u);

	uint64_t idx2 = vm.addChunk(64);
	EXPECT_EQ(idx2, 1u);
	EXPECT_EQ(vm.nBlocks(), 2u);
}

TEST(VirtualChunksTest, WriteAndReadChunk) {
	VirtualChunks vm("nextest_va");

	uint64_t b = vm.addChunk(256);

	unsigned char *ptr = vm.getChunk(b);
	ASSERT_NE(ptr, nullptr);

	// Write pattern
	for (int i = 0; i < 256; i++) ptr[i] = static_cast<unsigned char>(i & 0xFF);

	// Re-fetch the chunk and verify
	unsigned char *ptr2 = vm.getChunk(b);
	EXPECT_EQ(ptr2[0], 0);
	EXPECT_EQ(ptr2[127], 127);
	EXPECT_EQ(ptr2[255], 255);
}

TEST(VirtualChunksTest, MultipleChunks) {
	VirtualChunks vm("nextest_va");

	uint64_t b0 = vm.addChunk(128);
	uint64_t b1 = vm.addChunk(128);

	unsigned char *p0 = vm.getChunk(b0);
	std::memset(p0, 0xAA, 128);

	unsigned char *p1 = vm.getChunk(b1);
	std::memset(p1, 0xBB, 128);

	// Verify they are distinct
	p0 = vm.getChunk(b0);
	p1 = vm.getChunk(b1);
	EXPECT_EQ(p0[0], 0xAA);
	EXPECT_EQ(p1[0], 0xBB);
}

TEST(VirtualChunksTest, DropChunk) {
	VirtualChunks vm("nextest_va");

	uint64_t b = vm.addChunk(64);
	unsigned char *p = vm.getChunk(b);
	std::memset(p, 0x42, 64);

	vm.dropChunk(b);
	// After dropping, data is still on disk backing
	p = vm.getChunk(b);
	EXPECT_EQ(p[0], 0x42);
}

// VirtualArray tests
TEST(VirtualArrayTest, ResizeAndAccess) {
	VirtualArray<int> arr("nextest_arr");

	arr.resize(100);
	EXPECT_EQ(arr.size(), 100u);

	for (int i = 0; i < 100; i++)
		arr[i] = i * 10;

	for (int i = 0; i < 100; i++)
		EXPECT_EQ(arr[i], i * 10);
}

TEST(VirtualArrayTest, ResizeSmaller) {
	VirtualArray<int> arr("nextest_arr");
	arr.resize(200);
	EXPECT_EQ(arr.size(), 200u);

	arr[0] = 42;
	arr[199] = 99;

	EXPECT_EQ(arr[0], 42);
	EXPECT_EQ(arr[199], 99);
}

TEST(VirtualArrayTest, LargeArray) {
	VirtualArray<double> arr("nextest_large");
	const uint64_t N = 100000;

	arr.resize(N);
	EXPECT_EQ(arr.size(), N);

	arr[0] = 1.5;
	arr[N - 1] = 99.9;
	EXPECT_DOUBLE_EQ(arr[0], 1.5);
	EXPECT_DOUBLE_EQ(arr[N - 1], 99.9);
}
