#pragma once
#include"pixel_format.h"

enum class CompressionProvider
{
    AMD_Compressonator,
    Intel_ISPC,
};

struct ImageConvertConfig
{
    CompressionProvider     provider;       ///<压缩提供器

    const    PixelFormat *  pixel_fmt[4];   ///<选中格式
    bool                    gen_mipmaps;    ///<是否产生mipmaps

    bool                    use_color_key;  ///<是否使用ColorKey
    uint8                   color_key[3];   ///<ColorKey颜色
    
    bool                    force_grayscale;///<是否强制转换为灰度图
    bool                    discard_alpha;  ///<是否丢弃alpha通道

public:

    ImageConvertConfig()
    {
        mem_zero(*this);
    }
};//struct ImageConvertConfig
