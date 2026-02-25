cmake_minimum_required(VERSION 3.16)
project(nexus_lib)

set(HEADERS
	${VCGDIR}/wrap/ply/plylib.h
	common/dag.h
	common/nxs.h
	common/cone.h
	common/signature.h
	common/nexusdata.h
	common/nexusfile.h
	common/nexusfile_io.h
	common/mmap_file.h
	common/image.h
	common/logger.h
	common/traversal.h
	common/virtualarray.h
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
	common/nxs.cpp
	common/cone.cpp
	common/nexusdata.cpp
	common/nexusfile_io.cpp
	common/mmap_file.cpp
	common/image.cpp
	common/traversal.cpp
	common/virtualarray.cpp
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

target_link_libraries(nexus PUBLIC
	vcglib
	corto
	ZLIB::ZLIB
	PNG::PNG
	JPEG::JPEG
	plog::plog
)

target_include_directories(nexus PUBLIC
	${Stb_INCLUDE_DIR}
	${BSHOSHANY_THREAD_POOL_INCLUDE_DIRS}
)

set_target_properties(nexus PROPERTIES OUTPUT_NAME "nexus" PREFIX "lib")

if (WIN32)
	target_compile_definitions(nexus PUBLIC NOMINMAX)
endif()

foreach(DIR common nxsbuild nxsedit nxszip)
	install(DIRECTORY ${DIR}
		DESTINATION include
		FILES_MATCHING PATTERN "*.h")
endforeach()
install(TARGETS nexus
	EXPORT nexusTargets
	LIBRARY DESTINATION lib)
