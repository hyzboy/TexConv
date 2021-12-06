#include<iostream>
#include<hgl/util/cmd/CmdParse.h>
#include<hgl/filesystem/FileSystem.h>
#include"ILImage.h"
#include"TextureFileCreater.h"
#include"ImageConvertConfig.h"
#include"CMP_CompressonatorLib/Compressonator.h"
#include"ParamParse.h"

using namespace hgl;
using namespace hgl::filesystem;
using namespace hgl::util;

bool ConvertImage(const OSString &filename,const ImageConvertConfig *cfg)
{
    ILImage image;

    if(!image.LoadFile(filename))
        return(false);
 
    int miplevel=1;

    if(cfg->gen_mipmaps)
        miplevel=hgl::GetMipLevel(image.width(),image.height());
    
    const uint channels=image.channels();

    if(channels<0||channels>4)
    {
        LOG_ERROR(OS_TEXT("image format don't support "));
        return(false);
    }

    TextureFileCreater *tex_file_creater;
    const PixelFormat *fmt=cfg->pixel_fmt[channels-1];

    Image2D *origin_img=nullptr;

    if(fmt->format<ColorFormat::COMPRESS)
        tex_file_creater=CreateTFC[channels-1](fmt,&image);
    else
        tex_file_creater=CreateTextureFileCreaterCompress(fmt,&image);

    if(!tex_file_creater->WriteFileHeader(filename,TextureFileType::TexCubemap,miplevel))
    {
        tex_file_creater->Delete();
        LOG_ERROR(OS_TEXT("Write file header failed."));
        return(false);
    }

    tex_file_creater->InitFormat();

    uint width=image.width();
    uint height=image.height();
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

        if(i<miplevel)
        {
            if(width>1)width>>=1;
            if(height>1)height>>=1;

            image.Resize(width,height);
        }
    }

    LOG_INFO(OS_TEXT("pixel total length: ")+OSString::valueOf(total)+OS_TEXT(" bytes."));

    tex_file_creater->Close();

    delete tex_file_creater;
    return(true);
}

void CMP_RegisterHostPlugins();

bool ConvertImage(const OSString &filename,const ImageConvertConfig *cfg);

int os_main(int argc,os_char **argv)
{
    std::cout<<"Cubemap to Texture Convert tools 1.2"<<std::endl<<std::endl;

    if(argc<=1)
    {
        std::cout<< "Command format:\n"
                    "\tCubemapConv [/R:][/RG:][/RGB:][/RGBA:] [/mip] <filename>\n\n";

        PrintFormatList();
        return 0;
    }

    CmdParse cp(argc,argv);

    ImageConvertConfig icc;

    if(cp.Find(OS_TEXT("/mip"))!=-1)icc.gen_mipmaps=true;				//检测是否生成mipmaps
    
    ParseParamColorKey(&icc,cp);
    ParseParamFormat(&icc,cp);								            //检测推荐格式

    ilInit();
    
    CMP_RegisterHostPlugins();
    CMP_InitializeBCLibrary();

    if(filesystem::FileCanRead(argv[argc-1]))
    {
        ConvertImage(argv[argc-1],&icc);
    }
            
    CMP_ShutdownBCLibrary();
	ilShutDown();
    return 0;
}
