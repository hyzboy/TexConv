#include"ParamParse.h"
#include<hgl/log/log.h>
#include<iostream>

const PixelFormat *ParseParamFormat(const CmdParse &cmd,const os_char *flag,const PixelFormat *default_format)
{
    OSString fmtstr;

    if(!cmd.GetString(flag,fmtstr))return(default_format);

    const PixelFormat *result=GetPixelFormat(fmtstr.c_str());

    if(result)return(result);

    GLogInfo(OS_TEXT("[FORMAT ERROR] Don't support ")+fmtstr+OS_TEXT(" format."));

    return default_format;
}

void ParseParamFormat(ImageConvertConfig *icc,const CmdParse &cmd)
{
    //ָ����ʽ
    icc->pixel_fmt[0]=ParseParamFormat(cmd,OS_TEXT("/R:"),      GetPixelFormat(ColorFormat::R8));
    icc->pixel_fmt[1]=ParseParamFormat(cmd,OS_TEXT("/RG:"),     GetPixelFormat(ColorFormat::RG8));
    icc->pixel_fmt[2]=ParseParamFormat(cmd,OS_TEXT("/RGB:"),    GetPixelFormat(ColorFormat::RGB565));
    icc->pixel_fmt[3]=ParseParamFormat(cmd,OS_TEXT("/RGBA:"),   GetPixelFormat(ColorFormat::RGBA8));

    for(uint i=0;i<4;i++)
    {
        std::cout<<(i+1)<<": "<<icc->pixel_fmt[i]->name<<std::endl;
    }
}

void ParseParamColorKey(ImageConvertConfig *icc,const CmdParse &cmd)
{
    OSString ckstr;

    if(!cmd.GetString(OS_TEXT("/ColorKey:"),ckstr))return;

    const os_char *rgbstr=ckstr.c_str();

    ParseHexStr(icc->color_key[0],rgbstr+0);
    ParseHexStr(icc->color_key[1],rgbstr+2);
    ParseHexStr(icc->color_key[2],rgbstr+4);

    icc->use_color_key=true;
}