//注：起名为ILImageSupport是为了避免与IL中现有的ilimage冲突

#include"ILImage.h"
#include<IL/ilu.h>
#include<hgl/log/LogInfo.h>
#include<hgl/filesystem/FileSystem.h>

using namespace hgl;

namespace
{
    struct DevILError
    {
        uint code;
        const os_char *name;
    };
    
    constexpr const DevILError devil_error_text[]=
    {
    #define DEVIL_ERROR_TEXT(name)  {IL_##name,OS_TEXT(#name)},

        DEVIL_ERROR_TEXT(NO_ERROR)
        DEVIL_ERROR_TEXT(INVALID_ENUM)
        DEVIL_ERROR_TEXT(OUT_OF_MEMORY)
        DEVIL_ERROR_TEXT(FORMAT_NOT_SUPPORTED)
        DEVIL_ERROR_TEXT(INTERNAL_ERROR)
        DEVIL_ERROR_TEXT(INVALID_VALUE)
        DEVIL_ERROR_TEXT(ILLEGAL_OPERATION)
        DEVIL_ERROR_TEXT(ILLEGAL_FILE_VALUE)
        DEVIL_ERROR_TEXT(INVALID_FILE_HEADER)
        DEVIL_ERROR_TEXT(INVALID_PARAM)
        DEVIL_ERROR_TEXT(COULD_NOT_OPEN_FILE)
        DEVIL_ERROR_TEXT(INVALID_EXTENSION)
        DEVIL_ERROR_TEXT(FILE_ALREADY_EXISTS)
        DEVIL_ERROR_TEXT(OUT_FORMAT_SAME)
        DEVIL_ERROR_TEXT(STACK_OVERFLOW)
        DEVIL_ERROR_TEXT(STACK_UNDERFLOW)
        DEVIL_ERROR_TEXT(INVALID_CONVERSION)
        DEVIL_ERROR_TEXT(BAD_DIMENSIONS)
        DEVIL_ERROR_TEXT(FILE_READ_ERROR)
        DEVIL_ERROR_TEXT(FILE_WRITE_ERROR)

        DEVIL_ERROR_TEXT(LIB_GIF_ERROR)
        DEVIL_ERROR_TEXT(LIB_JPEG_ERROR)
        DEVIL_ERROR_TEXT(LIB_PNG_ERROR)
        DEVIL_ERROR_TEXT(LIB_TIFF_ERROR)
        DEVIL_ERROR_TEXT(LIB_MNG_ERROR)
        DEVIL_ERROR_TEXT(LIB_JP2_ERROR)
        DEVIL_ERROR_TEXT(LIB_EXR_ERROR)
        DEVIL_ERROR_TEXT(UNKNOWN_ERROR)

    #undef DEVIL_ERROR_TEXT
    };

    const os_char *GetDevILErrorString(const uint code)
    {
        for(const DevILError err:devil_error_text)
            if(err.code==code)
                return err.name;

        return nullptr;
    }    

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

ILImage::ILImage(ILImage *img):ILImage()
{
    ilGenImages(1,&il_index);
    Bind();
    ilCopyImage(img->il_index);
    Refresh();
}

ILImage::~ILImage()
{
    ilDeleteImages(1,&il_index);
}

void ILImage::Copy(ILImage *img)
{
    ilCopyImage(img->il_index);
    Refresh();
}

void ILImage::Refresh()
{
    il_width	=ilGetInteger(IL_IMAGE_WIDTH);
    il_height	=ilGetInteger(IL_IMAGE_HEIGHT);
    il_depth    =ilGetInteger(IL_IMAGE_DEPTH);
    il_bit		=ilGetInteger(IL_IMAGE_BITS_PER_PIXEL);
    il_format	=ilGetInteger(IL_IMAGE_FORMAT);
    il_type	    =ilGetInteger(IL_IMAGE_TYPE);

    if(ilGetInteger(IL_IMAGE_ORIGIN)==IL_ORIGIN_LOWER_LEFT)
        iluFlipImage();

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
        }
        else
        if(il_pattle==IL_PAL_RGBA32||il_pattle==IL_PAL_BGRA32)
        {
            channel_count=4;
            il_format=IL_RGBA;
            il_type=IL_UNSIGNED_BYTE;
            ilConvertImage(il_format,il_type);
        }
        else
        {
            LOG_ERROR("Don't support the pattle format.");
        }
    }

    il_bit		=ilGetInteger(IL_IMAGE_BITS_PER_PIXEL);
    
    if(il_format==IL_LUMINANCE||il_format==IL_ALPHA)channel_count=1;else
    if(il_format==IL_LUMINANCE_ALPHA)               channel_count=2;else
    if(il_format==IL_RGB||il_format==IL_BGR)        channel_count=3;else
    if(il_format==IL_RGBA||il_format==IL_BGRA)      channel_count=4;else
        channel_count=0;
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

    iluImageParameter(ILU_FILTER,ILU_LINEAR);

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

    if(!ilConvertImage(format,type))
        return(false);
    
    il_format   =format;
    il_type     =type;
    il_bit		=ilGetInteger(IL_IMAGE_BITS_PER_PIXEL);
    return(true);
}

bool ILImage::LoadFile(const OSString &filename)
{
    Bind();

    if(!filesystem::FileExist(filename))
    {
        LOG_ERROR(OS_TEXT("Can't find filename: ")+filename);
        return(false);
    }

    if(!ilLoadImage(filename.c_str()))
    {
        ILenum il_err_code=ilGetError();

        const os_char *err_text=GetDevILErrorString(il_err_code);        
        
        LOG_ERROR(OS_TEXT("can't Load image file <")+filename+OS_TEXT("> Error: ")+(err_text?err_text:OSString::numberOf(il_err_code)));

        return(false);
    }

    LOG_INFO(OS_TEXT("\nFile: ")+filename);

    Refresh();

    LOG_INFO(OS_TEXT("\t width: ")+OSString::numberOf(il_width));
    LOG_INFO(OS_TEXT("\theight: ")+OSString::numberOf(il_height));
    LOG_INFO(OS_TEXT("\t depth: ")+OSString::numberOf(il_depth));
    LOG_INFO(OS_TEXT("\t   bit: ")+OSString::numberOf(il_bit));
    LOG_INFO(OS_TEXT("\tformat: ")+GetILFormatName(il_format));
    LOG_INFO(OS_TEXT("\t  type: ")+GetILTypeName(il_type));

    return(true);    
}

const PixelDataType GetPixelDataType(ILuint type)
{
    if(type==IL_UNSIGNED_BYTE   )return PixelDataType::U8; else
    if(type==IL_UNSIGNED_SHORT  )return PixelDataType::U16;else
    if(type==IL_UNSIGNED_INT    )return PixelDataType::U32;else
    if(type==IL_FLOAT           )return PixelDataType::F32;else
    if(type==IL_DOUBLE          )return PixelDataType::F64;else
        return(PixelDataType::Unknow);
}

template<typename T> void MixRG(T *tar,T *src,T *alpha,const uint count)
{
}

//Image2D *ILImage::CreateImage2D()
//{
//    const PixelDataType pdt=GetPixelDataType(il_type);
//    void *src=nullptr;
//    const uint pixel_count=il_width*il_height;
//
//    if(channel_count==1)
//    {
//        if(il_format==IL_ALPHA      )src=ilGetAlpha(il_type);else
//        if(il_format==IL_LUMINANCE  )src=ilGetData();else
//            return(nullptr);
//    }
//    else
//    if(channel_count==2)
//    {
//        src=GetRG(il_type);
//    }
//    else
//    if(channel_count==3)
//    {
//        src=GetRGB(il_type);
//    }
//    else
//    if(channel_count==4)
//    {
//        src=GetRGBA(il_type);
//    }
//    
//    const uint pixel_byte=(il_bit>>3);
//    const uint total_bytes=pixel_count*pixel_byte;
//
//    void *img_data=new uint8[total_bytes];
//
//    memcpy(img_data,src,total_bytes);
//
//    return(new Image2D(il_width,il_height,channel_count,pdt,img_data));
//}

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

void *ILImage::GetAlpha(ILuint type)
{
    return ilGetAlpha(type);
}

void *ILImage::GetR(ILuint type)
{
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
    if(il_format!=format||il_type!=type)
    if(!Convert(format,type))
        return nullptr;

    return ilGetData();
}

template<typename T> void MixRGBA(T *rgba,T *alpha,int size)
{
	int i;

    for(i=0;i<size;i++)
    {
    	rgba+=3;
        *rgba++=*alpha++;
    }
}

void *ILImage::GetRGBA(ILuint type)
{
    void *data=GetData(IL_RGBA,type);
    void *alpha=ilGetAlpha(type);

    const int size=width()*height();

    if(type==IL_UNSIGNED_BYTE   ||type==IL_BYTE                 )MixRGBA<uint8 >((uint8  *)data,(uint8  *)alpha,size);else
    if(type==IL_UNSIGNED_SHORT  ||type==IL_SHORT||type==IL_HALF )MixRGBA<uint16>((uint16 *)data,(uint16 *)alpha,size);else
    if(type==IL_UNSIGNED_INT    ||type==IL_INT  ||type==IL_FLOAT)MixRGBA<uint32>((uint32 *)data,(uint32 *)alpha,size);else
    if(type==IL_DOUBLE                                          )MixRGBA<uint64>((uint64 *)data,(uint64 *)alpha,size);else
    return(nullptr);

    return data;
}