#include<iostream>
#include<IL/il.h>
#include<hgl/util/cmd/CmdParse.h>
#include<hgl/Time.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/filesystem/EnumFile.h>
#include"CMP_CompressonatorLib/Compressonator.h"
#include"ImageConvertConfig.h"
#include"ParamParse.h"

using namespace hgl;
using namespace hgl::filesystem;
using namespace hgl::util;

bool sub_folder      =false;

void CMP_RegisterHostPlugins();

bool ConvertImage(const OSString &filename,const ImageConvertConfig *cfg);

class EnumConvertImage:public EnumFile
{
private:

    ImageConvertConfig *cfg;
    uint convert_count=0;

protected:

    void ProcFile(EnumFileConfig *efc,FileInfo &fi) override
    {
        if(ConvertImage(fi.fullname,cfg))
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
    std::cout<<"Image to Texture Convert tools 1.3a"<<std::endl<<std::endl;

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

    CmdParse cp(argc,argv);

    ImageConvertConfig icc;

    if(cp.Find(OS_TEXT("/s"))!=-1)sub_folder=true;					//检测是否处理子目录
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

        const OSString time_gap_str=OSString::valueOf(time_gap);

        LOG_INFO(OS_TEXT("Total converted ")+OSString::valueOf(eci.GetConvertCount())
                +OS_TEXT(" textures for ")+time_gap_str.c_str()+OS_TEXT(" seconds."));
    }
            
    CMP_ShutdownBCLibrary();
	ilShutDown();
    return 0;
}
