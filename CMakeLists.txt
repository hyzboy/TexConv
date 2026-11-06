cmake_minimum_required(VERSION 3.28)

project(TexConv)

add_definitions(-DUNICODE -D_UNICODE)

# 调试支持:Release 也生成 PDB(不影响产物行为)
if(MSVC)
    add_compile_options(/Zi)
    add_link_options($<$<CONFIG:Release>:/DEBUG>)
endif()

# ---------------------------------------------------------------------------
# 独立配置支持(2026-09 重构后重新可用):
#
#   重构后 hgl 仅链入 TexImage.dll,独立配置只需引用根工程"预编译"的 hgl 静态库,
#   不再需要构建整个 ULRE。从根工程配置进来时(CMCore target 已存在)以下全部跳过。
#
#   用法(在 TexConv 目录下):
#     cmake -B build -S . -G "Visual Studio 18 2026" -A x64 ^
#           -DCMAKE_TOOLCHAIN_FILE=E:/vcpkg/scripts/buildsystems/vcpkg.cmake
#     cmake --build build --config Release
#   产物输出到 build/out/Windows_64_{Debug,Release}(与根工程同一约定)。
#   生成 TexConv.sln,内含 TexCommon/TexImage/TexEncAMD/TexEncIntel/TexConvCore/TexConv
#   六个工程(按 CM/Tools/Texture 分组),可整体构建与调试。
#
#   前提:根工程已至少构建过一次(存在 build/out 下的 hgl 静态库)。
# ---------------------------------------------------------------------------
if(NOT TARGET CMCore)
    set(TEXCONV_STANDALONE ON)

    # C++ 标准与根工程一致(compiler.cmake 的约定)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    # 运行时库与预编译 hgl 静态库一致(MT/MTd),否则 LNK2038
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    # GLM 类型布局宏(必须与预编译 CMCore 等库编译时一致)
    add_compile_definitions(GLM_FORCE_RADIANS
                            GLM_FORCE_DEPTH_ZERO_TO_ONE
                            GLM_ENABLE_EXPERIMENTAL
                            GLM_FORCE_DEFAULT_ALIGNED_GENTYPES)
    if(WIN32)
        add_compile_definitions(GLM_FORCE_AVX2)
    endif()

    set(ULRE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

    # 输出目录与根工程同一约定(build/out/Windows_64_{Debug,Release})
    include("${ULRE_ROOT}/CMCMakeModule/system_bit.cmake")
    include("${ULRE_ROOT}/CMCMakeModule/output_path.cmake")

    # absl(vcpkg;CMCore 公开头文件直接包含 absl 头)
    if(NOT DEFINED absl_DIR AND EXISTS "E:/vcpkg/installed/x64-windows/share/absl")
        set(absl_DIR "E:/vcpkg/installed/x64-windows/share/absl")
    endif()
    find_package(absl CONFIG REQUIRED)

    # 引用根工程预编译的 hgl 静态库
    set(HGL_OUT_DEBUG   "${ULRE_ROOT}/build/out/${CMAKE_SYSTEM_NAME}_${HGL_BITS}_Debug")
    set(HGL_OUT_RELEASE "${ULRE_ROOT}/build/out/${CMAKE_SYSTEM_NAME}_${HGL_BITS}_Release")

    foreach(_lib CMCoreType CMPlatform CMUtil CMCore)
        add_library(${_lib} STATIC IMPORTED)
        set_target_properties(${_lib} PROPERTIES
            IMPORTED_LOCATION                 "${HGL_OUT_RELEASE}/${_lib}.lib"
            IMPORTED_LOCATION_DEBUG           "${HGL_OUT_DEBUG}/${_lib}.lib"
            IMPORTED_LOCATION_RELEASE         "${HGL_OUT_RELEASE}/${_lib}.lib"
            IMPORTED_LOCATION_RELWITHDEBINFO  "${HGL_OUT_RELEASE}/${_lib}.lib"
            IMPORTED_LOCATION_MINSIZEREL      "${HGL_OUT_RELEASE}/${_lib}.lib"
        )
    endforeach()

    # 对齐根工程 CMCore target 的公开依赖
    target_link_libraries(CMCore INTERFACE
        CMCoreType CMPlatform
        absl::btree absl::flat_hash_map absl::node_hash_map)

    # hgl 头文件路径
    include_directories(
        "${ULRE_ROOT}/inc"
        "${ULRE_ROOT}/CMCoreType/inc"
        "${ULRE_ROOT}/CMMath/inc"
        "${ULRE_ROOT}/CMCore/inc"
        "${ULRE_ROOT}/CMUtil/inc"
    )

    message(STATUS "TexConv: standalone configure (hgl libs from ${HGL_OUT_RELEASE})")
endif()

find_package(ImageMagick COMPONENTS Magick++ REQUIRED)

if(NOT ImageMagick_FOUND)
    message(FATAL_ERROR "ImageMagick not found. Please install ImageMagick with Magick++ support.")
endif()

SET(IMAGE_LIBS ${ImageMagick_LIBRARIES})

message(STATUS "Using ImageMagick for image processing")
message(STATUS "ImageMagick include dirs: ${ImageMagick_INCLUDE_DIRS}")
message(STATUS "ImageMagick libraries: ${ImageMagick_LIBRARIES}")

if(WIN32)

    # Find Compressonator include directory
    find_path(COMPRESSONATOR_ROOT
        NAMES include/compressonator.h
        HINTS "C:/Compressonator_4.5.52"
        PATH_SUFFIXES Include include
    )

    include_directories(${COMPRESSONATOR_ROOT}/Include)

    # 按运行时库变体选择 Compressonator 导入库
    if(CMAKE_MSVC_RUNTIME_LIBRARY AND CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "MultiThreaded[^D]")
        SET(AMD_COM_LIB_DEBUG "Compressonator_MTd.lib")
        SET(AMD_COM_LIB_RELEASE "Compressonator_MT.lib")
        message(STATUS "Using static runtime: MT/MTd")
    else()
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

# ---------------------------------------------------------------------------
# ImageMagick 运行时随包部署:把 IM 安装目录的 DLL/配置 XML/modules 拷到
# 各 exe 旁,使 Debug/Release 行为一致,不再依赖系统 PATH。
# (Debug 曾因找不到 coders 模块报 "no decode delegate")
# ---------------------------------------------------------------------------
set(IM_RUNTIME_ROOT "")

if(ImageMagick_Magick++_LIBRARY)
    get_filename_component(IM_LIB_DIR "${ImageMagick_Magick++_LIBRARY}" DIRECTORY)
    get_filename_component(IM_ROOT_CANDIDATE "${IM_LIB_DIR}" DIRECTORY)

    if(EXISTS "${IM_ROOT_CANDIDATE}/modules/coders")
        set(IM_RUNTIME_ROOT "${IM_ROOT_CANDIDATE}")
    elseif(EXISTS "${IM_LIB_DIR}/modules/coders")
        set(IM_RUNTIME_ROOT "${IM_LIB_DIR}")
    endif()
endif()

if(NOT DEFINED ULRE_TOOLS_TEXCONV_DIR)
    if(TEXCONV_STANDALONE)
        set(ULRE_TOOLS_TEXCONV_DIR "${ULRE_ROOT}/Tools/TexConv")
    else()
        set(ULRE_TOOLS_TEXCONV_DIR "${CMAKE_SOURCE_DIR}/Tools/TexConv")
    endif()
endif()

function(texconv_copy_to_tools TARGET_NAME)
    if(NOT ULRE_TOOLS_TEXCONV_DIR)
        return()
    endif()

    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ULRE_TOOLS_TEXCONV_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${TARGET_NAME}>"
                "${ULRE_TOOLS_TEXCONV_DIR}/$<TARGET_FILE_NAME:${TARGET_NAME}>"
        COMMENT "Copy $<TARGET_FILE_NAME:${TARGET_NAME}> -> ${ULRE_TOOLS_TEXCONV_DIR}")
endfunction()

function(texconv_deploy_im_runtime TARGET_NAME)
    if(NOT IM_RUNTIME_ROOT)
        return()
    endif()

    file(GLOB IM_DLLS "${IM_RUNTIME_ROOT}/*.dll")
    file(GLOB IM_XMLS "${IM_RUNTIME_ROOT}/*.xml")

    foreach(_f IN LISTS IM_DLLS IM_XMLS)
        get_filename_component(_n "${_f}" NAME)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_f}" "$<TARGET_FILE_DIR:${TARGET_NAME}>/${_n}")
    endforeach()

    if(EXISTS "${IM_RUNTIME_ROOT}/modules")
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${IM_RUNTIME_ROOT}/modules" "$<TARGET_FILE_DIR:${TARGET_NAME}>/modules"
            COMMENT "Deploy ImageMagick modules -> $<TARGET_FILE_DIR:${TARGET_NAME}>/modules")
    endif()

    if(ULRE_TOOLS_TEXCONV_DIR)
        foreach(_f IN LISTS IM_DLLS IM_XMLS)
            get_filename_component(_n "${_f}" NAME)
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_f}" "${ULRE_TOOLS_TEXCONV_DIR}/${_n}")
        endforeach()

        if(EXISTS "${IM_RUNTIME_ROOT}/modules")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${IM_RUNTIME_ROOT}/modules" "${ULRE_TOOLS_TEXCONV_DIR}/modules"
                COMMENT "Deploy ImageMagick modules -> ${ULRE_TOOLS_TEXCONV_DIR}/modules")
        endif()
    endif()
endfunction()

# ---------------------------------------------------------------------------
# TexImage 链接辅助:TexImage.dll 恒按 Release 构建(ImageMagick 上游不支持
# MSVC Debug 语义);Debug 宿主链接 Release 的 import lib,并把 Release 的
# TexImage.dll 拷到自身运行目录(经 C ABI 调用,CRT 完全隔离)。
# ---------------------------------------------------------------------------
set(TEXCONV_RELEASE_TEXIMAGE_LIB "${OUTPUT_DIRECTORY_RELEASE}/TexImage.lib")
set(TEXCONV_RELEASE_TEXIMAGE_DLL "${OUTPUT_DIRECTORY_RELEASE}/TexImage.dll")

function(texconv_link_teximage TARGET_NAME)
    # 非 Debug 配置链接 TexImage target，CMake 会自动建立构建依赖；
    # Debug 链接的是 Release import lib 的字面路径，构建依赖由基线脚本前置保证，
    # 这里不能 add_dependencies(TexImage) —— 那会在 Debug 下编出一个没人链接的
    # Debug 版 TexImage（/MTd 混 IM 的 /MT，随时可能 LNK2038），纯浪费。
    target_link_libraries(${TARGET_NAME} PRIVATE
        $<$<CONFIG:Debug>:${TEXCONV_RELEASE_TEXIMAGE_LIB}>
        $<$<NOT:$<CONFIG:Debug>>:TexImage>)

    # Debug 宿主链的是 **Release** 的 import lib —— 它只在做过一次 Release 构建后才存在。
    # 缺它时链接器只吐一句难懂的 LNK1104（全量构建 EXIT=1 的固定来源），
    # 所以在 configure 阶段就把原因和修复命令说清楚。
    # scripts/ci/build-baseline.ps1 已把这一步做成自动前置。
    if(NOT EXISTS "${TEXCONV_RELEASE_TEXIMAGE_LIB}")
        message(WARNING
            "[TexConv] 缺少 Release 版 TexImage.lib\n"
            "        路径：${TEXCONV_RELEASE_TEXIMAGE_LIB}\n"
            "        原因：TexImage.dll 恒按 Release 构建（ImageMagick 上游只提供 /MT 版，\n"
            "              Debug 编译会撞 RuntimeLibrary / _ITERATOR_DEBUG_LEVEL 不匹配），\n"
            "              Debug 宿主必须链它的 import lib ⇒ 任何 Debug 构建之前需先做一次 Release 构建。\n"
            "        修复：${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --config Release --target TexImage\n"
            "        做完这一次之后，全量 Debug 构建即不再出现该 LNK1104。")
    endif()

    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${TEXCONV_RELEASE_TEXIMAGE_DLL}"
                $<TARGET_FILE_DIR:${TARGET_NAME}>
        COMMENT "Deploy Release TexImage.dll -> $<TARGET_FILE_DIR:${TARGET_NAME}>")

    if(ULRE_TOOLS_TEXCONV_DIR)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${TEXCONV_RELEASE_TEXIMAGE_DLL}"
                    "${ULRE_TOOLS_TEXCONV_DIR}/TexImage.dll"
            COMMENT "Deploy Release TexImage.dll -> ${ULRE_TOOLS_TEXCONV_DIR}")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# TexConv 模块化架构:
#   TexCommon    公共 C 接口头 / 格式表(静态库)
#   TexImage     图像核心(ImageMagick++;hgl 仅链入此 DLL)
#   TexEncAMD    AMD Compressonator 编码器插件
#   TexEncIntel  Intel ISPC 编码器插件(kernel*.obj 缺失时自动跳过构建)
#   TexConvCore  编排核心:流水线 / .Tex2D/.TexOct/.TexCube 容器 / 插件管理(运行时加载编码器)
#   TexConv      CLI 外壳
# 必须在 Compressonator 探测之后 add_subdirectory,子目录才能取到
# COMPRESSONATOR_ROOT / AMD_COM_LIB。
# ---------------------------------------------------------------------------
add_subdirectory(common)
add_subdirectory(image)
add_subdirectory(enc_amd)
add_subdirectory(enc_intel)
add_subdirectory(core)
add_subdirectory(cli)
add_subdirectory(gui)
add_subdirectory(tests)

# 旧单体实现(附加工具/独立配置支持)已移除;
# HDR2PNG/ComboTexture/DFGen 等附加工具源码保留在根目录,待基于 TexImage.dll 复活。

# ---------------------------------------------------------------------------
# VS 解决方案分组:六个工程同组(根解决方案与独立 TexConv.sln 均生效),
# 独立配置时 TexConv 设为启动工程。
# ---------------------------------------------------------------------------
set(TEXCONV_FOLDER "CM/Tools/Texture")

foreach(_t TexCommon TexImage TexEncAMD TexEncIntel TexConvCore TexConv)
    if(TARGET ${_t})
        set_target_properties(${_t} PROPERTIES FOLDER "${TEXCONV_FOLDER}")
    endif()
endforeach()

foreach(_t unit_image unit_encoders unit_core unit_df)
    if(TARGET ${_t})
        set_target_properties(${_t} PROPERTIES FOLDER "${TEXCONV_FOLDER}/Tests")
    endif()
endforeach()

if(TARGET TexConvQt)
    set_target_properties(TexConvQt PROPERTIES FOLDER "${TEXCONV_FOLDER}")
endif()

if(TEXCONV_STANDALONE)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT TexConv)
endif()
