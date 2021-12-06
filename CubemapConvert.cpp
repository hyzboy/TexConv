#include<iostream>
#include<hgl/util/cmd/CmdParse.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/type/StringList.h>
#include"ILImage.h"
#include"TextureFileCreater.h"
#include"ImageConvertConfig.h"
#include"CMP_CompressonatorLib/Compressonator.h"
#include"ParamParse.h"

using namespace hgl;
using namespace hgl::filesystem;
using namespace hgl::util;

bool ConvertCubemap(const OSString &filename,const OSStringList &file_list,const ImageConvertConfig *cfg)
{
    ILImage image[6];

    uint width,height,channels,bits;

    for(int i=0;i<6;i++)
    {
        if(!image[i].LoadFile(file_list[i]))
            return(false);

        if(i==0)
        {
            width=image[0].width();
            height=image[0].height();
            channels=image[0].channels();
            bits=image[0].bit();
        }
        else
        {
            if(width!=image[i].width()
             ||height!=image[i].height()
             ||channels!=image[i].channels()
             ||bits!=image[i].bit())
            {
                LOG_ERROR(OS_TEXT("image formats can't batch"));
                return(false);
            }
        }
    }
 
    int miplevel=1;

    if(cfg->gen_mipmaps)
        miplevel=hgl::GetMipLevel(width,height);
    
    if(channels<=0||channels>4)
    {
        LOG_ERROR(OS_TEXT("image format don't support "));
        return(false);
    }

    const PixelFormat *fmt=cfg->pixel_fmt[channels-1];

    uint total=0;
    uint bytes=0;

    for(int face=0;face<6;face++)
    {
        TextureFileCreater *tex_file_creater;

        if(fmt->format<ColorFormat::COMPRESS)
            tex_file_creater=CreateTFC[channels-1](fmt,&image[face]);
        else
            tex_file_creater=CreateTextureFileCreaterCompress(fmt,&image[face]);

        if(!tex_file_creater->WriteFileHeader(filename,TextureFileType::TexCubemap,miplevel))
        {
            tex_file_creater->Delete();
            LOG_ERROR(OS_TEXT("Write file header failed."));
            return(false);
        }

        tex_file_creater->InitFormat();

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

                image[face].Resize(width,height);
            }
        }

        tex_file_creater->Close();

        delete tex_file_creater;
    }

    LOG_INFO(OS_TEXT("pixel total length: ")+OSString::valueOf(total)+OS_TEXT(" bytes."));
    return(true);
}

void CMP_RegisterHostPlugins();

bool ConvertImage(const OSString &filename,const ImageConvertConfig *cfg);

int os_main(int argc,os_char **argv)
{
    std::cout<<"Cubemap to Texture Convert tools 1.2"<<std::endl<<std::endl;

    if(argc<=7)
    {
        std::cout<< "Command format:\n"
                    "\tCubemapConv [/R:][/RG:][/RGB:][/RGBA:] [/mip] <output texture filename> <neg x>,<neg y>,<neg z>,<pos x>,<pos y>,<pos z>\n\n";

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

    OSString out_filename=argv[argc-8];
    OSStringList file_list;

    for(int i=argc-7;i<argc-1;i++)
    {
        if(filesystem::FileCanRead(argv[i]))
            file_list.Add(argv[i]);
    }

    if(file_list.GetCount()==6)
    {
        os_out<<OS_TEXT("output: ")<<out_filename.c_str()<<std::endl;
        for(int i=0;i<6;i++)
            os_out<<OS_TEXT("source ")<<OSString::valueOf(i).c_str()<<OS_TEXT(": ")<<file_list[i].c_str()<<std::endl;

        ConvertCubemap(out_filename,file_list,&icc);
    }
            
    CMP_ShutdownBCLibrary();
	ilShutDown();
    return 0;
}
