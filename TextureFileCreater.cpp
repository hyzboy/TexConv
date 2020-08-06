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
    inline bool SplitFilename(BaseString<T> &pathname,BaseString<T> &filename,const BaseString<T> &fullname)
    {
        if(fullname.Length()<=1)
            return false;

        const T spear_char[] = { '/','\\' };

        const int pos=fullname.FindRightChar(spear_char);

        if(pos==-1)
            return(false);

        pathname.Strcpy(fullname,pos);
        filename.Strcpy(fullname.c_str()+pos+1);

        return(true);
    }

    template<typename T>
    inline BaseString<T> ReplaceExtName(const BaseString<T> &old_name,const BaseString<T> &new_extname,const T split_char='.')
    {
        if(old_name.Length()<=1)
            return(BaseString<T>::charOf(split_char)+new_extname);

        const int pos=old_name.FindRightChar(split_char);

        if(pos!=-1)
            return old_name.SubString(0,pos)+new_extname;
        else
            return old_name+BaseString<T>::charOf(split_char)+new_extname;
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

TextureFileCreater::TextureFileCreater(const PixelFormat *pf,ILImage *img)
{
    fmt=pf;
    image=img;

    pixel_total=image->pixel_total();
    pixel_bytes=pf->GetPixelBytes();
    total_bytes=pixel_total*pixel_bytes;

    dos=nullptr;
}
TextureFileCreater::~TextureFileCreater()
{
    SAFE_CLEAR(dos);
}

bool TextureFileCreater::WriteFileHeader(const OSString &old_filename)
{
    OSString pn,fn;

    SplitFilename<os_char>(pn,fn,old_filename);

    filename=ReplaceExtName<os_char>(old_filename,OS_TEXT(".Tex2D"));

    if(!fos.CreateTrunc(filename))
        return(false);

    dos=new io::LEDataOutputStream(&fos);

    dos->Write("Tex2D\x1A",6);
    dos->WriteUint8(2);                                 //版本
    dos->WriteUint8(0);                                 //mipmaps级数
    dos->WriteUint32(image->width());
    dos->WriteUint32(image->height());

    return(true);
}

bool TextureFileCreater::Write(void *data)
{
    if(fmt->format>ColorFormat::COMPRESS)
    {
        dos->WriteUint8(0);
        dos->WriteUint16(uint(fmt->format)-uint(ColorFormat::COMPRESS));
        dos->WriteUint32(total_bytes);
    }
    else
    {
        dos->WriteUint8(fmt->channels);                     //颜色通道数
        dos->WriteUint8((uint8 *)fmt->color,4);             //颜色标记
        dos->WriteUint8(fmt->bits,4);                       //颜色位数
        dos->WriteUint8((uint8)fmt->type);                  //数据类型
    }

    return(dos->Write(data,total_bytes)==total_bytes);
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