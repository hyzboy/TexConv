#pragma once
// 像素数据类型与通道布局。
// TexPixelType 的值序必须与 hgl 的 ImagePixelType(MagickImage.h)保持一致,
// TexImage.dll 内部直接做值映射,不得重排。

#include "texconv_api.h"

enum TexPixelType
{
    TEX_PT_Int8    = 0,
    TEX_PT_UInt8   = 1,
    TEX_PT_Int16   = 2,
    TEX_PT_UInt16  = 3,
    TEX_PT_Int32   = 4,
    TEX_PT_UInt32  = 5,
    TEX_PT_Float16 = 6,
    TEX_PT_Float32 = 7,
    TEX_PT_Float64 = 8,
};

enum TexImageLayout
{
    TEX_LAYOUT_Alpha      = 0,
    TEX_LAYOUT_Gray       = 1,
    TEX_LAYOUT_GrayAlpha  = 2,
    TEX_LAYOUT_RGB        = 3,
    TEX_LAYOUT_RGBA       = 4,
};

inline int TexLayoutChannels(int layout)
{
    switch(layout)
    {
        case TEX_LAYOUT_Alpha:
        case TEX_LAYOUT_Gray:       return 1;
        case TEX_LAYOUT_GrayAlpha:  return 2;
        case TEX_LAYOUT_RGB:        return 3;
        case TEX_LAYOUT_RGBA:       return 4;
        default:                    return 0;
    }
}

inline int TexPixelTypeBytes(int pt)
{
    switch(pt)
    {
        case TEX_PT_Int8:
        case TEX_PT_UInt8:      return 1;
        case TEX_PT_Int16:
        case TEX_PT_UInt16:
        case TEX_PT_Float16:    return 2;
        case TEX_PT_Int32:
        case TEX_PT_UInt32:
        case TEX_PT_Float32:    return 4;
        case TEX_PT_Float64:    return 8;
        default:                return 0;
    }
}
