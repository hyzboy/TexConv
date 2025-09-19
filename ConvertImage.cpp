#include<hgl/log/log.h>
#include<IL/ilu.h>
#include"ILImage.h"
#include"TextureFileCreater.h"
#include"ImageConvertConfig.h"

bool ConvertImage(const OSString &filename,const ImageConvertConfig *cfg)
{
    ILImage image;

    if(!image.LoadFile(filename))
        return(false);
 
    int miplevel=1;

    if(cfg->gen_mipmaps)
        miplevel=hgl::GetMipLevel(image.width(),image.height());
    
    const uint channels=image.channels();

    if(channels<=0||channels>4)
    {
        GLogError(OS_TEXT("image format don't support "));
        return(false);
    }

    const PixelFormat *fmt=cfg->pixel_fmt[channels-1];
    uint width = image.width();
    uint height = image.height();

    AutoDelete<TextureFileCreater> tex_file_creater=CreateTFC(fmt,channels);

    if(!tex_file_creater->CreateTexFile(filename,VK_IMAGE_VIEW_TYPE_2D))
    {
        GLogError(OS_TEXT("Create Texture failed."));
        return(false);
    }

    if(!tex_file_creater->WriteSize2D(width,height))
    {
        GLogError(OS_TEXT("Write size failed."));
        return(false);
    }

    if(!tex_file_creater->WritePixelFormat(miplevel))
    {
        GLogError(OS_TEXT("Write format failed."));
        return(false);
    }

    if(!tex_file_creater->InitFormat(&image))
    {
        GLogError(OS_TEXT("Init texture format failed."));
        return(false);
    }

    uint total=0;
    uint bytes=0;

    for(int i=0;i<miplevel;i++)
    {
        bytes=tex_file_creater->Write();

        if(bytes<=0)
        {
            tex_file_creater->Delete();
            return(false);
        }

        total+=bytes;

        if(miplevel>1&&i<miplevel)
        {
            if(width>1)width>>=1;
            if(height>1)height>>=1;

            image.Resize(width,height);
        }
    }

    GLogInfo(OS_TEXT("pixel total length: ")+OSString::numberOf(total)+OS_TEXT(" bytes."));

    tex_file_creater->Close();

    return(true);
}

