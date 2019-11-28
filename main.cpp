﻿#include<il/il.h>
#include<il/ilu.h>
#include<iostream>
#include"cmd_parse.h"
#include"pixel_format.h"
#include<hgl/type/DataType.h>
#include<hgl/type/StrChar.h>
#include<hgl/Time.h>
#include<hgl/filesystem/EnumFile.h>
#include<hgl/log/LogInfo.h>

using namespace hgl;
using namespace hgl::filesystem;

bool                    sub_folder      =false;

const	PixelFormat *   pixel_fmt[4]	={nullptr,nullptr,nullptr,nullptr};     //选中格式
bool					gen_mipmaps	    =false;								    //是否产生mipmaps

bool					use_color_key   =false;							        //是否使用ColorKey
uint8					color_key[3];									        //ColorKey颜色

const PixelFormat *ParseParamFormat(const cmd_parse &cmd,const char *flag,const PixelFormat *default_format)
{
    std::string fmtstr;

    if(!cmd.GetString(flag,fmtstr))return(nullptr);

    const PixelFormat *result=GetPixelFormat(fmtstr.c_str());

    if(result)return(result);

    std::cerr<<"[FORMAT ERROR] Don't support \""<<fmtstr.c_str()<<"\" format."<<std::endl;

    return default_format;
}

void ParseParamFormat(const cmd_parse &cmd)
{
    //指定格式
    pixel_fmt[0]=ParseParamFormat(cmd,"/R:",      GetPixelFormat(ColorFormat::R8UN));
    pixel_fmt[1]=ParseParamFormat(cmd,"/RG:",     GetPixelFormat(ColorFormat::RG8UN));
    pixel_fmt[2]=ParseParamFormat(cmd,"/RGB:",    GetPixelFormat(ColorFormat::RGB565));
    pixel_fmt[3]=ParseParamFormat(cmd,"/RGBA:",   GetPixelFormat(ColorFormat::RGBA8UN));

    for(uint i=0;i<4;i++)
        std::cout<<i<<": "<<pixel_fmt[i]->name<<std::endl;
}

void ParamColorKey(const cmd_parse &cmd)
{
    std::string ckstr;

    if(!cmd.GetString("/ColorKey:",ckstr))return;

    char rgbstr[6];

    hgl_cpy(rgbstr,ckstr.c_str(),6);		//注意：hgl_cpy是跨类型复制的，不要替换成strcpy或memcpy

    ParseHexStr(color_key[0],rgbstr+0);
    ParseHexStr(color_key[1],rgbstr+2);
    ParseHexStr(color_key[2],rgbstr+4);

    use_color_key=true;
}

class EnumConvertImage:public EnumFile
{
private:

    uint convert_count=0;

protected:

    void ProcFile(EnumFileConfig *efc,FileInfo &fi) override
    {
        //ConvertImage(fi.fullname);
    }

public:

    const uint GetConvertCount()const{return convert_count;}
};//class EnumConvertImage:public EnumFile

#if HGL_OS == HGL_OS_Windows
int _wmain(int argc,wchar_t **argv)
#else
int main(int argc,char **argv)
#endif//
{
    std::cout<<"Image to Texture Convert tools 1.1"<<std::endl<<std::endl;

    if(argc<=1)
    {
        std::cout<< "Command format:\n"
                    "\tTexConv [/R:][/RG:][/RGB:][/RGBA:] [/s] [/mip] <filename or pathname>\n"
                    "\n"
                    "Params:\n"
                    "\t/s : proc sub-directory\n"
                    "\n";

        PrintFormatList();
        return 0;
    }

    cmd_parse cp(argc,argv);

    if(cp.Find("/s")!=-1)sub_folder=true;					//检测是否处理子目录
    if(cp.Find("/mip")!=-1)gen_mipmaps=true;				//检测是否生成mipmaps
    
    ParamColorKey(cp);
    ParseParamFormat(cp);								    //检测推荐格式
   
    ilInit();

    double start_time=GetMicroTime();
    double end_time;

    EnumFileConfig efc(cur_path);

    efc.find_name   =OSString(argv[1]);
    efc.proc_file   =true;
    efc.sub_folder  =sub_folder;

    EnumConvertImage eci;
    
    eci.Enum(&efc);

    end_time=GetTime();

    LOG_INFO(OS_TEXT("总计转换图片")+OSString(eci.GetConvertCount())
        +OS_TEXT("张，总计耗时")+OSString(end_time-start_time)+OS_TEXT("秒"));

	ilShutDown();
    return 0;
}
