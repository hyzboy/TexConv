#include<iostream>
#include<IL/il.h>
#include<IL/ilu.h>
#include<hgl/log/Logger.h>
#include<hgl/util/cmd/CmdParse.h>
#include<hgl/time/Time.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/filesystem/EnumFile.h>
#include"ImageConvertConfig.h"
#include"ParamParse.h"

using namespace hgl;
using namespace hgl::filesystem;
using namespace hgl::util;

bool sub_folder      =false;

void CMP_RegisterHostPlugins();

bool ConvertImage(const OSString &filename,const OSString &new_filename,const ImageConvertConfig *cfg);

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

class EnumConvertImage:public EnumFile
{
private:

    ImageConvertConfig *cfg;
    uint convert_count=0;

protected:

    void ProcFile(EnumFileConfig *efc,FileInfo &fi) override
    {
        const os_char *ext=hgl::strrchr(fi.name,'.');

        if(ext)
        {
            if(hgl::stricmp(ext,OS_TEXT(".Tex2D"))==0)  //已经是纹理文件了
                return;
        }

        OSString new_filename=ReplaceExtension<os_char>(fi.fullname,TEXTURE_FILE_EXT_NAME[size_t(VK_IMAGE_VIEW_TYPE_2D)]);

        if(ConvertImage(fi.fullname,new_filename,cfg))
            ++convert_count;
    }

public:

    const uint GetConvertCount()const{return convert_count;}

    EnumConvertImage(ImageConvertConfig *icc)
    {
        cfg=icc;
    }
};//class EnumConvertImage:public EnumFile

int os_main(int argc,os_char **argv)
{
    std::cout<<"Image to Texture Convert tools 1.4"<<std::endl<<std::endl;

    if(argc<=1)
    {
        std::cout<< "Command format:\n"
                    "\tTexConv [/R:][/RG:][/RGB:][/RGBA:] [/s] [/mip] <pathname or filename>\n"
                    "\n"
                    "Params:\n"
                    "\t/s : proc sub-directory\n"
                    "\n";

        PrintFormatList();
        return 0;
    }

    logger::InitLogger(OS_TEXT("TexConv"));

    CmdParse cp(argc,argv);

    ImageConvertConfig icc;

    if(cp.Find(OS_TEXT("/s"))!=-1)sub_folder=true;					    //检测是否处理子目录
    if(cp.Find(OS_TEXT("/mip"))!=-1)icc.gen_mipmaps=true;				//检测是否生成mipmaps
    
    ParseParamColorKey(&icc,cp);
    ParseParamFormat(&icc,cp);								            //检测推荐格式

    ilInit();
    iluImageParameter(ILU_FILTER,ILU_SCALE_MITCHELL);
    
    if(filesystem::FileCanRead(argv[argc-1]))
    {
        OSString new_filename=ReplaceExtension<os_char>(argv[argc-1],TEXTURE_FILE_EXT_NAME[size_t(VK_IMAGE_VIEW_TYPE_2D)]);

        ConvertImage(argv[argc-1],new_filename,&icc);
    }
    else
    {
        double start_time=GetMicroTime();
        double end_time;

        EnumFileConfig efc(argv[argc-1]);

        efc.proc_file   =true;
        efc.sub_folder  =sub_folder;

        EnumConvertImage eci(&icc);

        eci.Enum(&efc);

        end_time=GetMicroTime();

        const double time_gap=(end_time-start_time)/1000000;

        const OSString time_gap_str=OSString::floatOf(time_gap,2);

        GLogInfo(OS_TEXT("Total converted ")+OSString::numberOf(eci.GetConvertCount())
                +OS_TEXT(" textures for ")+time_gap_str.c_str()+OS_TEXT(" seconds."));
    }

	ilShutDown();
    return 0;
}
