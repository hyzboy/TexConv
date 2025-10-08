#include<hgl/log/log.h>
#include<IL/ilu.h>
#include"ILImage.h"
#include"TextureFileCreater.h"
#include"ImageConvertConfig.h"
#include<hgl/filesystem/FileSystem.h>

using namespace hgl::filesystem;

namespace
{
    constexpr os_char TEXTURE_FILE_EXT_NAME[][20]=            //顺序必须等同VkImageViewType
    {
        OS_TEXT("Tex1D"),
        OS_TEXT("Tex2D"),
        OS_TEXT("Tex3D"),
        OS_TEXT("TexCube"),
        OS_TEXT("Tex1DArray"),
        OS_TEXT("Tex2DArray"),
        OS_TEXT("TexCubeArray")
    };
}

bool ConvertImage(const OSString &filename,const ImageConvertConfig *cfg)
{
    ILImage image;

    if(!image.LoadFile(filename))
        return(false);
 
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
    
    OSString new_filename=ReplaceExtension<os_char>(filename,TEXTURE_FILE_EXT_NAME[size_t(VK_IMAGE_VIEW_TYPE_2D)]);

    if(!tex_file_creater->CreateTexFile(filename,new_filename,VK_IMAGE_VIEW_TYPE_2D))
    {
        GLogError(OS_TEXT("Create Texture failed."));
        return(false);
    }

    if(!tex_file_creater->WriteSize2D(width,height))
    {
        GLogError(OS_TEXT("Write size failed."));
        return(false);
    }

    int miplevel=1;
    const bool is_compress_fmt=(fmt->channels==0);

    if(cfg->gen_mipmaps)
    {
        miplevel=hgl::GetMipLevel(image.width(),image.height());

        if(is_compress_fmt)     //压缩格式最小只能4X4,所以没有2X2/1X1这种尺寸
        {
            if(width>4||height>4)
                miplevel-=2;        //对于大于4x4的图片，减少2级mipmaps
        }
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

    uint min_size=(is_compress_fmt?4:1);

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
            if(width>min_size)width>>=1;
            if(height>min_size)height>>=1;

            image.Resize(width,height);
        }
    }

    GLogInfo(OS_TEXT("pixel total length: ")+OSString::numberOf(total)+OS_TEXT(" bytes."));

    tex_file_creater->Close();

    return(true);
}

