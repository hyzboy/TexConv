# 测试驱动(不进发布包,EXCLUDE_FROM_ALL,需显式构建)。

add_executable(unit_image EXCLUDE_FROM_ALL
    unit_image.cpp
)

target_compile_definitions(unit_image PRIVATE TEXCONV_SHARED_USE)
target_link_libraries(unit_image PRIVATE TexCommon TexImage)

add_executable(unit_encoders EXCLUDE_FROM_ALL
    unit_encoders.cpp
)

target_compile_definitions(unit_encoders PRIVATE TEXCONV_SHARED_USE)
target_link_libraries(unit_encoders PRIVATE TexCommon TexImage)

add_executable(unit_core EXCLUDE_FROM_ALL
    unit_core.cpp
)

target_compile_definitions(unit_core PRIVATE TEXCONV_SHARED_USE)
target_link_libraries(unit_core PRIVATE TexCommon TexConvCore)

# 引擎侧 .TexCube 加载验证(解析 TextureCubeLoader,纯文件层,不依赖 Vulkan 设备)
add_executable(unit_cubefile EXCLUDE_FROM_ALL
    unit_cubefile.cpp
)

target_link_libraries(unit_cubefile PRIVATE
    ULRE.SceneGraph ULRE.Vulkan ULRE.Work
    CMAssetsManage CM2D CMMath CMCore CMPlatform CMUtil CMCoreType)

add_executable(unit_df EXCLUDE_FROM_ALL
    unit_df.cpp
)

target_compile_definitions(unit_df PRIVATE TEXCONV_SHARED_USE)
target_link_libraries(unit_df PRIVATE TexCommon TexImage)
