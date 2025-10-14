#include"TextureFileCreater.h"
#include<hgl/filesystem/FileSystem.h>
#include<hgl/log/log.h>

using namespace hgl::filesystem;

bool ToILType(ILuint &type,const uint8 bits,const VulkanBaseType cdt)
{
    constexpr ILuint target_type[3][(uint)VulkanBaseType::END_RANGE-1]=
    {
               //UINT                SINT,       UNORM              SNORM       USCALE,SSCALE,  UFLOAT      SFLOAT
        /*  8 */{IL_UNSIGNED_BYTE,   IL_BYTE,    IL_UNSIGNED_BYTE,  IL_BYTE,    0,0,            0,          0},
        /* 16 */{IL_UNSIGNED_SHORT,  IL_SHORT,   IL_UNSIGNED_SHORT, IL_SHORT,   0,0,            IL_HALF,    IL_HALF},
        /* 32 */{IL_UNSIGNED_INT,    IL_INT,     IL_UNSIGNED_INT,   IL_INT,     0,0,            IL_FLOAT,   IL_FLOAT}
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

constexpr char TEXTURE_FILE_HEADER[]="Texture";
constexpr uint TEXTURE_FILE_HEADER_LENGTH=sizeof(TEXTURE_FILE_HEADER)-1;

bool TextureFileCreater::CreateTexFile(const OSString &old_filename, const OSString &new_filename,const VkImageViewType &type)
{
    OSString pn,fn;

    SplitPath<os_char>(pn,fn,old_filename);

    if(type<VK_IMAGE_VIEW_TYPE_1D
     ||type>VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
    {
        LogError(OS_TEXT("TextureFileCreater::WriteFileHeader(")+old_filename+OS_TEXT(") texture type error that it's ")+OSString::numberOf(int(type)));
        return(false);
    }

    //filename=ReplaceExtension<os_char>(old_filename,TEXTURE_FILE_EXT_NAME[type]);
    filename=new_filename;

    if(!fos.CreateTrunc(new_filename))
        return(false);

    dos=new io::LEDataOutputStream(&fos);

    dos->Write(TEXTURE_FILE_HEADER,TEXTURE_FILE_HEADER_LENGTH);
    dos->WriteUint8(0);                                 //版本
    dos->WriteUint8(type);                              //类型

	return(true);
}

bool TextureFileCreater::WriteSize1D(const uint32 length)
{
    if(!dos->WriteUint32(length))return(false);

    if(!dos->WriteUint64(0))return(false);

    return(true);
}

bool TextureFileCreater::WriteSize2D(const uint32 width,const uint32 height)
{
    if(!dos->WriteUint32(width))return(false);
    if(!dos->WriteUint32(height))return(false);

    if(!dos->WriteUint32(0))return(false);

    return(true);
}

bool TextureFileCreater::WriteSize3D(const uint32 width,const uint32 height,const uint32 depth)
{
    if(!dos->WriteUint32(width))return(false);
    if(!dos->WriteUint32(height))return(false);
    if(!dos->WriteUint32(depth))return(false);

    return(true);
}

bool TextureFileCreater::WriteSize1DArray(const uint32 length,const uint32 layers)
{
    if(!dos->WriteUint32(length))return(false);
    if(!dos->WriteUint32(0))return(false);
    if(!dos->WriteUint32(layers))return(false);

    return(true);
}

bool TextureFileCreater::WriteSize2DArray(const uint32 width,const uint32 height,const uint32 layers)
{
    if(!dos->WriteUint32(width))return(false);
    if(!dos->WriteUint32(height))return(false);
    if(!dos->WriteUint32(layers))return(false);

    return(true);
}

bool TextureFileCreater::WritePixelFormat(const uint mip_level)
{
    if (pixel_format->format > ColorFormat::COMPRESS)
    {
        char spaces[10]={};

        if(!dos->WriteUint8(0))return(false);

        hgl::strcpy(spaces,9,pixel_format->name);       //压缩格式名字没有超过9字节的

        if(dos->Write(spaces,9)!=9)return(false);
    }
    else
    {
        if(!dos->WriteUint8(pixel_format->channels))return(false);						//颜色通道数
        if(!dos->WriteUint8((uint8*)pixel_format->color, 4))return(false);				//颜色标记
        if(!dos->WriteUint8(pixel_format->bits, 4))return(false);						//颜色位数
        if(!dos->WriteUint8((uint8)pixel_format->type))return(false);					//数据类型
    }

    if(!dos->WriteUint8(mip_level))return(false);                                       //mipmaps级数

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

TextureFileCreater *CreateTextureFileCreaterCompress(const PixelFormat *pf);

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