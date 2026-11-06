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

if(TARGET TexImage)
    add_dependencies(TexConv TexImage)
endif()
if(TARGET TexEncAMD)
    add_dependencies(TexConv TexEncAMD)
endif()
if(TARGET TexEncIntel)
    add_dependencies(TexConv TexEncIntel)
endif()

texconv_deploy_im_runtime(TexConv)
texconv_copy_to_tools(TexConv)

