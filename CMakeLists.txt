cmake_minimum_required(VERSION 3.28)

project(TexConv)

add_subdirectory(basis_universal)

add_definitions(-DUNICODE -D_UNICODE)

if(WIN32)

    # Find DevIL includes and libraries
    find_path(DEVIL_SDK_DIR
        NAMES include/IL/il.h
        HINTS "${CMAKE_CURRENT_SOURCE_DIR}/DevIL Windows SDK"
        PATH_SUFFIXES include
    )
    include_directories("${DEVIL_SDK_DIR}/include")

    IF(HGL_BITS EQUAL 32)
        SET(DEVIL_LIBRARY_PATH "${DEVIL_SDK_DIR}/lib/x86/unicode/Release")
        SET(COM_SYSTEM x86)
    else()
        SET(DEVIL_LIBRARY_PATH "${DEVIL_SDK_DIR}/lib/x64/unicode/Release")
        SET(COM_SYSTEM x64)
    endif()
 
    SET(HGL_DEVIL_LIB   ${DEVIL_LIBRARY_PATH}/DevIL.lib
                        ${DEVIL_LIBRARY_PATH}/ILU.lib
                        ${DEVIL_LIBRARY_PATH}/ILUT.lib)

endif(WIN32)

SET(ISPC_TC_HEADER_PATH "${CMAKE_CURRENT_SOURCE_DIR}/ISPCTextureCompressor/ispc_texcomp")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    SET(ISPC_TC_LIBRARY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/ISPCTextureCompressor/ISPC Texture Compressor/x64/Debug")

    SET(INTEL_TC_LIBRARY    ${ISPC_TC_LIBRARY_PATH}/kernel.obj                        
                            ${ISPC_TC_LIBRARY_PATH}/kernel_avx.obj
                            ${ISPC_TC_LIBRARY_PATH}/kernel_avx2.obj
                            ${ISPC_TC_LIBRARY_PATH}/kernel_sse2.obj
                            ${ISPC_TC_LIBRARY_PATH}/kernel_sse4.obj

                            ${ISPC_TC_LIBRARY_PATH}/kernel_astc.obj)


else()
    SET(ISPC_TC_LIBRARY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/ISPCTextureCompressor/ISPC Texture Compressor/x64/Release")
    
    SET(INTEL_TC_LIBRARY    ${ISPC_TC_LIBRARY_PATH}/kernel.obj                        
                            ${ISPC_TC_LIBRARY_PATH}/kernel_avx.obj
                            ${ISPC_TC_LIBRARY_PATH}/kernel_avx2.obj
                            ${ISPC_TC_LIBRARY_PATH}/kernel_sse2.obj
                            ${ISPC_TC_LIBRARY_PATH}/kernel_sse4.obj
                            
                            ${ISPC_TC_LIBRARY_PATH}/kernel_astc.obj
                            ${ISPC_TC_LIBRARY_PATH}/kernel_astc_avx.obj
                            ${ISPC_TC_LIBRARY_PATH}/kernel_astc_avx2.obj
                            ${ISPC_TC_LIBRARY_PATH}/kernel_astc_sse2.obj
                            ${ISPC_TC_LIBRARY_PATH}/kernel_astc_sse4.obj)
endif()

SET(INTEL_TC_SOURCE ${ISPC_TC_HEADER_PATH}/ispc_texcomp.cpp
					${ISPC_TC_HEADER_PATH}/ispc_texcomp.h
					${ISPC_TC_HEADER_PATH}/ispc_texcomp_astc.cpp)

SOURCE_GROUP("Intel ISPC Texture Compression" FILES ${INTEL_TC_SOURCE})

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
                                TextureFileCreaterCompress.cpp
                                TextureFileCreaterBasisU.cpp)

SET(PARAM_PARSE_SOURCE ParamParse.h ParamParse.cpp)

SOURCE_GROUP("Image File" FILES ${ILIMAGE_SOURCE})
SOURCE_GROUP("Pixel Format" FILES ${PIXEL_FORMAT_SOURCE})
SOURCE_GROUP("Texture File Creater" FILES ${TEXTURE_FILE_CREATER_SOURCE})
SOURCE_GROUP("CMD Param parse" FILES ${PARAM_PARSE_SOURCE})

SET(TEX_CONV_SOURCE ${ILIMAGE_SOURCE} ${PIXEL_FORMAT_SOURCE} ${TEXTURE_FILE_CREATER_SOURCE} ${PARAM_PARSE_SOURCE})
SET(TEX_CONV_LIBRARY CMCore CMPlatform CMUtil ${HGL_DEVIL_LIB} ${INTEL_TC_LIBRARY})

if(WIN32)
	set(TEX_CONV_LIBRARY ${TEX_CONV_LIBRARY} glm::glm)
else()
	set(TEX_CONV_LIBRARY ${TEX_CONV_LIBRARY} GLM)
endif(WIN32)

add_executable(TexConv ${TEX_CONV_SOURCE} ${INTEL_TC_SOURCE} main.cpp ConvertImage.cpp)
target_link_libraries(TexConv PRIVATE ${TEX_CONV_LIBRARY} basisu_encoder )

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
