# TexConv.exe —— CLI 外壳(零 hgl:只依赖 TexCommon 头 + TexConvCore)。
# 目标名保持 TexConv,输出路径不变(gen_mipmap_level_assets.py 等脚本依赖)。

add_executable(TexConv
    main.cpp
    cmdline.h
    cmdline.cpp
)

target_compile_definitions(TexConv PRIVATE TEXCONV_SHARED_USE)

target_link_libraries(TexConv
    PRIVATE
        TexCommon
        TexConvCore
)

texconv_deploy_im_runtime(TexConv)
