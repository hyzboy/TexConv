#pragma once
// .Tex2D 容器格式常量(迁自原 TextureFileCreater.cpp,格式不变,引擎零改动)。
//
// 头部布局(Tex2D,共 32 字节):
//   "Texture"        7 字节 ASCII
//   version          uint8   (当前 0)
//   view_type        uint8   (VkImageViewType 值,Tex2D=2)
//   width/height/depth  uint32 × 3   (depth 恒 0)
//   格式块:
//     压缩:  uint8(0) + 9 字节格式名(定长,不足补 0)
//     非压缩: uint8 通道数 + color[4] + bits[4] + uint8 vulkan_type
//   mip_level        uint8
// 之后为各级 mip 数据,自大到小;TextureFileCreater::Write 对不足 8 字节的数据补 0 至 8 字节。

#include "texconv_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TEX2D_HEADER_MAGIC      "Texture"
#define TEX2D_HEADER_MAGIC_LEN  7
#define TEX2D_HEADER_SIZE       32      // Tex2D 视图类型的头部长度
#define TEX2D_VERSION           0

// 对齐 VkImageViewType(头部按此值写入;Vulkan 该枚举从 0 开始:1D=0,2D=1)
enum Tex2DViewType
{
    TEX_VIEW_1D        = 0,
    TEX_VIEW_2D        = 1,
    TEX_VIEW_3D        = 2,
    TEX_VIEW_CUBE      = 3,
    TEX_VIEW_1D_ARRAY  = 4,
    TEX_VIEW_2D_ARRAY  = 5,
    TEX_VIEW_CUBE_ARRAY= 6,
};

#ifdef __cplusplus
}
#endif
