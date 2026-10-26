cmake_minimum_required(VERSION 3.10)
project(nexus)

option(BUILD_NXS_BUILD "" OFF)
option(BUILD_NXS_EDIT "" OFF)

# The command line tools (and the viewer) were written on Qt, which the library no longer uses;
# they are not kept up to date here.
if(BUILD_NXS_BUILD OR BUILD_NXS_EDIT)
	message(FATAL_ERROR "nxsbuild and nxsedit are not built from this copy of Nexus: the library no longer uses Qt, and they still do")
endif()

# A standalone build installs the static library, its headers and the CMake package. A project
# that embeds nexus (add_subdirectory) links it into its own targets and installs only those.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set(NEXUS_INSTALL_DEFAULT ON)
else()
	set(NEXUS_INSTALL_DEFAULT OFF)
endif()
option(NEXUS_INSTALL "Install the nexus library, its headers and its CMake package" ${NEXUS_INSTALL_DEFAULT})

set (CMAKE_CXX_STANDARD 17)

# enable setting options with SET cmake command
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Node textures are JPEG or WebP, and the textures of a model come as JPEG, PNG, TIFF, WebP, BMP, GIF or
# TGA files: libjpeg-turbo, libpng, libtiff and libwebp decode and encode them, and stb_image
# reads the small formats. No GUI toolkit is involved.
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(TIFF REQUIRED)
find_package(WebP CONFIG REQUIRED)
find_package(Stb REQUIRED)

# nexus version
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/NEXUS_VERSION")
	file (READ "${CMAKE_CURRENT_SOURCE_DIR}/NEXUS_VERSION" NEXUS_VERSION)
else()
	message(FATAL_ERROR "NEXUS_VERSION file not found in the repository")
endif()

# VCGLib -- required. A parent project that already added it (target vcglib, with VCGDIR set)
# is used as is: adding it a second time would add its install rules a second time, and an
# installed VCGLib found by find_package could not take the vcglib name.
if(NOT TARGET vcglib)
	find_package(vcglib CONFIG)
endif()
if(TARGET vcglib::vcglib)
	message(STATUS "nexus - VCGLib - using system provided library")
	add_library(vcglib ALIAS vcglib::vcglib)
	get_target_property(VCGDIR vcglib::vcglib INTERFACE_INCLUDE_DIRECTORIES)
elseif(TARGET vcglib)
	message(STATUS "nexus - VCGLib - using the parent project's library")
elseif (VCGDIR) # VCGDIR exists - using custom user vcglib path
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

find_package(corto CONFIG)
if(TARGET corto::corto)
	message(STATUS "nexus - corto - using system provided library")
	add_library(corto ALIAS corto::corto)
else()
	# corto (a submodule, kept as upstream has it) asks for CMake 3.5, which CMake 4 builds only
	# with a deprecation warning; it builds with the policies of 3.10, which this project requires
	# anyway. The function keeps the setting to corto's directory (CMake before 4.0 ignores it).
	function(nexus_add_corto)
		if(NOT CMAKE_POLICY_VERSION_MINIMUM OR CMAKE_POLICY_VERSION_MINIMUM VERSION_LESS 3.10)
			set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
		endif()
		add_subdirectory(src/corto EXCLUDE_FROM_ALL)
	endfunction()
	nexus_add_corto()
endif()

add_subdirectory(src)

if (BUILD_NXS_BUILD)
	add_subdirectory(src/nxsbuild)
endif()

if(BUILD_NXS_EDIT)
	add_subdirectory(src/nxsedit)
endif()

if(NEXUS_INSTALL AND TARGET vcglib::vcglib AND TARGET corto::corto)
	install(EXPORT nexusTargets
		DESTINATION lib/cmake/nexus
		NAMESPACE nexus::
		FILE nexusTargets.cmake
	)

	include(CMakePackageConfigHelpers)
	# generate the config file that includes the exports
	configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
		"${CMAKE_CURRENT_BINARY_DIR}/nexusConfig.cmake"
		INSTALL_DESTINATION lib/cmake/nexus
		NO_SET_AND_CHECK_MACRO
		NO_CHECK_REQUIRED_COMPONENTS_MACRO
	)

	install(FILES
		"${CMAKE_CURRENT_BINARY_DIR}/nexusConfig.cmake"
		DESTINATION lib/cmake/nexus
	)
endif()
