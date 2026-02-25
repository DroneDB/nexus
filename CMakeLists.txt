cmake_minimum_required(VERSION 3.16)
project(nexus LANGUAGES CXX)

option(BUILD_TESTS        "Build unit tests"           OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# enable setting options with SET cmake command
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# --- vcpkg packages ---
find_package(ZLIB       REQUIRED)
find_package(PNG        REQUIRED)
find_package(JPEG       REQUIRED)
find_package(plog       CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_path(BSHOSHANY_THREAD_POOL_INCLUDE_DIRS "BS_thread_pool.hpp")

# nexus version
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/NEXUS_VERSION")
	file(READ "${CMAKE_CURRENT_SOURCE_DIR}/NEXUS_VERSION" NEXUS_VERSION)
	string(STRIP "${NEXUS_VERSION}" NEXUS_VERSION)
else()
	message(FATAL_ERROR "NEXUS_VERSION file not found in the repository")
endif()

# VCGLib -- required
if (VCGDIR) # VCGDIR exists - using custom user vcglib path
	if(EXISTS ${VCGDIR})
		add_subdirectory(${VCGDIR} ${CMAKE_CURRENT_BINARY_DIR}/vcglib)
		message(STATUS "nexus - VCGLib - using custom VCGDIR path library")
	else()
		set(VCGDIR NOTFOUND)
	endif()
else()
	get_filename_component(VCGDIR "${CMAKE_CURRENT_LIST_DIR}/../vcglib" ABSOLUTE)
	if(EXISTS ${VCGDIR})
		add_subdirectory(${VCGDIR} ${CMAKE_CURRENT_BINARY_DIR}/vcglib)
		message(STATUS "nexus - VCGLib - using bundled source")
	else()
		set(VCGDIR NOTFOUND)
	endif()
endif()
set(VCGDIR "${VCGDIR}")

if(NOT VCGDIR)
	message(FATAL_ERROR "VCGLib not found.")
endif()

#corto options
set(BUILD_CORTO_CODEC_UNITY OFF)
set(BUILD_CORTO_EXE OFF)

add_subdirectory(src/corto EXCLUDE_FROM_ALL)

add_subdirectory(src)

if(BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
