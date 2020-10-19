#pragma once

#include<hgl/type/DataType.h>

using namespace hgl;

enum class ColorFormat
{
    NONE=0,

    BGRA4,
    RGB565,
    A1RGB5,
    R8,
    RG8,
    RGBA8,
    RGBA8SN,
    RGBA8U,
    RGBA8I,
    A2BGR10,
    R16,
    R16F,
    RG16,
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

enum class ColorDataType
{
    NONE=0,

    UNORM,
    SNORM,
    USCALE,
    SSCALE,
    UINT,
    SINT,
    UFLOAT,
    SFLOAT,
    SRGB,

    END
};//enum class ColorDataType

struct ColorDataFormat
{
    char            color;      //颜色成份 R/G/B/A/D等
    ColorDataType   type;       //UNORM/SNORM等
    uint8           bits;       //位数
};//

struct PixelFormat
{
    ColorFormat     format;
    char            name[32];

    uint8           channels;   //颜色通道数
    char            color[4];
    uint8           bits[4];
    ColorDataType   type;

public:

    const uint GetPixelBytes()const{return (bits[0]+bits[1]+bits[2]+bits[3])>>3;}                   ///<获取单个象素所需字节数
};//struct PixelFormat

const PixelFormat *GetPixelFormat(ColorFormat);                                                     ///<根据获取获取象素格式类型
const PixelFormat *GetPixelFormat(const os_char *name);                                             ///<根据名称获取象素格式类型

void PrintFormatList();
