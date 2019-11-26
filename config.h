#pragma once

using uint=unsigned int;

namespace
{
    enum class FormatType:uint
    {
        R=0,
        RG,
        RGB,
        RGBA,

        CubeR,
        CubeRG,
        CubeRGB,
        CubeRGBA,

        BEGIN_RANGE =R,
        END_RANGE   =CubeRGBA,
        RANGE_SIZE  =END_RANGE-BEGIN_RANGE+1
    };//

    struct FormatConfig
    {
        uint force_bit;             ///<色彩强制位数，为0表示自适应
        bool force_float;           ///<色彩强制浮点
        bool gen_mipmaps;           ///<生成mipmaps
        bool gen_ibl_cubemap;       ///<生成ibl pbr用ibl cubemap
    };
}//namespace
