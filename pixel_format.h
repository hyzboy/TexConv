#pragma once

#include<hgl/type/DataType.h>

using namespace hgl;

enum class ColorFormat
{
    NONE=0,

    BGRA4,
    RGB565,
    A1RGB5,
    R8UN,
    RG8UN,
    RGBA8UN,
    ABGR8UN,
    A2BGR10UN,
    R16UN,
    R16F,
    RG16UN,
    RG16F,
    RGBA16UN,
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

    END
};//enum class ColorFormat

enum class ColorDataType
{
    NONE=0,

    UNORM,
    SNORM,
    USCALED,
    SSCALED,
    UINT,
    SINT,
    UFLOAT,
    SFLOAT,

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
    ColorFormat color_format;
    char name[32];

    char            color[4];
    uint8           bits[4];
    ColorDataType   type;

public:

    const uint GetPixelBytes()const{return bits[0]+bits[1]+bits[2]+bits[3];}                        ///<获取单个象素所需字节数
};//

const PixelFormat *GetPixelFormat(const char *name);                                                ///<根据名称获取象素格式类型