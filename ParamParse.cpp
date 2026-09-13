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
    icc->pixel_fmt[0]=ParseParamFormat(cmd,OS_TEXT("/R:"),      GetPixelFormat(ColorFormat::BC4));
    icc->pixel_fmt[1]=ParseParamFormat(cmd,OS_TEXT("/RG:"),     GetPixelFormat(ColorFormat::BC5));
    icc->pixel_fmt[2]=ParseParamFormat(cmd,OS_TEXT("/RGB:"),    GetPixelFormat(ColorFormat::BC7));
    icc->pixel_fmt[3]=ParseParamFormat(cmd,OS_TEXT("/RGBA:"),   GetPixelFormat(ColorFormat::BC7));

    // /normal：法线贴图按双通道存（BC5）——X/Y 进 R/G，Z 由 shader 用
    // z=sqrt(1-x²-y²) 还原（同 UE/Unity 的做法；见 ShaderLibrary/ntb/*.glsl）。
    // 好处不是体积（BC5 与 BC7 同为 16 字节/块），而是去掉 Z 通道的量化误差、
    // 且两通道各按 BC4 精度编码，法线更平滑。源图几通道都一样处理。
    if(cmd.Contains(OS_TEXT("/normal")))
    {
        const PixelFormat *bc5=GetPixelFormat(ColorFormat::BC5);

        for(uint i=0;i<4;i++)
            icc->pixel_fmt[i]=bc5;
    }

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
