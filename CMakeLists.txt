cmake_minimum_required(VERSION 3.28)

project(TexConv)

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

    if(MSVC)
        # 为AMD Compressonator库准备所有可能的后缀
        # 注意：实际链接时，只有项目配置匹配的库才会被使用
        set(LIB_SUFFIX_LIST "MD" "MDd" "MT" "MTd")
        message(STATUS "Will support all runtime library variants: ${LIB_SUFFIX_LIST}")
    endif()

    # Find Compressonator include directory
    find_path(COMPRESSONATOR_ROOT
        NAMES include/compressonator.h
        HINTS "C:\\Compressonator_4.5.52"
        PATH_SUFFIXES Include include
    )
    
    include_directories(${COMPRESSONATOR_ROOT}/Include)
    link_directories(${COMPRESSONATOR_ROOT}/lib/bin/${COM_SYSTEM})
    
    # 使用生成器表达式根据配置类型选择库文件
    # 默认使用MD/MDd（DLL运行时），这是Visual Studio的默认设置
    # 如果项目使用MT/MTd，需要在CMakeLists.txt顶部设置：
    # set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # 简化方案：检测是否设置了静态运行时库
    if(CMAKE_MSVC_RUNTIME_LIBRARY AND CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "MultiThreaded[^D]")
        # 使用静态运行时库 (MT/MTd)
        SET(AMD_COM_LIB_DEBUG "Compressonator_MTd.lib")
        SET(AMD_COM_LIB_RELEASE "Compressonator_MT.lib")
        message(STATUS "Using static runtime: MT/MTd")
    else()
        # 使用DLL运行时库 (MD/MDd) - 默认
        SET(AMD_COM_LIB_DEBUG "Compressonator_MDd.lib")
        SET(AMD_COM_LIB_RELEASE "Compressonator_MD.lib")
        message(STATUS "Using DLL runtime: MD/MDd")
    endif()
    
    SET(AMD_COM_LIB 
        $<$<CONFIG:Debug>:${AMD_COM_LIB_DEBUG}>
        $<$<NOT:$<CONFIG:Debug>>:${AMD_COM_LIB_RELEASE}>
    )

    message(STATUS "AMD Compressonator Debug Library: ${AMD_COM_LIB_DEBUG}")
    message(STATUS "AMD Compressonator Release Library: ${AMD_COM_LIB_RELEASE}")

endif(WIN32)

SET(ISPC_TC_HEADER_PATH "${CMAKE_CURRENT_SOURCE_DIR}/ISPCTextureCompressor/ispc_texcomp")

# 使用生成器表达式支持多配置生成器（如Visual Studio）
SET(ISPC_TC_LIBRARY_PATH_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/ISPCTextureCompressor/ISPC Texture Compressor/x64/Debug")
SET(ISPC_TC_LIBRARY_PATH_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/ISPCTextureCompressor/ISPC Texture Compressor/x64/Release")

# Debug和Release共有的库文件
SET(INTEL_TC_LIBRARY_COMMON
    $<$<CONFIG:Debug>:${ISPC_TC_LIBRARY_PATH_DEBUG}/kernel.obj>
    $<$<CONFIG:Debug>:${ISPC_TC_LIBRARY_PATH_DEBUG}/kernel_avx.obj>
    $<$<CONFIG:Debug>:${ISPC_TC_LIBRARY_PATH_DEBUG}/kernel_avx2.obj>
    $<$<CONFIG:Debug>:${ISPC_TC_LIBRARY_PATH_DEBUG}/kernel_sse2.obj>
    $<$<CONFIG:Debug>:${ISPC_TC_LIBRARY_PATH_DEBUG}/kernel_sse4.obj>
    $<$<CONFIG:Debug>:${ISPC_TC_LIBRARY_PATH_DEBUG}/kernel_astc.obj>
    
    $<$<NOT:$<CONFIG:Debug>>:${ISPC_TC_LIBRARY_PATH_RELEASE}/kernel.obj>
    $<$<NOT:$<CONFIG:Debug>>:${ISPC_TC_LIBRARY_PATH_RELEASE}/kernel_avx.obj>
    $<$<NOT:$<CONFIG:Debug>>:${ISPC_TC_LIBRARY_PATH_RELEASE}/kernel_avx2.obj>
    $<$<NOT:$<CONFIG:Debug>>:${ISPC_TC_LIBRARY_PATH_RELEASE}/kernel_sse2.obj>
    $<$<NOT:$<CONFIG:Debug>>:${ISPC_TC_LIBRARY_PATH_RELEASE}/kernel_sse4.obj>
    $<$<NOT:$<CONFIG:Debug>>:${ISPC_TC_LIBRARY_PATH_RELEASE}/kernel_astc.obj>
    $<$<NOT:$<CONFIG:Debug>>:${ISPC_TC_LIBRARY_PATH_RELEASE}/kernel_astc_avx.obj>
    $<$<NOT:$<CONFIG:Debug>>:${ISPC_TC_LIBRARY_PATH_RELEASE}/kernel_astc_avx2.obj>
    $<$<NOT:$<CONFIG:Debug>>:${ISPC_TC_LIBRARY_PATH_RELEASE}/kernel_astc_sse2.obj>
    $<$<NOT:$<CONFIG:Debug>>:${ISPC_TC_LIBRARY_PATH_RELEASE}/kernel_astc_sse4.obj>
)

SET(INTEL_TC_LIBRARY ${INTEL_TC_LIBRARY_COMMON})

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
                                TextureFileCreaterCompressAMD.cpp
                                TextureFileCreaterCompressIntel.cpp)

SET(PARAM_PARSE_SOURCE ParamParse.h ParamParse.cpp)

SOURCE_GROUP("Image File" FILES ${ILIMAGE_SOURCE})
SOURCE_GROUP("Pixel Format" FILES ${PIXEL_FORMAT_SOURCE})
SOURCE_GROUP("Texture File Creater" FILES ${TEXTURE_FILE_CREATER_SOURCE})
SOURCE_GROUP("CMD Param parse" FILES ${PARAM_PARSE_SOURCE})

SET(TEX_CONV_SOURCE ${ILIMAGE_SOURCE} ${PIXEL_FORMAT_SOURCE} ${TEXTURE_FILE_CREATER_SOURCE} ${PARAM_PARSE_SOURCE})
SET(TEX_CONV_LIBRARY CMCore CMPlatform CMUtil ${HGL_DEVIL_LIB} ${INTEL_TC_LIBRARY} ${AMD_COM_LIB})

if(WIN32)
	set(TEX_CONV_LIBRARY ${TEX_CONV_LIBRARY} glm::glm)
else()
	set(TEX_CONV_LIBRARY ${TEX_CONV_LIBRARY} GLM)
endif(WIN32)

add_executable(TexConv ${TEX_CONV_SOURCE} ${INTEL_TC_SOURCE} main.cpp ConvertImage.cpp)
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
