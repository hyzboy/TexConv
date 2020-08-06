#include<hgl/log/LogInfo.h>
#include"ILImage.h"
#include"TextureFileCreater.h"

TextureFileCreater *CreateTextureFileCreaterR(const PixelFormat *,ILImage *);
TextureFileCreater *CreateTextureFileCreaterRG(const PixelFormat *,ILImage *);
TextureFileCreater *CreateTextureFileCreaterRGB(const PixelFormat *,ILImage *);
TextureFileCreater *CreateTextureFileCreaterRGBA(const PixelFormat *,ILImage *);

TextureFileCreater *CreateTextureFileCreaterCompress(const PixelFormat *,ILImage *);

using CTFC_FUNC=TextureFileCreater *(*)(const PixelFormat *,ILImage *);

static CTFC_FUNC CreateTFC[4]={CreateTextureFileCreaterR,CreateTextureFileCreaterRG,CreateTextureFileCreaterRGB,CreateTextureFileCreaterRGBA};

bool ConvertImage(const OSString &filename,const PixelFormat **pf)
{
    ILImage image;

    if(!image.LoadFile(filename))
        return(false);

    image.Bind();

    const uint channels=image.channels();

    if(channels<0||channels>4)
    {
        LOG_ERROR(OS_TEXT("image format don't support "));
        return(false);
    }

    TextureFileCreater *tex_file_creater;
    const PixelFormat *fmt=pf[channels-1];

    if(fmt->format<ColorFormat::COMPRESS)
        tex_file_creater=CreateTFC[channels-1](fmt,&image);
    else
        tex_file_creater=CreateTextureFileCreaterCompress(fmt,&image);

    if(!tex_file_creater->WriteFileHeader(filename))
    {
        tex_file_creater->Delete();
        LOG_ERROR(OS_TEXT("Write file header failed."));
        return(false);
    }

    if(!tex_file_creater->Write())
    {
        tex_file_creater->Delete();
        return(false);
    }

    tex_file_creater->Close();

    delete tex_file_creater;
    return(true);
}

