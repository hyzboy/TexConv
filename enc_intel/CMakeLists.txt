# TexEncIntel.dll —— Intel ISPC 编码器插件。
#
# 需要两类输入:
#   1. C++ 侧  ISPCTextureCompressor/ispc_texcomp/{ispc_texcomp,ispc_texcomp_astc}.cpp
#   2. ISPC 侧 kernel.ispc / kernel_astc.ispc —— 必须由 ispc 编译器编成 .obj,
#      并同时产出 kernel_ispc.h / kernel_astc_ispc.h(上面那两个 .cpp 会 include)
#
# kernel 由本文件在**构建期**直接调用 ispc.exe 生成,不走上游 ISPCTextureCompressor
# 的 ispc_texcomp.vcxproj:那个工程是 v142 + Win32/x64 四配置,且把产物写进源码树
# (x64\{Debug,Release}\ 与 "ISPC Texture Compressor/" 下),与本仓库
# (VS 18 2026 / 仅 x64)不兼容,多人协作时还会互相覆盖。
#
# 生成目录:<build>/.../enc_intel/ispc_kernels/。ispc 产物与 MSVC 配置无关,
# 因此 Debug/Release 共用同一份,只生成一次。
#
# 找不到 ispc.exe 时不构建本目标 —— Core 运行时枚举不到 Intel 后端,
# 与旧版 fail-fast 语义一致(AMD 后端仍可用)。
# 编译器获取:在 TexConv 目录执行 download_win_ispc.bat(装到 ISPCTextureCompressor/ISPC/win)。

set(ISPC_TC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../ISPCTextureCompressor")
set(ISPC_TC_SRC  "${ISPC_TC_ROOT}/ispc_texcomp")

# ispc.exe 查找顺序:CMake 变量 > 环境变量 ISPC_EXE > download_win_ispc.bat 的安装位置
set(TEXCONV_ISPC_EXE "" CACHE FILEPATH
    "ispc.exe used to compile ISPCTextureCompressor kernels (see download_win_ispc.bat)")

if(NOT TEXCONV_ISPC_EXE)
    if(DEFINED ENV{ISPC_EXE} AND EXISTS "$ENV{ISPC_EXE}")
        file(TO_CMAKE_PATH "$ENV{ISPC_EXE}" TEXCONV_ISPC_EXE)
    elseif(EXISTS "${ISPC_TC_ROOT}/ISPC/win/ispc.exe")
        set(TEXCONV_ISPC_EXE "${ISPC_TC_ROOT}/ISPC/win/ispc.exe")
    endif()
endif()

# 与上游 vcxproj 一致:每个 kernel 生成 1 个分派 obj + 每个目标集 1 个 obj
set(ISPC_KERNEL_TARGETS sse2 sse4 avx avx2)
set(ISPC_KERNEL_NAMES   kernel kernel_astc)
set(ISPC_KERNEL_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/ispc_kernels")

string(REPLACE ";" "," ISPC_KERNEL_TARGET_LIST "${ISPC_KERNEL_TARGETS}")

if(TEXCONV_ISPC_EXE AND EXISTS "${TEXCONV_ISPC_EXE}")

    message(STATUS "TexConv: ispc found (${TEXCONV_ISPC_EXE}) -> build TexEncIntel.dll")

    set(ISPC_KERNEL_OBJS    "")
    set(ISPC_KERNEL_HEADERS "")

    foreach(_k IN LISTS ISPC_KERNEL_NAMES)

        set(_objs "${ISPC_KERNEL_OUT_DIR}/${_k}.obj")
        set(_hdrs "${ISPC_KERNEL_OUT_DIR}/${_k}_ispc.h")

        foreach(_t IN LISTS ISPC_KERNEL_TARGETS)
            list(APPEND _objs "${ISPC_KERNEL_OUT_DIR}/${_k}_${_t}.obj")
            list(APPEND _hdrs "${ISPC_KERNEL_OUT_DIR}/${_k}_ispc_${_t}.h")
        endforeach()

        # OUTPUT 必须把 .obj 与 .h 都列全:obj 进链接行(驱动 CustomBuild 生成),
        # h 供编译 .cpp 时 include —— 两者都缺一不可。
        # DEPENDS 里带上 ispc.exe:换编译器版本时 kernel 也会重新生成。
        add_custom_command(
            OUTPUT ${_objs} ${_hdrs}
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${ISPC_KERNEL_OUT_DIR}"
            COMMAND "${TEXCONV_ISPC_EXE}"
                    -O2 "${_k}.ispc"
                    -o "${ISPC_KERNEL_OUT_DIR}/${_k}.obj"
                    -h "${ISPC_KERNEL_OUT_DIR}/${_k}_ispc.h"
                    --target=${ISPC_KERNEL_TARGET_LIST}
                    --opt=fast-math
            DEPENDS "${ISPC_TC_SRC}/${_k}.ispc" "${TEXCONV_ISPC_EXE}"
            WORKING_DIRECTORY "${ISPC_TC_SRC}"
            COMMENT "ISPC: ${_k}.ispc -> ispc_kernels/${_k}_*.obj"
            VERBATIM)

        list(APPEND ISPC_KERNEL_OBJS    ${_objs})
        list(APPEND ISPC_KERNEL_HEADERS ${_hdrs})

    endforeach()

    add_library(TexEncIntel SHARED
        texenc_intel.cpp
        ${ISPC_TC_SRC}/ispc_texcomp.cpp
        ${ISPC_TC_SRC}/ispc_texcomp.h
        ${ISPC_TC_SRC}/ispc_texcomp_astc.cpp
    )

    target_compile_definitions(TexEncIntel PRIVATE TEXCONV_SHARED_BUILD)

    target_include_directories(TexEncIntel
        PRIVATE
            ${ISPC_TC_ROOT}          # ispc_texcomp/ispc_texcomp.h
            ${ISPC_KERNEL_OUT_DIR}   # kernel_ispc.h / kernel_astc_ispc.h(ispc 生成)
    )

    target_link_libraries(TexEncIntel PRIVATE TexCommon)

    # kernel*.obj 直接进链接行(同旧版做法):它们同时是上面 add_custom_command 的
    # OUTPUT ⇒ VS 会在 ClCompile 之前先跑 CustomBuild,头文件必定已就位。
    # OBJECT_DEPENDS 再显式约束一次编译顺序(其他生成器/并行构建更稳)。
    target_link_libraries(TexEncIntel PRIVATE ${ISPC_KERNEL_OBJS})

    set_source_files_properties(
        ${ISPC_TC_SRC}/ispc_texcomp.cpp
        ${ISPC_TC_SRC}/ispc_texcomp_astc.cpp
        PROPERTIES OBJECT_DEPENDS "${ISPC_KERNEL_HEADERS}")

    texconv_copy_to_tools(TexEncIntel)

else()

    message(STATUS
        "TexConv: ispc.exe not found -> TexEncIntel.dll skipped\n"
        "         运行 src/Tools/TexConv/download_win_ispc.bat,"
        " 或 -DTEXCONV_ISPC_EXE=<path> / 环境变量 ISPC_EXE")

endif()