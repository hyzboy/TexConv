//注：起名为ILImageSupport是为了避免与IL中现有的ilimage冲突

#include"ILImage.h"
#include<IL/ilu.h>
#include<hgl/log/LogInfo.h>

using namespace hgl;

namespace
{
    const OSString GetILFormatName(const ILuint format)
    {
        #define IL_FMT2NAME(name)    if(format==IL_##name)return OS_TEXT(#name);

        IL_FMT2NAME(COLOR_INDEX)
        IL_FMT2NAME(ALPHA)
        IL_FMT2NAME(RGB)
        IL_FMT2NAME(RGBA)
        IL_FMT2NAME(BGR)
        IL_FMT2NAME(BGRA)
        IL_FMT2NAME(LUMINANCE)
        IL_FMT2NAME(LUMINANCE_ALPHA)

        #undef IL_FMT2NAME

        return OS_TEXT("Error format");
    }
    
    const OSString GetILTypeName(const ILuint type)
    {
        #define IL_TYPE2NAME(name)    if(type==IL_##name)return OS_TEXT(#name);

        IL_TYPE2NAME(BYTE)
        IL_TYPE2NAME(UNSIGNED_BYTE)
        IL_TYPE2NAME(SHORT)
        IL_TYPE2NAME(UNSIGNED_SHORT)
        IL_TYPE2NAME(INT)
        IL_TYPE2NAME(UNSIGNED_INT)
        IL_TYPE2NAME(FLOAT)
        IL_TYPE2NAME(DOUBLE)
        IL_TYPE2NAME(HALF)

        #undef IL_TYPE2NAME

        return OS_TEXT("Error type");
    }
}//namespace

ILImage::ILImage()
{
    ilGenImages(1,&il_index);
}

ILImage::~ILImage()
{
    ilDeleteImages(1,&il_index);
}

void ILImage::Bind()
{
    ilBindImage(il_index);
}

bool ILImage::Convert(ILuint format,ILuint type)
{
    if(il_format==format
     &&il_type==type)return(true);

    Bind();
    if(!ilConvertImage(format,type))
        return(false);
    
    il_format=format;
    il_type=type;
    return(true);
}

bool ILImage::LoadFile(const OSString &filename)
{
    Bind();

    if(!ilLoadImage(filename.c_str()))
        return(false);

    LOG_INFO(OS_TEXT("File: ")+filename);

    il_width	=ilGetInteger(IL_IMAGE_WIDTH);
    il_height	=ilGetInteger(IL_IMAGE_HEIGHT);
    il_bit		=ilGetInteger(IL_IMAGE_BITS_PER_PIXEL);
    il_format	=ilGetInteger(IL_IMAGE_FORMAT);
    il_type	    =ilGetInteger(IL_IMAGE_TYPE);

    if(ilGetInteger(IL_IMAGE_ORIGIN)==IL_ORIGIN_LOWER_LEFT)
        iluFlipImage();

    LOG_INFO(OS_TEXT("\t width: ")+OSString(il_width));
    LOG_INFO(OS_TEXT("\theight: ")+OSString(il_height));
    LOG_INFO(OS_TEXT("\t   bit: ")+OSString(il_bit));
    LOG_INFO(OS_TEXT("\tformat: ")+GetILFormatName(il_format));
    LOG_INFO(OS_TEXT("\t  type: ")+GetILTypeName(il_type));

    if(il_format==IL_COLOR_INDEX)
    {
        uint il_pattle=ilGetInteger(IL_PALETTE_TYPE);

        if(il_pattle==IL_PAL_RGB24||il_pattle==IL_PAL_BGR24)
        {
            il_bit      =24;
            il_format   =IL_BGR;
            il_type     =IL_UNSIGNED_BYTE;
        }
        else
        if(il_pattle==IL_PAL_RGB32||il_pattle==IL_PAL_BGR32)
        {
            il_bit      =48;
            il_format   =IL_BGR;
            il_type     =IL_UNSIGNED_SHORT;
        }
        else
        if(il_pattle==IL_PAL_RGBA32||il_pattle==IL_PAL_BGRA32)
        {
            il_bit      =32;
            il_format   =IL_BGRA;
            il_type     =IL_UNSIGNED_BYTE;
        }
        else
        {
            LOG_ERROR("Don't support the pattle format.");
            return(false);
        }

        Convert(il_format,il_type);
    }
    
    if(il_format==IL_LUMINANCE||il_format==IL_ALPHA)channel_count=1;else
    if(il_format==IL_LUMINANCE_ALPHA)               channel_count=2;else
    if(il_format==IL_RGB||il_format==IL_BGR)        channel_count=3;else
    if(il_format==IL_RGBA||il_format==IL_BGRA)      channel_count=4;else
        channel_count=0;

    return(true);    
}

void *ILImage::GetR(ILuint type)
{
    Bind();

    if(il_format==IL_ALPHA)return ilGetAlpha(type);
    if(il_format==IL_LUMINANCE)
    {
        if(il_type!=type)
            if(!Convert(il_format,type))
                return(nullptr);

        return ilGetData();
    }

    return(nullptr);
}

void *ILImage::GetData(ILuint format,ILuint type)
{
    Bind();

    if(il_format!=format||il_type!=type)
    if(!Convert(format,type))
        return nullptr;

    return ilGetData();
}
