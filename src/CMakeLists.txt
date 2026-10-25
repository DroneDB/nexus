cmake_minimum_required(VERSION 3.10)
project(nexus)

set(HEADERS
	${VCGDIR}/wrap/ply/plylib.h
	common/cone.h
	common/signature.h
	common/nexusdata.h
	common/nexusfile.h
	common/qtnexusfile.h
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
	common/cone.cpp
	common/nexusdata.cpp
	common/qtnexusfile.cpp
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

# Link Qt's bundled image and text libraries and the JPEG image plugin statically when the Qt
# build ships them as static archives, so the library carries its own JPEG codec (node textures
# are JPEG) instead of looking for a plugin directory at run time. USE_NEXUS_STATIC_LIBJPEG tells
# the parent project to import the plugin (Q_IMPORT_PLUGIN).
set(ADDITIONAL_LIBS "")
set(LIB_EXT "a")
set(LIB_PREFIX "lib")
if (WIN32)
	set(LIB_EXT "lib")
	set(LIB_PREFIX "")
endif()

get_property(qtcoreloc TARGET Qt${QT_VERSION_MAJOR}::Core PROPERTY IMPORTED_LOCATION_RELEASE)
get_filename_component(QT_LIB_BASE ${qtcoreloc} DIRECTORY)
set(static_libs "${LIB_PREFIX}qtlibpng.${LIB_EXT}" "${LIB_PREFIX}qtpcre2.${LIB_EXT}" "${LIB_PREFIX}qtfreetype.${LIB_EXT}")
foreach(lib ${static_libs})
	if (EXISTS "${QT_LIB_BASE}/${lib}")
		message(STATUS "Linking ${lib}")
		list(APPEND ADDITIONAL_LIBS "${QT_LIB_BASE}/${lib}")
	endif()
	set(QLIBJPEG_PATH "${QT_LIB_BASE}/../plugins/imageformats/${LIB_PREFIX}qjpeg.${LIB_EXT}")
	if (EXISTS "${QLIBJPEG_PATH}")
		message(STATUS "Linking ${LIB_PREFIX}qjpeg.${LIB_EXT}")
		list(APPEND ADDITIONAL_LIBS "${QLIBJPEG_PATH}")
		set_property(GLOBAL PROPERTY USE_NEXUS_STATIC_LIBJPEG "YES")
	endif()
endforeach()

target_link_libraries(nexus PUBLIC vcglib corto Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui ${ADDITIONAL_LIBS})

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
