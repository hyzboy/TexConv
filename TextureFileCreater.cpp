#include"TextureFileCreater.h"
#include<hgl/filesystem/FileSystem.h>

namespace
{
    /**
    * 截取完整路径中的路径名和文件名
    * @param pathname 拆分后的路径名
    * @param filename 拆分后的文件名
    * @param fullname 拆分前的完整路径文件名
    */
    template<typename T>
    inline bool SplitFilename(String<T> &pathname,String<T> &filename,const String<T> &fullname)
    {
        if(fullname.Length()<=1)
            return false;

        const T spear_char[] = { '/','\\' };

        const int pos=fullname.FindRightChars(spear_char);

        if(pos==-1)
            return(false);

        pathname.Strcpy(fullname,pos);
        filename.Strcpy(fullname.c_str()+pos+1);

        return(true);
    }

    template<typename T>
    inline String<T> ReplaceExtName(const String<T> &old_name,const String<T> &new_extname,const T split_char='.')
    {
        if(old_name.Length()<=1)
            return(String<T>::charOf(split_char)+new_extname);

        const int pos=old_name.FindRightChar(split_char);

        if(pos!=-1)
            return old_name.SubString(0,pos)+new_extname;
        else
            return old_name+String<T>::charOf(split_char)+new_extname;
    }
}//namespace

bool ToILType(ILuint &type,const uint8 bits,const ColorDataType cdt)
{
    constexpr ILuint target_type[3][(uint)ColorDataType::END-1]=
    {
                //UNORM             SNORM       UINT                SINT,       USCALE,SSCALE,  UFLOAT      SFLOAT
        /*  8 */{IL_UNSIGNED_BYTE,  IL_BYTE,    IL_UNSIGNED_BYTE,   IL_BYTE,    0,0,            0,          0},
        /* 16 */{IL_UNSIGNED_SHORT, IL_SHORT,   IL_UNSIGNED_SHORT,  IL_SHORT,   0,0,            IL_HALF,    IL_HALF},
        /* 32 */{IL_UNSIGNED_INT,   IL_INT,     IL_UNSIGNED_INT,    IL_INT,     0,0,            IL_FLOAT,   IL_FLOAT}
    };

    if(bits<=8  )type=target_type[0][(uint)cdt-1];else
    if(bits<=16 )type=target_type[1][(uint)cdt-1];else
    if(bits<=32 )type=target_type[2][(uint)cdt-1];else
        return(false);

    return(type);
}

TextureFileCreater::TextureFileCreater(const PixelFormat *pf)
{
    pixel_format=pf;

    dos=nullptr;
    image=nullptr;
}

TextureFileCreater::~TextureFileCreater()
{
    SAFE_CLEAR(dos);
}

const char texture_file_type_name[uint(TextureFileType::RANGE_SIZE)][16]=
{
    "Tex1D",
    "Tex2D",
    "Tex3D",
    "TexCubemap",
    "Tex1DArray",
    "Tex2DArray",
    "TexCubemapArray"
};

bool TextureFileCreater::CreateTexFile(const OSString& old_filename, const TextureFileType& type)
{
    OSString pn,fn;

    SplitFilename<os_char>(pn,fn,old_filename);

    if(!RangeCheck<TextureFileType>(type))
    {
        LOG_ERROR(OS_TEXT("TextureFileCreater::WriteFileHeader(")+old_filename+OS_TEXT(") texture type error that it's ")+OSString::valueOf(int(type)));
        return(false);
    }

    AnsiString file_type_name=texture_file_type_name[uint(type)];
    OSString file_ext_name=OS_TEXT(".")+ToOSString(file_type_name);

    filename=ReplaceExtName<os_char>(old_filename,file_ext_name);

    if(!fos.CreateTrunc(filename))
        return(false);

    dos=new io::LEDataOutputStream(&fos);

    dos->Write(file_type_name.c_str(),file_type_name.Length());
    dos->WriteUint8(0x1A);
    dos->WriteUint8(3);                                 //版本

	return(true);
}

bool TextureFileCreater::WriteSize1D(const uint mip_level,const uint length)
{
    if(!dos->WriteUint8(mip_level))return(false);                         //mipmaps级数
    if(!dos->WriteUint32(length))return(false);

    return(true);
}

bool TextureFileCreater::WriteSize2D(const uint mip_level, const uint width,const uint height)
{
    if(!dos->WriteUint8(mip_level))return(false);                         //mipmaps级数
    if(!dos->WriteUint32(width))return(false);
    if(!dos->WriteUint32(height))return(false);

    return(true);
}

bool TextureFileCreater::WritePixelFormat()
{
    if (pixel_format->format > ColorFormat::COMPRESS)
    {
        if(!dos->WriteUint8(0))return(false);
        if(!dos->WriteUint16(uint(pixel_format->format)-uint(ColorFormat::BC1RGB)))return(false);
    }
    else
    {
        if(!dos->WriteUint8(pixel_format->channels))return(false);						//颜色通道数
        if(!dos->WriteUint8((uint8*)pixel_format->color, 4))return(false);				//颜色标记
        if(!dos->WriteUint8(pixel_format->bits, 4))return(false);						//颜色位数
        if(!dos->WriteUint8((uint8)pixel_format->type))return(false);					//数据类型
    }

	return(true);
}

uint32 TextureFileCreater::Write(void *data,const uint total_bytes)
{
    const uint64 space=0;

    if(dos->Write(data,total_bytes)!=total_bytes)
        return(0);

    if(total_bytes<8)    
    {
        if(dos->Write(&space,8-total_bytes)!=8-total_bytes)
            return(0);

        return 8;
    }

    return total_bytes;
}

void TextureFileCreater::Close()
{
    SAFE_CLEAR(dos);
    fos.Close();
}

void TextureFileCreater::Delete()
{
    Close();

    filesystem::FileDelete(filename);
}

TextureFileCreater *CreateTextureFileCreaterR(const PixelFormat *);
TextureFileCreater *CreateTextureFileCreaterRG(const PixelFormat *);
TextureFileCreater *CreateTextureFileCreaterRGB(const PixelFormat *);
TextureFileCreater *CreateTextureFileCreaterRGBA(const PixelFormat *);

TextureFileCreater *CreateTextureFileCreaterCompress(const PixelFormat *);

TextureFileCreater *CreateTFC(const PixelFormat *fmt,const int channels)
{
    if(!fmt)return(nullptr);
    if(channels<1||channels>4)return(nullptr);

    using CTFC_FUNC=TextureFileCreater *(*)(const PixelFormat *);

    static CTFC_FUNC CreateTFC[4]=
    {
        CreateTextureFileCreaterR,
        CreateTextureFileCreaterRG,
        CreateTextureFileCreaterRGB,
        CreateTextureFileCreaterRGBA
    };

    if(fmt->format<ColorFormat::COMPRESS)
        return CreateTFC[channels-1](fmt);
    else
        return CreateTextureFileCreaterCompress(fmt);
}