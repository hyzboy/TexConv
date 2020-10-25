﻿#include"TextureFileCreater.h"
#include"ILImage.h"

class TextureFileCreaterR:public TextureFileCreater
{
    ILuint type;

public:

    using TextureFileCreater::TextureFileCreater;

public:

    bool InitFormat() override
    {
        if(!ToILType(type,pixel_format->bits[0],pixel_format->type))
            return(false);

        return image->ConvertToR(type);
    }

    uint32 Write() override
    {
        const uint total_bytes=(pixel_format->total_bits*image->pixel_total())>>3;

        std::cout<<"Convert Image To: "<<image->width()<<"x"<<image->height()<<" "<<total_bytes<<" bytes."<<std::endl;

        void *data=image->GetR(type);

        if(!data)
            return(0);

        //目前仅有R8UN,R16UN,R16F,R32U,R32I,R32F几种，都是8的整倍数，所以直接写
        return TextureFileCreater::Write(data,total_bytes);
    }
};//class TextureFileCreaterR:public TextureFileCreater

TextureFileCreater *CreateTextureFileCreaterR(const PixelFormat *pf,ILImage *image)
{
    return(new TextureFileCreaterR(pf,image));
}