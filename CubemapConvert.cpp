#include<iostream>
#include<hgl/util/cmd/CmdParse.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/type/StringList.h>
#include<hgl/log/LogInfo.h>
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

    AutoDelete<TextureFileCreater> tex_file_creater=CreateTFC(fmt,channels);

    if (!tex_file_creater->CreateTexFile(filename, VK_IMAGE_VIEW_TYPE_CUBE))
    {
        LOG_ERROR(OS_TEXT("Create Texture failed."));
        return(false);
    }

    if (!tex_file_creater->WriteSize2D(width, height))
    {
        LOG_ERROR(OS_TEXT("Write size failed."));
        return(false);
    }

    if (!tex_file_creater->WritePixelFormat(miplevel))
    {
        LOG_ERROR(OS_TEXT("Write format failed."));
        return(false);
    }

    for(uint face=0;face<6;face++)
    {
        image[face].Bind();
        width=image[face].width();
        height=image[face].height();

        if (!tex_file_creater->InitFormat(&image[face]))
        {
            LOG_ERROR(OS_TEXT("Init texture format failed."));
            return(false);
        }

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

                image[face].Resize(width,height);
            }
        }
    }
    
    tex_file_creater->Close();
        
    LOG_INFO(OS_TEXT("pixel total length: ")+OSString::valueOf(total)+OS_TEXT(" bytes."));
    return(true);
}

void CMP_RegisterHostPlugins();

bool ConvertImage(const OSString &filename,const ImageConvertConfig *cfg);

int os_main(int argc,os_char **argv)
{
    std::cout<<"Cubemap to Texture Convert tools 1.3"<<std::endl<<std::endl;

    if(argc<=7)
    {
        std::cout<< "Command format:\n"
                    "\tCubemapConv [/R:][/RG:][/RGB:][/RGBA:] [/mip] <output texture filename> <pos x>,<neg x>,<pos y>,<neg y>,<pos z>,<neg z>\n\n";

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

    OSString out_filename=argv[argc-7];
    OSStringList file_list;

    for(int i=argc-6;i<argc;i++)
    {
        if(filesystem::FileCanRead(argv[i]))
            file_list.Add(argv[i]);
        else
        {
            LOG_ERROR(OS_TEXT("Can't check file ")+OSString(argv[i]));
            return(false);
        }
    }

    if(file_list.GetCount()==6)
    {
        constexpr os_char face_name[6][3]=
        {
            OS_TEXT("+X"),
            OS_TEXT("-X"),
            OS_TEXT("+Y"),
            OS_TEXT("-Y"),
            OS_TEXT("+Z"),
            OS_TEXT("-Z")
        };

        for(int i=0;i<6;i++)
            os_out<<OS_TEXT("    ")<<face_name[i]<<OS_TEXT(": ")<<file_list[i].c_str()<<std::endl;

        os_out<<OS_TEXT("output: ")<<out_filename.c_str()<<std::endl;

        ConvertCubemap(out_filename,file_list,&icc);
    }
            
    CMP_ShutdownBCLibrary();
	ilShutDown();
    return 0;
}
