cmake_minimum_required(VERSION 3.16)
project(nexus_tests)

find_package(GTest CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(MINIZIP NAMES minizip-ng CONFIG REQUIRED)

enable_testing()

add_executable(nexus_tests
	test_mmap_file.cpp
	test_image.cpp
	test_signature.cpp
	test_virtualarray.cpp
	test_nexusdata.cpp
	test_nexusbuild.cpp
	test_objloader.cpp
	test_plyloader.cpp
	test_stlloader.cpp
	testarea.cpp
	testfs.cpp
)

target_link_libraries(nexus_tests PRIVATE
	nexus
	GTest::gtest
	GTest::gtest_main
	cpr::cpr
	MINIZIP::minizip-ng
)

target_include_directories(nexus_tests PRIVATE
	${CMAKE_SOURCE_DIR}/src
	${CMAKE_SOURCE_DIR}/tests
	${Stb_INCLUDE_DIR}
	${BSHOSHANY_THREAD_POOL_INCLUDE_DIRS}
)

include(GoogleTest)
gtest_discover_tests(nexus_tests)
