# TexEncIntel.dll —— Intel ISPC 编码器插件。
# kernel*.obj 必须由 ispc 编译器生成(download_*_ispc 脚本可拉取编译器);
# 缺失时不构建本目标,Core 运行时枚举不到 Intel 后端(等价旧版 fail-fast 语义)。

set(ISPC_TC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../ISPCTextureCompressor")
set(ISPC_TC_HEADER_PATH "${ISPC_TC_ROOT}/ispc_texcomp")
set(ISPC_TC_LIB_DIR_DEBUG   "${ISPC_TC_ROOT}/ISPC Texture Compressor/x64/Debug")
set(ISPC_TC_LIB_DIR_RELEASE "${ISPC_TC_ROOT}/ISPC Texture Compressor/x64/Release")

if(EXISTS "${ISPC_TC_LIB_DIR_DEBUG}/kernel.obj" OR EXISTS "${ISPC_TC_LIB_DIR_RELEASE}/kernel.obj")
    message(STATUS "TexConv: ISPCTextureCompressor kernels found -> build TexEncIntel.dll")

    add_library(TexEncIntel SHARED
        texenc_intel.cpp
        ${ISPC_TC_HEADER_PATH}/ispc_texcomp.cpp
        ${ISPC_TC_HEADER_PATH}/ispc_texcomp.h
        ${ISPC_TC_HEADER_PATH}/ispc_texcomp_astc.cpp
    )

    target_compile_definitions(TexEncIntel PRIVATE TEXCONV_SHARED_BUILD)

    target_include_directories(TexEncIntel PRIVATE ${ISPC_TC_ROOT})

    target_link_libraries(TexEncIntel PRIVATE TexCommon)

    # kernel*.obj 按配置链接(与旧版 CMakeLists 的清单一致)
    set(ISPC_KERNEL_DEBUG
        "${ISPC_TC_LIB_DIR_DEBUG}/kernel.obj"
        "${ISPC_TC_LIB_DIR_DEBUG}/kernel_avx.obj"
        "${ISPC_TC_LIB_DIR_DEBUG}/kernel_avx2.obj"
        "${ISPC_TC_LIB_DIR_DEBUG}/kernel_sse2.obj"
        "${ISPC_TC_LIB_DIR_DEBUG}/kernel_sse4.obj"
        "${ISPC_TC_LIB_DIR_DEBUG}/kernel_astc.obj"
    )

    set(ISPC_KERNEL_RELEASE
        "${ISPC_TC_LIB_DIR_RELEASE}/kernel.obj"
        "${ISPC_TC_LIB_DIR_RELEASE}/kernel_avx.obj"
        "${ISPC_TC_LIB_DIR_RELEASE}/kernel_avx2.obj"
        "${ISPC_TC_LIB_DIR_RELEASE}/kernel_sse2.obj"
        "${ISPC_TC_LIB_DIR_RELEASE}/kernel_sse4.obj"
        "${ISPC_TC_LIB_DIR_RELEASE}/kernel_astc.obj"
        "${ISPC_TC_LIB_DIR_RELEASE}/kernel_astc_avx.obj"
        "${ISPC_TC_LIB_DIR_RELEASE}/kernel_astc_avx2.obj"
        "${ISPC_TC_LIB_DIR_RELEASE}/kernel_astc_sse2.obj"
        "${ISPC_TC_LIB_DIR_RELEASE}/kernel_astc_sse4.obj"
    )

    target_link_libraries(TexEncIntel PRIVATE
        $<$<CONFIG:Debug>:${ISPC_KERNEL_DEBUG}>
        $<$<NOT:$<CONFIG:Debug>>:${ISPC_KERNEL_RELEASE}>
    )
else()
    message(STATUS "TexConv: ISPCTextureCompressor kernels NOT found -> TexEncIntel.dll skipped")
endif()
