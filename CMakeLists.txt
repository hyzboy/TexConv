﻿cmake_minimum_required(VERSION 3.5)

project(TexConv)

add_definitions(-DUNICODE -D_UNICODE)

if(WIN32)

    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/DevIL Windows SDK/include")

    IF(HGL_BITS EQUAL 32)
        SET(DEVIL_LIBRARY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/DevIL Windows SDK/lib/x86/unicode/Release")
    else()
        SET(DEVIL_LIBRARY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/DevIL Windows SDK/lib/x64/unicode/Release")
    endif()
 
    SET(HGL_DEVIL_LIB   ${DEVIL_LIBRARY_PATH}/DevIL.lib
                        ${DEVIL_LIBRARY_PATH}/ILU.lib
                        ${DEVIL_LIBRARY_PATH}/ILUT.lib)
endif(WIN32)

set(LIB_BUILD_COMPRESSONATOR_SDK    ON)
set(LIB_BUILD_FRAMEWORK_SDK         ON)
set(LIB_BUILD_IMAGEIO               OFF)
set(LIB_BUILD_ANALYSIS              OFF)
set(LIB_BUILD_COMMON                ON)
set(LIB_BUILD_CORE                  ON)
set(LIB_BUILD_GPUDECODE             ON)

include_directories(CMP_CompressonatorLib)
include_directories(CMP_Framework/Common)
include_directories(CMP_Framework/Common/half)

add_subdirectory(Applications/_plugins/common)
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
#                    Image2D.h
#                    Image2D.cpp)

SET(PIXEL_FORMAT_SOURCE pixel_format.cpp
                        pixel_format.h
                        ImageConvertConfig.h)

SET(TEXTURE_FILE_CREATER_SOURCE TextureFileCreater.h
                                TextureFileCreater.cpp
                                TextureFileCreaterR.cpp
                                TextureFileCreaterRG.cpp
                                TextureFileCreaterRGB.cpp
                                TextureFileCreaterRGBA.cpp
                                TextureFileCreaterCompress.cpp)

SET(PARAM_PARSE_SOURCE ParamParse.h ParamParse.cpp)

SOURCE_GROUP("Image File" FILES ${ILIMAGE_SOURCE})
SOURCE_GROUP("Pixel Format" FILES ${PIXEL_FORMAT_SOURCE})
SOURCE_GROUP("Texture File Creater" FILES ${TEXTURE_FILE_CREATER_SOURCE})
SOURCE_GROUP("CMD Param parse" FILES ${PARAM_PARSE_SOURCE})

SET(TEX_CONV_SOURCE ${ILIMAGE_SOURCE} ${PIXEL_FORMAT_SOURCE} ${TEXTURE_FILE_CREATER_SOURCE} ${PARAM_PARSE_SOURCE})
SET(TEX_CONV_LIBRARY CMCore CMPlatform CMUtil CMP_Compressonator CMP_Framework CMP_Core ${HGL_DEVIL_LIB})

if(WIN32)
	set(TEX_CONV_LIBRARY ${TEX_CONV_LIBRARY} glm::glm)
else()
	set(TEX_CONV_LIBRARY ${TEX_CONV_LIBRARY} GLM)
endif(WIN32)

add_executable(TexConv ${TEX_CONV_SOURCE} main.cpp ConvertImage.cpp)
target_link_libraries(TexConv PRIVATE ${TEX_CONV_LIBRARY})

add_executable(CubeMapConv ${TEX_CONV_SOURCE} CubeMapConvert.cpp)
target_link_libraries(CubeMapConv PRIVATE ${TEX_CONV_LIBRARY})

add_executable(HDR2PNG HDR2PNG.cpp ${ILIMAGE_SOURCE})
target_link_libraries(HDR2PNG PRIVATE CMCore CMPlatform CMUtil ${HGL_DEVIL_LIB})

IF(CM_BUILD_GUI_TOOLS)
    set(IDENTIFIER "com.hyzgame.texconv")

    target_link_libraries(TexConv PRIVATE Qt${CM_QT_MAJOR_VERSION}::Core Qt${CM_QT_MAJOR_VERSION}::Gui Qt${CM_QT_MAJOR_VERSION}::Widgets)
ENDIF(CM_BUILD_GUI_TOOLS)

add_executable(ComboTexture ComboTexture.cpp ${ILIMAGE_SOURCE})
target_link_libraries(ComboTexture PRIVATE CMCore CMPlatform CMUtil ${HGL_DEVIL_LIB})

#add_executable(YUVTest YUVTest.cpp YUV.cpp SpheremapNormal.cpp ${ILIMAGE_SOURCE})
#target_link_libraries(YUVTest PRIVATE CMCore CMPlatform CMUtil ${HGL_DEVIL_LIB})

#add_executable(NormalTest NormalTest.cpp SpheremapNormal.cpp ${ILIMAGE_SOURCE})
#target_link_libraries(NormalTest PRIVATE CMCore CMPlatform CMUtil ${HGL_DEVIL_LIB})

add_executable(DFGen DistanceFieldGenerater.cpp ILImage.h ILImageSupport.cpp)
target_link_libraries(DFGen PRIVATE CMCore CMPlatform CMUtil ${HGL_DEVIL_LIB})

macro(texture_tool_project project_name)
    set_property(TARGET ${project_name} PROPERTY FOLDER "CM/Tools/Texture")
endmacro()

texture_tool_project(TexConv)
texture_tool_project(CubeMapConv)
texture_tool_project(ComboTexture)
texture_tool_project(DFGen)
#texture_tool_project(IMTest)
texture_tool_project(HDR2PNG)
