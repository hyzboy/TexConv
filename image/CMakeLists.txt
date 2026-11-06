# TexImage.dll —— 图像核心(ImageMagick++ 加载/预处理/通道转换)。
#
# 本 DLL 不依赖 hgl:它只用 STL/CRT,并与 ImageMagick 一样使用**动态 CRT**(/MD),
# 因此 Debug/Release 都能自足构建,Debug 宿主直接链 Debug 版即可。
#
# 历史:早先本 DLL 链 hgl(全工程 /MT)且被钉死为 Release,是因为 /MT 与 IM 的 /MD
# 不能混编;摘掉 hgl 后这个限制消失,那套「恒 Release + Debug 宿主链 Release
# import lib + 必须"先 Release 后 Debug"」的规避手段也随之全部删除。

add_library(TexImage SHARED
    MagickImage.h
    MagickImageSupport.cpp
    ImageLog.h
    ImageLog.cpp
    tex_image_impl.cpp
)

target_compile_definitions(TexImage PRIVATE TEXCONV_SHARED_BUILD)

# 与 ImageMagick 保持同一运行时库:任何配置都用 /MD(而不是 Debug 的 /MDd)。
# 这样 _ITERATOR_DEBUG_LEVEL 恒为 0,跨 DLL 边界传递 Magick::Image /
# Magick::Exception 时 ABI 才真正一致 —— 这是本 DLL 能与 IM 混用动态 CRT 的前提。
set_target_properties(TexImage PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

target_link_libraries(TexImage
    PRIVATE
        TexCommon
        ${ImageMagick_LIBRARIES}
)

target_include_directories(TexImage PRIVATE ${ImageMagick_INCLUDE_DIRS})

texconv_copy_to_tools(TexImage)