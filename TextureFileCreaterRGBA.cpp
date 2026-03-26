#include"TextureFileCreater.h"
#include"ImageLoader.h"
#include<hgl/log/log.h>
#include<hgl/color/ColorFormat.h>

class TextureFileCreaterRGBA:public TextureFileCreater
{
    ImagePixelType pixel_type;

public:

    using TextureFileCreater::TextureFileCreater;

    bool InitFormat(MagickImage* img) override
    {
        image = img;

        if(pixel_format->format==ColorFormat::RGBA8
         ||pixel_format->format==ColorFormat::RGBA16
         ||pixel_format->format==ColorFormat::RGBA16U
         ||pixel_format->format==ColorFormat::RGBA16I
         ||pixel_format->format==ColorFormat::RGBA16F
         ||pixel_format->format==ColorFormat::RGBA32U
         ||pixel_format->format==ColorFormat::RGBA32I
         ||pixel_format->format==ColorFormat::RGBA32F)
        {
            if(!ToImagePixelType(pixel_type,pixel_format->bits[0],pixel_format->type))
                return(false);
        }
        else if(pixel_format->format==ColorFormat::RGBA4
              ||pixel_format->format==ColorFormat::BGRA4)
        {
            pixel_type=ImagePixelType::UInt8;
        }
        else if(pixel_format->format==ColorFormat::A1RGB5)
        {
            pixel_type=ImagePixelType::UInt8;
        }
        else if(pixel_format->format==ColorFormat::A2BGR10)
        {
            pixel_type=ImagePixelType::UInt16;
        }
        else
        {
            LogError(OS_TEXT("Don't support this RGBA format"));
            return(false);
        }

        return image->ConvertToRGBA(pixel_type);
    }

public:

    uint32 Write() override
    {
        const uint total_bytes=(pixel_format->total_bits*image->pixel_total())>>3;

        LogInfo("Convert Image To: "+AnsiString::numberOf(image->width())+"x"+AnsiString::numberOf(image->height())+" "+AnsiString::numberOf(total_bytes)+" bytes.");

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
            void *origin_rgba=image->GetRGBA(pixel_type);

            return TextureFileCreater::Write(origin_rgba,total_bytes);
        }
        else if(pixel_format->format==ColorFormat::ABGR8)
        {
            uint32 *origin_rgba=(uint32 *)(image->GetRGBA(pixel_type));

            EndianSwap<uint32>(origin_rgba,image->pixel_total());

            return TextureFileCreater::Write(origin_rgba,total_bytes);
        }
        else if(pixel_format->format==ColorFormat::BGRA4)
        {
            void *origin_rgba=image->GetRGBA(ImagePixelType::UInt8);

            AutoDeleteArray<uint16> bgra4(image->pixel_total());

            RGBA8toBGRA4(bgra4,(uint8 *)origin_rgba,image->pixel_total());

            return TextureFileCreater::Write(bgra4,total_bytes);
        }
        else if(pixel_format->format==ColorFormat::RGBA4)
        {
            void *origin_rgba=image->GetRGBA(ImagePixelType::UInt8);

            AutoDeleteArray<uint16> rgba4(image->pixel_total());

            RGBA8toRGBA4(rgba4,(uint8 *)origin_rgba,image->pixel_total());

            return TextureFileCreater::Write(rgba4,total_bytes);
        }
        else if(pixel_format->format==ColorFormat::A1RGB5)
        {
            void *origin_rgba=image->GetRGBA(ImagePixelType::UInt8);

            AutoDeleteArray<uint16> a1_rgb5(image->pixel_total());

            RGBA8toA1RGB5(a1_rgb5,(uint8 *)origin_rgba,image->pixel_total());

            return TextureFileCreater::Write(a1_rgb5,total_bytes);
        }
        else if(pixel_format->format==ColorFormat::A2BGR10)
        {
            void *origin_rgba=image->GetRGBA(ImagePixelType::UInt16);

            AutoDeleteArray<uint32> a2_bgr10(image->pixel_total());

            RGBA16toA2BGR10(a2_bgr10,(uint16 *)origin_rgba,image->pixel_total());

            return TextureFileCreater::Write(a2_bgr10,total_bytes);
        }
        else
        {
            LogError(OS_TEXT("Don't support this RGBA format"));
            return(0);
        }
    }
};//class TextureFileCreaterRGB:public TextureFileCreater

TextureFileCreater *CreateTextureFileCreaterRGBA(const PixelFormat *pf)
{
    return(new TextureFileCreaterRGBA(pf));
}
