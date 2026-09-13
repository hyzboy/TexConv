# TexImage.dll —— 图像核心(ImageMagick++ 加载/预处理/通道转换)。
# hgl(CMCore/CMPlatform/CMUtil)只允许链接进本 DLL,不得进入其它模块。

add_library(TexImage SHARED
    MagickImage.h
    MagickImageSupport.cpp
    tex_image_impl.cpp
)

target_compile_definitions(TexImage PRIVATE TEXCONV_SHARED_BUILD)

target_link_libraries(TexImage
    PRIVATE
        TexCommon
        CMCore CMPlatform CMUtil
        ${ImageMagick_LIBRARIES}
)

target_include_directories(TexImage PRIVATE ${ImageMagick_INCLUDE_DIRS})
