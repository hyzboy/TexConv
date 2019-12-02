#include"TextureFileCreater.h"
#include"ILImage.h"
#include<hgl/log/LogInfo.h>

class TextureFileCreaterRGBA:public TextureFileCreater
{
public:

    using TextureFileCreater::TextureFileCreater;

    void RGBA8toRGBA4(uint16 *target,uint8 *src,uint size)
    {
        for(uint i=0;i<size;i++)
        {
            *target=((src[3]<<8)&0xF000)
                   |((src[2]<<4)&0xF00)
                   |((src[1]   )&0xF0)
                   | (src[0]>>4);

            ++target;
            src+=4;
        }
    }
    
    void RGBA8toA1RGB5(uint16 *target,uint8 *src,uint size)
    {
        for(uint i=0;i<size;i++)
        {
            *target=((src[3]<<8)&0x8000)
                   |((src[2]<<7)&0x7C00)
                   |((src[1]<<2)&0x3E0)
                   | (src[0]>>3);

            ++target;
            src+=4;
        }
    }

    void RGBA16toA2BGR10(uint32 *target,uint16 *src,uint size)
    {
        for(uint i=0;i<size;i++)
        {
            *target=((src[3]<<16)&0xC0000000)
                   |((src[0]<<14)&0x3FF00000)
                   |((src[1]<< 4)&0xFFC00)
                   | (src[2]>> 6);

            ++target;
            src+=4;
        }
    }

public:
 
    bool Write() override
    {
        if(fmt->format==ColorFormat::RGBA8UN
         ||fmt->format==ColorFormat::RGBA16UN
         ||fmt->format==ColorFormat::RGBA16F
         ||fmt->format==ColorFormat::RGBA32U
         ||fmt->format==ColorFormat::RGBA32I
         ||fmt->format==ColorFormat::RGBA32F)
        {
            ILuint type;

            if(!ToILType(type,fmt->bits[0],fmt->type))
                return(nullptr);

            void *origin_rgba=image->GetRGBA(type);

            return TextureFileCreater::Write(origin_rgba);   
        }
        else if(fmt->format==ColorFormat::BGRA4)
        {
            void *origin_rgba=image->GetRGBA(IL_UNSIGNED_BYTE);

            AutoDelete<uint16> bgra4=new uint16[image->pixel_total()];

            RGBA8toRGBA4(bgra4,(uint8 *)origin_rgba,image->pixel_total());

            return TextureFileCreater::Write(bgra4);
        }
        else if(fmt->format==ColorFormat::A1RGB5)
        {
            void *origin_rgba=image->GetRGBA(IL_UNSIGNED_BYTE);

            AutoDelete<uint16> a1_rgb5=new uint16[image->pixel_total()];

            RGBA8toA1RGB5(a1_rgb5,(uint8 *)origin_rgba,image->pixel_total());

            return TextureFileCreater::Write(a1_rgb5);
        }
        else if(fmt->format==ColorFormat::A2BGR10UN)
        {
            void *origin_rgba=image->GetRGBA(IL_UNSIGNED_SHORT);

            AutoDelete<uint32> a2_bgr10=new uint32[image->pixel_total()];

            RGBA16toA2BGR10(a2_bgr10,(uint16 *)origin_rgba,image->pixel_total());

            return TextureFileCreater::Write(a2_bgr10);
        }
        else
        {
            LOG_ERROR(OS_TEXT("Don't support this RGBA format"));
            return(false);
        }
    }
};//class TextureFileCreaterRGB:public TextureFileCreater

TextureFileCreater *CreateTextureFileCreaterRGBA(const PixelFormat *pf,ILImage *image)
{
    return(new TextureFileCreaterRGBA(pf,image));
}
