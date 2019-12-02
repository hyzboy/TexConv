#include"TextureFileCreater.h"
#include"ILImage.h"

class TextureFileCreaterR:public TextureFileCreater
{
public:

    using TextureFileCreater::TextureFileCreater;

public:

    bool Write() override
    {
        ILuint type;

        if(!ToILType(type,fmt->bits[0],fmt->type))
            return(nullptr);

        void *data=image->GetR(type);

        if(!data)
            return(nullptr);

        //目前仅有R8UN,R16UN,R16F,R32U,R32I,R32F几种，都是8的整倍数，所以直接写
        return TextureFileCreater::Write(data);
    }
};//class TextureFileCreaterR:public TextureFileCreater

TextureFileCreater *CreateTextureFileCreaterR(const PixelFormat *pf,ILImage *image)
{
    return(new TextureFileCreaterR(pf,image));
}