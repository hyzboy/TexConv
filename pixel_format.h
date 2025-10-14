#pragma once

#include<hgl/type/DataType.h>
#include<hgl/graph/VKFormat.h>

using namespace hgl;
using namespace hgl::graph;

enum class ColorFormat
{
    NONE=0,

    RGBA4,
    BGRA4,
    RGB565,
    A1RGB5,
    R8,
    RG8,
    RGBA8,
    RGBA8SN,
    RGBA8U,
    RGBA8I,
    ABGR8,
    A2BGR10,
    R16,
    R16U,
    R16I,
    R16F,
    RG16,
    RG16U,
    RG16I,
    RG16F,
    RGBA16,
    RGBA16SN,
    RGBA16U,
    RGBA16I,
    RGBA16F,
    R32U,
    R32I,
    R32F,
    RG32U,
    RG32I,
    RG32F,
    RGB32U,
    RGB32I,
    RGB32F,
    RGBA32U,
    RGBA32I,
    RGBA32F,
    B10GR11UF,

    COMPRESS,

    BC1RGB,
    BC1RGBA,
    BC2,
    BC3,
    BC4,
    BC5,
    BC6H,
    BC6H_SF,
    BC7,

    END
};//enum class ColorFormat

struct ColorDataFormat
{
    char                color;      //颜色成份 R/G/B/A/D等
    VulkanBaseType      type;       //UNORM/SNORM等
    uint8               bits;       //位数
};//

struct PixelFormat
{
    ColorFormat         format;
    char                name[10];

    uint8               channels;   //颜色通道数
    char                color[4];
    uint8               bits[4];
    uint                total_bits; //每象素总位数
    VulkanBaseType      type;
};//struct PixelFormat

const PixelFormat *GetPixelFormat(ColorFormat);                                                     ///<根据获取获取象素格式类型
const PixelFormat *GetPixelFormat(const os_char *name);                                             ///<根据名称获取象素格式类型

void PrintFormatList();
