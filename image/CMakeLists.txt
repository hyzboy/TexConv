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

# ImageMagick(C++ 接口)上游不支持 MSVC Debug 语义运行(动态 coder 模块加载失败,
# "no decode delegate")。TexImage 是 IM 的唯一封装层、对外纯 C ABI、CRT 类型不跨
# 边界——故恒按 Release 配置构建;Debug 宿主(TexConvCore/TexConv/TexConvQt)
# 链接 Release 产物(见根 CMakeLists 的 texconv_link_teximage)。
# 注意:Debug 构建前需先完成一次 Release 构建(提供 TexImage.lib/dll)。
set_target_properties(TexImage PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_DEBUG TRUE)
