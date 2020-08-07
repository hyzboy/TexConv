//注：起名为ILImageSupport是为了避免与IL中现有的ilimage冲突

#include"ILImage.h"
#include<IL/ilu.h>
#include<hgl/log/LogInfo.h>
#include<hgl/filesystem/FileSystem.h>

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

constexpr ILenum format_by_channel[]=
{
	IL_LUMINANCE,
	IL_LUMINANCE_ALPHA,
	IL_RGB,
	IL_RGBA,
};

bool SaveImageToFile(const OSString &filename,ILuint w,ILuint h,const float scale,ILuint c,ILuint t,void *data)
{
    if(filename.IsEmpty())return(false);
    if(w<=0||h<=1)return(false);
    if(c<1||c>4)return(false);
    if(!data)return(false);

    ILuint il_index;

    ilGenImages(1,&il_index);
    ilBindImage(il_index);

    if(!ilTexImage(w,h,1,c,format_by_channel[c-1],t,data))
        return(false);

    iluScale(w*scale,h*scale,1);

    iluFlipImage();
    ilEnable(IL_FILE_OVERWRITE);

    const bool result=ilSaveImage(filename.c_str());

    ilDeleteImages(1,&il_index);

    return result;
}

void ILImage::Bind()
{
    ilBindImage(il_index);
}

bool ILImage::Resize(uint nw,uint nh)
{
    if(nw==il_width&&nh==il_height)return(true);
    if(nw==0||nh==0)return(false);

    Bind();

    if(!iluScale(nw,nh,il_depth))
        return(false);

    il_width=nw;
    il_height=nh;

    return(true);
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

    if(!filesystem::FileExist(filename))
    {
        LOG_INFO(OS_TEXT("Can't find filename: ")+filename);
        return(false);
    }

    if(!ilLoadImage(filename.c_str()))
        return(false);

    LOG_INFO(OS_TEXT("\nFile: ")+filename);

    il_width	=ilGetInteger(IL_IMAGE_WIDTH);
    il_height	=ilGetInteger(IL_IMAGE_HEIGHT);
    il_depth    =ilGetInteger(IL_IMAGE_DEPTH);
    il_bit		=ilGetInteger(IL_IMAGE_BITS_PER_PIXEL);
    il_format	=ilGetInteger(IL_IMAGE_FORMAT);
    il_type	    =ilGetInteger(IL_IMAGE_TYPE);

    if(ilGetInteger(IL_IMAGE_ORIGIN)==IL_ORIGIN_LOWER_LEFT)
        iluFlipImage();

    LOG_INFO(OS_TEXT("\t width: ")+OSString::valueOf(il_width));
    LOG_INFO(OS_TEXT("\theight: ")+OSString::valueOf(il_height));
    LOG_INFO(OS_TEXT("\t depth: ")+OSString::valueOf(il_depth));
    LOG_INFO(OS_TEXT("\t   bit: ")+OSString::valueOf(il_bit));
    LOG_INFO(OS_TEXT("\tformat: ")+GetILFormatName(il_format));
    LOG_INFO(OS_TEXT("\t  type: ")+GetILTypeName(il_type));

    if(il_format==IL_COLOR_INDEX)
    {
        uint il_pattle=ilGetInteger(IL_PALETTE_TYPE);

        if(il_pattle==IL_PAL_RGB24||il_pattle==IL_PAL_BGR24
         ||il_pattle==IL_PAL_RGB32||il_pattle==IL_PAL_BGR32)
        {
            channel_count=3;
            il_format=IL_RGB;
            il_type=IL_UNSIGNED_BYTE;
            ilConvertImage(il_format,il_type);
            return(true);
        }
        else
        if(il_pattle==IL_PAL_RGBA32||il_pattle==IL_PAL_BGRA32)
        {
            channel_count=4;
            il_format=IL_RGBA;
            il_type=IL_UNSIGNED_BYTE;
            ilConvertImage(il_format,il_type);
            return(true);
        }
        else
        {
            LOG_ERROR("Don't support the pattle format.");
            return(false);
        }
    }
    
    if(il_format==IL_LUMINANCE||il_format==IL_ALPHA)channel_count=1;else
    if(il_format==IL_LUMINANCE_ALPHA)               channel_count=2;else
    if(il_format==IL_RGB||il_format==IL_BGR)        channel_count=3;else
    if(il_format==IL_RGBA||il_format==IL_BGRA)      channel_count=4;else
        channel_count=0;

    return(true);    
}

void *ILImage::ToRGB(ILuint type)
{
    if(il_format!=IL_RGB)
        Convert(IL_RGB,type);

    return ilGetData();
}

void *ILImage::ToGray(ILuint type)
{
    if(il_format!=IL_LUMINANCE)
        Convert(IL_LUMINANCE,type);

    return ilGetData();
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
