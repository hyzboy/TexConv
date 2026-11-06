cmake_minimum_required(VERSION 3.28)

project(TexConv)

add_definitions(-DUNICODE -D_UNICODE)

# 调试支持:Release 也生成 PDB(不影响产物行为)
if(MSVC)
    add_compile_options(/Zi)
    add_link_options($<$<CONFIG:Release>:/DEBUG>)
endif()

# ---------------------------------------------------------------------------
# 独立配置支持:
#
#   TexImage.dll 摘掉 hgl 后,TexConv 整棵树已是**零 hgl 依赖**,不再需要根工程
#   的预编译产物 —— 可直接在本目录独立配置构建,无需先构建整个 ULRE。
#   从根工程配置进来时(CMCore target 已存在)以下全部跳过。
#
#   用法(在 TexConv 目录下):
#     cmake -B build -S . -G "Visual Studio 18 2026" -A x64 ^
#           -DCMAKE_TOOLCHAIN_FILE=E:/vcpkg/scripts/buildsystems/vcpkg.cmake
#     cmake --build build --config Release
#   产物输出到 build/out/Windows_64_{Debug,Release}(与根工程同一约定)。
#   生成 TexConv.sln,内含 TexCommon/TexImage/TexEncAMD/TexEncIntel/TexConvCore/TexConv
#   六个工程(按 CM/Tools/Texture 分组),可整体构建与调试。
#
#   例外:tests/unit_cubefile 用引擎侧 TextureCubeLoader 反查产出文件,只在根工程内
#   配置时才生成(见 tests/CMakeLists.txt 的 TARGET 判断)。
# ---------------------------------------------------------------------------
if(NOT TARGET CMCore)
    set(TEXCONV_STANDALONE ON)

    # C++ 标准与根工程一致(compiler.cmake 的约定)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    # 固定静态 CRT(MT/MTd),与 Compressonator 的 MT 变体导入库保持同一 CRT
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    set(ULRE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

    # 输出目录与根工程同一约定(build/out/Windows_64_{Debug,Release})
    include("${ULRE_ROOT}/CMCMakeModule/system_bit.cmake")
    include("${ULRE_ROOT}/CMCMakeModule/output_path.cmake")

    message(STATUS "TexConv: standalone configure (no hgl dependency)")
endif()

# ImageMagick 缺失时只跳过 TexConv,不 FATAL —— 它是工具链里的一支,不该把整个
# 引擎的 configure 卡死(vcpkg/CI 机器上常常没装 IM)。
find_package(ImageMagick COMPONENTS Magick++ QUIET)

if(NOT ImageMagick_FOUND)
    message(STATUS
        "[TexConv] 未找到 ImageMagick(Magick++)，跳过 TexConv 构建。\n"
        "         安装含 Magick++ 的 ImageMagick 后重新配置，或用 -DImageMagick_DIR=<dir> 指定；\n"
        "         不需要 TexConv 时加 -DULRE_ENABLE_TEXCONV=OFF 可把本子树整体摘掉。")
    return()
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

    # Compressonator 缺失时只跳过 AMD 后端:TexConvCore 运行时枚举不到该插件,
    # 会明确报"没有可用编码器",而不是在链接期以一条难懂的 LNK 报错收场。
    if(COMPRESSONATOR_ROOT)
        set(TEXCONV_HAVE_COMPRESSONATOR ON)

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
    else()
        message(STATUS
            "[TexConv] 未找到 AMD Compressonator SDK(compressonator.h)，跳过 TexEncAMD 构建。")
    endif()

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
# TexImage 链接辅助。
#
# TexImage.dll 是唯一与 ImageMagick 打交道的层,它与 IM 一样用**动态 CRT**
# (/MD,见 image/CMakeLists.txt),因此 Debug/Release 都能自足构建 ——
# 不再有"TexImage 恒 Release、Debug 宿主链 Release import lib"的特例,
# 也就不再需要"先 Release 后 Debug"的构建顺序。
# ---------------------------------------------------------------------------
function(texconv_link_teximage TARGET_NAME)
    target_link_libraries(${TARGET_NAME} PRIVATE TexImage)

    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:TexImage>"
                "$<TARGET_FILE_DIR:${TARGET_NAME}>"
        COMMENT "Deploy $<TARGET_FILE_NAME:TexImage> -> $<TARGET_FILE_DIR:${TARGET_NAME}>")

    if(ULRE_TOOLS_TEXCONV_DIR)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:TexImage>"
                    "${ULRE_TOOLS_TEXCONV_DIR}/$<TARGET_FILE_NAME:TexImage>"
            COMMENT "Deploy $<TARGET_FILE_NAME:TexImage> -> ${ULRE_TOOLS_TEXCONV_DIR}")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# TexConv 模块化架构:
#   TexCommon    公共 C 接口头 / 格式表(静态库)
#   TexImage     图像核心(ImageMagick++;纯 STL/CRT,与 IM 同为 /MD)
#   TexEncAMD    AMD Compressonator 编码器插件
#   TexEncIntel  Intel ISPC 编码器插件(kernel*.obj 缺失时自动跳过构建)
#   TexConvCore  编排核心:流水线 / .Tex2D/.TexOct/.TexCube 容器 / 插件管理(运行时加载编码器)
#   TexConv      CLI 外壳
# 必须在 Compressonator 探测之后 add_subdirectory,子目录才能取到
# COMPRESSONATOR_ROOT / AMD_COM_LIB。
# ---------------------------------------------------------------------------
add_subdirectory(common)
add_subdirectory(image)
if(TEXCONV_HAVE_COMPRESSONATOR)
    add_subdirectory(enc_amd)
endif()
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
