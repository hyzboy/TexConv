#pragma once
#include"pixel_format.h"

struct ImageConvertConfig
{
    const	PixelFormat *   pixel_fmt[4];   ///<选中格式
    bool					gen_mipmaps;    ///<是否产生mipmaps

    bool					use_color_key;  ///<是否使用ColorKey
    uint8					color_key[3];   ///<ColorKey颜色

public:

    ImageConvertConfig()
    {
        hgl_zero(*this);
    }
};//struct ImageConvertConfig
