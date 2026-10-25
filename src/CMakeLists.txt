cmake_minimum_required(VERSION 3.10)
project(nexus)

set(HEADERS
	${VCGDIR}/wrap/ply/plylib.h
	common/cone.h
	common/error.h
	common/mappedfile.h
	common/numeric.h
	common/pathutil.h
	common/scratchfile.h
	common/signature.h
	common/nexusdata.h
	common/nexusfile.h
	common/stdnexusfile.h
	common/textfile.h
	common/textparse.h
	common/traversal.h
	common/virtualarray.h
	nxsbuild/image.h
	nxsbuild/imageio.h
	nxsbuild/kdtree.h
	nxsbuild/mesh.h
	nxsbuild/meshstream.h
	nxsbuild/meshloader.h
	nxsbuild/nexusbuilder.h
	nxsbuild/objloader.h
	nxsbuild/plyloader.h
	nxsbuild/stlloader.h
	nxsbuild/texpyramid.h
	nxsbuild/tmesh.h
	nxsbuild/colormap.h
	nxsbuild/tsploader.h
	nxsbuild/tsloader.h
	nxsbuild/vcgloader.h
	nxsedit/extractor.h
	nxszip/bitstream.h
	nxszip/meshcoder.h
	nxszip/meshdecoder.h
	nxszip/tunstall.h
)

set(SOURCES
	${VCGDIR}/wrap/ply/plylib.cpp
	common/cone.cpp
	common/mappedfile.cpp
	common/nexusdata.cpp
	common/scratchfile.cpp
	common/stdnexusfile.cpp
	common/textfile.cpp
	common/traversal.cpp
	common/virtualarray.cpp
	nxsbuild/image.cpp
	nxsbuild/imageio.cpp
	nxsbuild/imagejpeg.cpp
	nxsbuild/kdtree.cpp
	nxsbuild/mesh.cpp
	nxsbuild/meshstream.cpp
	nxsbuild/meshloader.cpp
	nxsbuild/nexusbuilder.cpp
	nxsbuild/objloader.cpp
	nxsbuild/plyloader.cpp
	nxsbuild/stlloader.cpp
	nxsbuild/texpyramid.cpp
	nxsbuild/tmesh.cpp
	nxsbuild/colormap.cpp
	nxsbuild/tsploader.cpp
	nxsbuild/tsloader.cpp
	nxsedit/extractor.cpp
	nxszip/abitstream.cpp
	nxszip/atunstall.cpp
	nxszip/meshcoder.cpp
	nxszip/meshdecoder.cpp
)

add_library(nexus STATIC
	${SOURCES} ${HEADERS})

target_include_directories(nexus PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	$<INSTALL_INTERFACE:include>
)

# std::filesystem, and the headers of VCGLib that nexus includes need C++17. PUBLIC, so what
# links nexus is built as C++17 too.
target_compile_features(nexus PUBLIC cxx_std_17)

# nexus ends up inside a shared library (libnxs), whose code must be position independent; the
# thread-local state of the image decoders makes it matter.
set_target_properties(nexus PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The image libraries are private: the program that links nexus does not need their headers.
target_include_directories(nexus PRIVATE ${Stb_INCLUDE_DIR})
target_link_libraries(nexus PUBLIC vcglib corto)
target_link_libraries(nexus PRIVATE JPEG::JPEG PNG::PNG TIFF::TIFF WebP::webpdecoder)

if (WIN32)
	target_compile_definitions(nexus PUBLIC NOMINMAX)
endif()

if(NEXUS_INSTALL)
	foreach(DIR common nxsbuild nxsedit nxsview nxszip)
		install(DIRECTORY ${DIR}
			DESTINATION include
			FILES_MATCHING PATTERN "*.h")
	endforeach()
	install(TARGETS nexus
		EXPORT nexusTargets
		LIBRARY DESTINATION lib)
endif()
