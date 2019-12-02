#include"ConvertImage.h"
#include"ILImage.h"
#include"TextureFileCreater.h"

TextureFileCreater *CreateTextureFileCreaterR(const PixelFormat *,ILImage *);
TextureFileCreater *CreateTextureFileCreaterRG(const PixelFormat *,ILImage *);
TextureFileCreater *CreateTextureFileCreaterRGB(const PixelFormat *,ILImage *);
TextureFileCreater *CreateTextureFileCreaterRGBA(const PixelFormat *,ILImage *);

ConvertImage::ConvertImage()
{
    image=nullptr;
}

ConvertImage::~ConvertImage()
{
    SAFE_CLEAR(image);
}

bool ConvertImage::Load(const OSString &fn)
{
    LOG_INFO(OS_TEXT("File: ")+fn);

    image=new ILImage();

    if(!image->LoadFile(fn))
    {
        delete image;
        return(false);
    }

    filename=fn;

    return(true);
}

bool ConvertImage::Convert(const PixelFormat **pf)
{
    image->Bind();

    const uint channels=image->channels();

    TextureFileCreater *tex_file_creater;

    if(channels==1)tex_file_creater=CreateTextureFileCreaterR(pf[0],image);else
    if(channels==2)tex_file_creater=CreateTextureFileCreaterRG(pf[1],image);else
    if(channels==3)tex_file_creater=CreateTextureFileCreaterRGB(pf[2],image);else
    if(channels==4)tex_file_creater=CreateTextureFileCreaterRGBA(pf[3],image);else
    {
        LOG_ERROR(OS_TEXT("image format don't support "));
        return(false);
    }

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
