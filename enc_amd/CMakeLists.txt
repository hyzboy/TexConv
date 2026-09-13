# TexEncAMD.dll —— AMD Compressonator 编码器插件(运行时由 TexConvCore LoadLibrary 加载)。

add_library(TexEncAMD SHARED
    texenc_amd.cpp
)

target_compile_definitions(TexEncAMD PRIVATE TEXCONV_SHARED_BUILD)

# AMD_COM_LIB 是按配置选择的导入库名(MT/MD 变体见根 CMakeLists),
# 这里显式补库搜索路径(子目录不继承根目录在 add_subdirectory 之后的 link_directories)
target_link_directories(TexEncAMD PRIVATE ${COMPRESSONATOR_ROOT}/lib/bin/x64)

target_link_libraries(TexEncAMD
    PRIVATE
        TexCommon
        ${AMD_COM_LIB}
)

# Compressonator.h 位于 SDK Include;UseDefinitions.h(本仓库根目录)可能被其引用
target_include_directories(TexEncAMD
    PRIVATE
        ${COMPRESSONATOR_ROOT}/Include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)
