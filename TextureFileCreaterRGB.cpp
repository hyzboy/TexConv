#include"TextureFileCreater.h"
#include"ILImage.h"
#include<hgl/log/LogInfo.h>

class TextureFileCreaterRGB:public TextureFileCreater
{
public:

    using TextureFileCreater::TextureFileCreater;

    void RGB8toRGB565(uint16 *target,uint8 *src,uint size)
    {
        for(uint i=0;i<size;i++)
        {
            *target=((src[2]<<8)&0xF800)
                   |((src[1]<<3)&0x7E0)
                   | (src[0]>>3);

            ++target;
            src+=3;
        }
    }

public:
 
    bool Write() override
    {
        if(fmt->format==ColorFormat::RGB32U
         ||fmt->format==ColorFormat::RGB32I
         ||fmt->format==ColorFormat::RGB32F)
        {
            ILuint type;

            if(!ToILType(type,fmt->bits[0],fmt->type))
                return(nullptr);

            void *origin_rgb=image->GetRGB(type);

            return TextureFileCreater::Write(origin_rgb);   
        }
        else if(fmt->format==ColorFormat::RGB565)
        {
            void *origin_rgb=image->GetRGB(IL_UNSIGNED_BYTE);

            AutoDelete<uint16> rgb565=new uint16[image->pixel_total()];

            RGB8toRGB565(rgb565,(uint8 *)origin_rgb,image->pixel_total());

            return TextureFileCreater::Write(rgb565);
        }
        else
        {
            LOG_ERROR(OS_TEXT("Don't support this RGB format"));
            return(false);
        }
    }
};//class TextureFileCreaterRGB:public TextureFileCreater

TextureFileCreater *CreateTextureFileCreaterRGB(const PixelFormat *pf,ILImage *image)
{
    return(new TextureFileCreaterRGB(pf,image));
}
