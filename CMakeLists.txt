﻿cmake_minimum_required(VERSION 3.0)

project(TexConv)

add_definitions(-DUNICODE -D_UNICODE)

if(WIN32)
    include_directories("DevIL Windows SDK/include")
    link_directories("DevIL Windows SDK/lib/x64/unicode/Release")
endif(WIN32)

include_directories(CMP_CompressonatorLib)
include_directories(CMP_Framework/Common)
include_directories(CMP_Framework/Common/half)

add_subdirectory(CMP_CompressonatorLib)
add_subdirectory(CMP_Framework)
add_subdirectory(CMP_Core)

SET(INTEL_TC_SOURCE IntelTextureCompression/ispc_texcomp.cpp
					IntelTextureCompression/ispc_texcomp.h
					IntelTextureCompression/kernel_ispc.h
					IntelTextureCompression/kernel_ispc.obj
					IntelTextureCompression/kernel_ispc_avx.h
					IntelTextureCompression/kernel_ispc_avx.obj
					IntelTextureCompression/kernel_ispc_avx2.h
					IntelTextureCompression/kernel_ispc_avx2.obj
					IntelTextureCompression/kernel_ispc_sse2.h
					IntelTextureCompression/kernel_ispc_sse2.obj
					IntelTextureCompression/kernel_ispc_sse4.h
					IntelTextureCompression/kernel_ispc_sse4.obj)

SOURCE_GROUP("Intel Texture Compression" FILES ${INTEL_TC_SOURCE})

SET(ILIMAGE_SOURCE  ILImage.h
                    ILImageSupport.cpp)

SET(PIXEL_FORMAT_SOURCE pixel_format.cpp
                        pixel_format.h)

SET(TEXTURE_FILE_CREATER_SOURCE TextureFileCreater.h
                                TextureFileCreater.cpp
                                TextureFileCreaterR.cpp
                                TextureFileCreaterRG.cpp
                                TextureFileCreaterRGB.cpp
                                TextureFileCreaterRGBA.cpp
                                TextureFileCreaterCompress.cpp)

set(SOURCE_FILE main.cpp
                ConvertImage.cpp)

SOURCE_GROUP("Image File" FILES ${ILIMAGE_SOURCE})
SOURCE_GROUP("Pixel Format" FILES ${PIXEL_FORMAT_SOURCE})
SOURCE_GROUP("Texture File Creater" FILES ${TEXTURE_FILE_CREATER_SOURCE})

add_executable(TexConv ${SOURCE_FILE} ${ILIMAGE_SOURCE} ${PIXEL_FORMAT_SOURCE} ${TEXTURE_FILE_CREATER_SOURCE})

target_link_libraries(TexConv PRIVATE CMCore CMPlatform CMUtil DevIL ILU Compressonator CMP_Framework CMP_Core)

IF(CM_BUILD_GUI_TOOLS)
    set(IDENTIFIER "com.hyzgame.texconv")

    target_link_libraries(TexConv PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets)
ENDIF(CM_BUILD_GUI_TOOLS)

add_executable(ComboTexture ComboTexture.cpp ${ILIMAGE_SOURCE})
target_link_libraries(ComboTexture PRIVATE CMCore CMPlatform CMUtil DevIL ILU MathGeoLib)

#add_executable(YUVTest YUVTest.cpp YUV.cpp SpheremapNormal.cpp ${ILIMAGE_SOURCE})
#target_link_libraries(YUVTest PRIVATE CMCore CMPlatform CMUtil DevIL ILU MathGeoLib)

#add_executable(NormalTest NormalTest.cpp SpheremapNormal.cpp ${ILIMAGE_SOURCE})
#target_link_libraries(NormalTest PRIVATE CMCore CMPlatform CMUtil DevIL ILU MathGeoLib)

add_executable(DFGen DistanceFieldGenerater.cpp ILImage.h ILImageSupport.cpp)
target_link_libraries(DFGen PRIVATE CMCore CMPlatform CMUtil DevIL ILU MathGeoLib)
