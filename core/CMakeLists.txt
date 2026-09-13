# TexConvCore.dll —— 编排核心:流水线 / .Tex2D 容器 / 编码器插件管理。
# 纯 CRT/STL,不链接 hgl;编码器插件运行时按 texenc_*.dll 加载。

add_library(TexConvCore SHARED
    src/internal.h
    src/tex_core_impl.cpp
    src/tex2d_container.cpp
    src/pipeline.cpp
)

target_compile_definitions(TexConvCore PRIVATE TEXCONV_SHARED_BUILD)

target_link_libraries(TexConvCore
    PRIVATE
        TexCommon
        TexImage
)
