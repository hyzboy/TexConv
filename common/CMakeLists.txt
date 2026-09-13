# TexCommon —— 公共静态库:纯 C 接口头 + 格式表,零第三方依赖。
# CLI/Qt GUI/TexConvCore 均只依赖本库的头文件与格式表。

add_library(TexCommon STATIC
    src/format_table.cpp
)

target_include_directories(TexCommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
