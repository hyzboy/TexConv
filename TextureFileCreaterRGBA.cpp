#include"TextureFileCreater.h"
#include"ILImage.h"
#include<hgl/log/LogInfo.h>

class TextureFileCreaterRGBA:public TextureFileCreater
{
    ILuint type;

public:

    using TextureFileCreater::TextureFileCreater;
    
    bool InitFormat() override
    {
        if(pixel_format->format==ColorFormat::RGBA8
         ||pixel_format->format==ColorFormat::RGBA16
         ||pixel_format->format==ColorFormat::RGBA16U
         ||pixel_format->format==ColorFormat::RGBA16I
         ||pixel_format->format==ColorFormat::RGBA16F
         ||pixel_format->format==ColorFormat::RGBA32U
         ||pixel_format->format==ColorFormat::RGBA32I
         ||pixel_format->format==ColorFormat::RGBA32F)
        {
            if(!ToILType(type,pixel_format->bits[0],pixel_format->type))
                return(nullptr);
        }
        else if(pixel_format->format==ColorFormat::RGBA4
              ||pixel_format->format==ColorFormat::BGRA4)
        {
            type=IL_UNSIGNED_BYTE;
        }
        else if(pixel_format->format==ColorFormat::A1RGB5)
        {
            type=IL_UNSIGNED_BYTE;
        }
        else if(pixel_format->format==ColorFormat::A2BGR10)
        {
            type=IL_UNSIGNED_SHORT;
        }
        else
        {
            LOG_ERROR(OS_TEXT("Don't support this RGBA format"));
            return(false);
        }

        return image->ConvertToRGBA(type);
    }

    void RGBA8toBGRA4(uint16 *target,uint8 *src,uint size)
    {
        for(uint i=0;i<size;i++)
        {
            *target=((src[2]<<8)&0xF000)
                   |((src[1]<<4)&0xF00)
                   |((src[0]   )&0xF0)
                   | (src[3]>>4);

            ++target;
            src+=4;
        }
    }

    void RGBA8toRGBA4(uint16 *target,uint8 *src,uint size)
    {
        for(uint i=0;i<size;i++)
        {
            *target=((src[0]<<8)&0xF000)
                   |((src[1]<<4)&0xF00)
                   |((src[2]   )&0xF0)
                   | (src[3]>>4);

            ++target;
            src+=4;
        }
    }
   
    void RGBA8toA1RGB5(uint16 *target,uint8 *src,uint size)
    {
        for(uint i=0;i<size;i++)
        {
            *target=((src[3]<<8)&0x8000)
                   |((src[0]<<7)&0x7C00)
                   |((src[1]<<2)&0x3E0)
                   | (src[2]>>3);

            ++target;
            src+=4;
        }
    }

    void RGBA16toA2BGR10(uint32 *target,uint16 *src,uint size)
    {
        for(uint i=0;i<size;i++)
        {
            *target=((src[3]<<16)&0xC0000000)
                   |((src[2]<<14)&0x3FF00000)
                   |((src[1]<< 4)&0xFFC00)
                   | (src[0]>> 6);

            ++target;
            src+=4;
        }
    }

public:
 
    uint32 Write() override
    {
        const uint total_bytes=(pixel_format->total_bits*image->pixel_total())>>3;

        std::cout<<"Convert Image To: "<<image->width()<<"x"<<image->height()<<" "<<total_bytes<<" bytes."<<std::endl;

        if(pixel_format->format==ColorFormat::RGBA8
         ||pixel_format->format==ColorFormat::RGBA8SN
         ||pixel_format->format==ColorFormat::RGBA8U
         ||pixel_format->format==ColorFormat::RGBA8I
         ||pixel_format->format==ColorFormat::RGBA16
         ||pixel_format->format==ColorFormat::RGBA16U
         ||pixel_format->format==ColorFormat::RGBA16I
         ||pixel_format->format==ColorFormat::RGBA16F
         ||pixel_format->format==ColorFormat::RGBA32U
         ||pixel_format->format==ColorFormat::RGBA32I
         ||pixel_format->format==ColorFormat::RGBA32F)
        {
            void *origin_rgba=image->GetRGBA(type);

            return TextureFileCreater::Write(origin_rgba,total_bytes);
        }
        else if(pixel_format->format==ColorFormat::ABGR8)
        {
            uint32 *origin_rgba=(uint32 *)(image->GetRGBA(type));

            EndianSwap<uint32>(origin_rgba,image->pixel_total());

            return TextureFileCreater::Write(origin_rgba,total_bytes);            
        }
        else if(pixel_format->format==ColorFormat::BGRA4)
        {
            void *origin_rgba=image->GetRGBA(IL_UNSIGNED_BYTE);

            AutoDeleteArray<uint16> bgra4(image->pixel_total());

            RGBA8toBGRA4(bgra4,(uint8 *)origin_rgba,image->pixel_total());

            return TextureFileCreater::Write(bgra4,total_bytes);
        }
        else if(pixel_format->format==ColorFormat::RGBA4)
        {
            void *origin_rgba=image->GetRGBA(IL_UNSIGNED_BYTE);

            AutoDeleteArray<uint16> rgba4(image->pixel_total());

            RGBA8toRGBA4(rgba4,(uint8 *)origin_rgba,image->pixel_total());

            return TextureFileCreater::Write(rgba4,total_bytes);
        }
        else if(pixel_format->format==ColorFormat::A1RGB5)
        {
            void *origin_rgba=image->GetRGBA(IL_UNSIGNED_BYTE);

            AutoDeleteArray<uint16> a1_rgb5(image->pixel_total());

            RGBA8toA1RGB5(a1_rgb5,(uint8 *)origin_rgba,image->pixel_total());

            return TextureFileCreater::Write(a1_rgb5,total_bytes);
        }
        else if(pixel_format->format==ColorFormat::A2BGR10)
        {
            void *origin_rgba=image->GetRGBA(IL_UNSIGNED_SHORT);

            AutoDeleteArray<uint32> a2_bgr10(image->pixel_total());

            RGBA16toA2BGR10(a2_bgr10,(uint16 *)origin_rgba,image->pixel_total());

            return TextureFileCreater::Write(a2_bgr10,total_bytes);
        }
        else
        {
            LOG_ERROR(OS_TEXT("Don't support this RGBA format"));
            return(0);
        }
    }
};//class TextureFileCreaterRGB:public TextureFileCreater

TextureFileCreater *CreateTextureFileCreaterRGBA(const PixelFormat *pf,ILImage *image)
{
    return(new TextureFileCreaterRGBA(pf,image));
}
