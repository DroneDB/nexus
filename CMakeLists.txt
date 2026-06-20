#export CMAKE_PREFIX_PATH=path to Qt/5.5/gcc_64/ (where you would find bin/qmake)
cmake_minimum_required(VERSION 3.5)
project(nexus)

option(BUILD_NXS_BUILD "" OFF)
option(BUILD_NXS_EDIT "" OFF)

set (CMAKE_CXX_STANDARD 11)

# enable setting options with SET cmake command
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

find_package(Qt5 COMPONENTS Core Gui REQUIRED)

# nexus version
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/NEXUS_VERSION")
	file (READ "${CMAKE_CURRENT_SOURCE_DIR}/NEXUS_VERSION" NEXUS_VERSION)
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

# corto is a third-party submodule (cnr-isti-vclab/corto) pinned at a commit whose
# CMakeLists still declares cmake_minimum_required(VERSION 3.1). CMake >= 4.0 removed
# compatibility with versions < 3.5, so floor the policy version only for the corto
# subtree without modifying the vendored third-party sources.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
add_subdirectory(src/corto EXCLUDE_FROM_ALL)
unset(CMAKE_POLICY_VERSION_MINIMUM)

add_subdirectory(src)

if (BUILD_NXS_BUILD)
	add_subdirectory(src/nxsbuild)
endif()

if(BUILD_NXS_EDIT)
	add_subdirectory(src/nxsedit)
endif()

