#include"TextureFileCreater.h"
#include"ImageLoader.h"
#include<hgl/log/log.h>
#include"Compressonator.h"
#include<cstring>
#include<cstdlib>

/**
 * AMD Compressonator 压缩编码器。
 *
 * 用传统 API（CMP_Texture + CMP_ConvertTexture，编码器编在库里，依赖 CMP_InitializeBCLibrary）。
 * 不用 CMP_MipSet + CMP_ProcessTexture：后者属 SDK 的 compute/插件体系，插件文件缺失时
 * 返回 CMP_ERR_PLUGIN_FILE_NOT_FOUND(15)，而失败时 MipSetOut 里的数据是"已按目标格式申请、
 * 从未写入"的内存——旧实现忽略返回值直接落盘，产出的 .Tex2D 载荷就是进程内存垃圾。
 */
class TextureFileCreaterCompressAMD:public TextureFileCreater
{
    int channels;
    int type;
    uint pixel_bytes;
    CMP_FORMAT source_fmt;
    CMP_FORMAT target_fmt;
    std::string target_fmt_name;

public:

    using TextureFileCreater::TextureFileCreater;

public:

    bool InitFormat(MagickImage* img) override
    {
        image = img;

        channels=image->channels();
        type=(int)image->pixelType();

        constexpr CMP_FORMAT fmt_list[]=
        {
            CMP_FORMAT_BC1, //ColorFormat::BC1
            CMP_FORMAT_BC1, //ColorFormat::BC1
            CMP_FORMAT_BC2, //ColorFormat::BC2
            CMP_FORMAT_BC3, //ColorFormat::BC3
            CMP_FORMAT_BC4, //ColorFormat::BC4
            CMP_FORMAT_BC5, //ColorFormat::BC5
            CMP_FORMAT_BC6H, //ColorFormat::BC6H
            CMP_FORMAT_BC6H_SF, //ColorFormat::BC6H_SF
            CMP_FORMAT_BC7, //ColorFormat::BC7
        };

        constexpr char fmt_name_list[][8]=
        {
            "BC1RGB",
            "BC1RGBA",
            "BC2",
            "BC3",
            "BC4",
            "BC5",
            "BC6H",
            "BC6H_SF",
            "BC7"
        };

        const int fmt_index=size_t(pixel_format->format)-size_t(ColorFormat::BC1RGB);

        target_fmt=fmt_list[fmt_index];

        target_fmt_name=fmt_name_list[fmt_index];

        // Log message similar to Intel compressor
        {
            AnsiString msg = "Compress Image to: ";
            msg += AnsiString::numberOf(image->width());
            msg += "x";
            msg += AnsiString::numberOf(image->height());
            msg += " ";
            msg += target_fmt_name.c_str();
            msg += " format.";
            LogInfo(msg.c_str());
        }

        if(type==(int)ImagePixelType::UInt8 ){pixel_bytes=1;}else
        if(type==(int)ImagePixelType::UInt16 ){pixel_bytes=2;}else
        if(type==(int)ImagePixelType::UInt32 ){pixel_bytes=4;}else
        if(type==(int)ImagePixelType::Float16 ){pixel_bytes=2;}else
        if(type==(int)ImagePixelType::Float32 ){pixel_bytes=4;}else
        {
            LogError(OS_TEXT("unknow type: %d"), type);
            return(false);
        }

        if(channels==1)
        {
            if(type==(int)ImagePixelType::UInt8 )source_fmt=CMP_FORMAT_R_8;else
            if(type==(int)ImagePixelType::UInt16 )source_fmt=CMP_FORMAT_R_16;else
            if(type==(int)ImagePixelType::Float16 )source_fmt=CMP_FORMAT_R_16F;else
            if(type==(int)ImagePixelType::Float32 )source_fmt=CMP_FORMAT_R_32F;else
            {
                LogError(OS_TEXT("channels1 unknow type: %d"), type);
                return(false);
            }

            image->ConvertToGray((ImagePixelType)type);
        }
        else
        if(channels==2)
        {
            if(type==(int)ImagePixelType::UInt8 )source_fmt=CMP_FORMAT_RG_8;else
            if(type==(int)ImagePixelType::UInt16 )source_fmt=CMP_FORMAT_RG_16;else
            if(type==(int)ImagePixelType::Float16 )source_fmt=CMP_FORMAT_RG_16F;else
            if(type==(int)ImagePixelType::Float32 )source_fmt=CMP_FORMAT_RG_32F;else
            {
                LogError(OS_TEXT("channels2 unknow type: %d"), type);
                return(false);
            }

            image->ConvertToRG((ImagePixelType)type);
        }
        else
        if(channels==3)
        {
            if(type==(int)ImagePixelType::UInt8 )source_fmt=CMP_FORMAT_RGBA_8888;else
            if(type==(int)ImagePixelType::UInt16 )source_fmt=CMP_FORMAT_RGBA_16;else
            if(type==(int)ImagePixelType::Float16 )source_fmt=CMP_FORMAT_RGBA_16F;else
            if(type==(int)ImagePixelType::Float32 )source_fmt=CMP_FORMAT_RGBA_32F;else
            {
                LogError(OS_TEXT("channels3 unknow type: %d"), type);
                return(false);
            }

            image->ConvertToRGBA((ImagePixelType)type);
        }
        else
        if(channels==4)
        {
            if(type==(int)ImagePixelType::UInt8 )source_fmt=CMP_FORMAT_RGBA_8888;else
            if(type==(int)ImagePixelType::UInt16 )source_fmt=CMP_FORMAT_RGBA_16;else
            if(type==(int)ImagePixelType::Float16 )source_fmt=CMP_FORMAT_RGBA_16F;else
            if(type==(int)ImagePixelType::Float32 )source_fmt=CMP_FORMAT_RGBA_32F;else
            {
                LogError(OS_TEXT("channels4 unknow type: %d"), type);
                return(false);
            }

            image->ConvertToRGBA((ImagePixelType)type);
        }
        else
        {
            LogError(OS_TEXT("unknow channels: %d"), channels);
            return(false);
        }

        return(true);
    }

    uint32 Write() override
    {
        const int width=image->width();
        const int height=image->height();

        // 3/4 通道都按 RGBA 取（InitFormat 已 ConvertToRGBA）
        const int src_channels=(channels>=3)?4:channels;

        void *source_data=nullptr;

        if(src_channels==1)
            source_data=image->GetGray((ImagePixelType)type);
        else
        if(src_channels==2)
            source_data=image->GetRG((ImagePixelType)type);
        else
            source_data=image->GetRGBA((ImagePixelType)type);

        if(!source_data)
        {
            LogError(OS_TEXT("Failed to get image data for compression"));
            return(0);
        }

        CMP_Texture src_tex;
        mem_zero(src_tex);

        src_tex.dwSize      =sizeof(CMP_Texture);
        src_tex.dwWidth     =width;
        src_tex.dwHeight    =height;
        src_tex.dwPitch     =width*pixel_bytes*src_channels;
        src_tex.format      =source_fmt;
        src_tex.dwDataSize  =src_tex.dwPitch*height;
        src_tex.pData       =(CMP_BYTE *)source_data;

        CMP_Texture dst_tex;
        mem_zero(dst_tex);

        dst_tex.dwSize      =sizeof(CMP_Texture);
        dst_tex.dwWidth     =width;
        dst_tex.dwHeight    =height;
        dst_tex.format      =target_fmt;
        dst_tex.dwDataSize  =CMP_CalculateBufferSize(&dst_tex);
        dst_tex.pData       =(CMP_BYTE *)malloc(dst_tex.dwDataSize);

        if(!dst_tex.pData)
        {
            LogError(OS_TEXT("Failed to allocate destination buffer for compression"));
            delete [] (uint8_t *)source_data;
            return(0);
        }

        CMP_CompressOptions options;
        mem_zero(options);

        options.dwSize      =sizeof(CMP_CompressOptions);
        options.fquality    =1.0f;
        options.dwnumThreads=(target_fmt==CMP_FORMAT_BC4)?1:8;      // BC4单线程

        const CMP_ERROR cmp_result=CMP_ConvertTexture(&src_tex,&dst_tex,&options,nullptr);

        // 编码失败必须报错返回：否则 dst_tex 里是"已按目标格式申请、从未写入"的内存，落盘即垃圾
        if(cmp_result!=CMP_OK)
        {
            LogError(OS_TEXT("CMP_ConvertTexture failed, error code: %d"),(int)cmp_result);
            free(dst_tex.pData);
            delete [] (uint8_t *)source_data;
            return(0);
        }

        // Log final compressed size similar to Intel
        {
            AnsiString msg = "Compress Image To: ";
            msg += AnsiString::numberOf(width);
            msg += "x";
            msg += AnsiString::numberOf(height);
            msg += " ";
            msg += AnsiString::numberOf((uint)dst_tex.dwDataSize);
            msg += " bytes.";
            LogInfo(msg.c_str());
        }

        const uint32 result=TextureFileCreater::Write(dst_tex.pData,dst_tex.dwDataSize);

        free(dst_tex.pData);
        delete [] (uint8_t *)source_data;

        return result;
    }
};//class TextureFileCreaterCompressAMD:public TextureFileCreater

TextureFileCreater *CreateTextureFileCreaterCompressAMD(const PixelFormat *pf)
{
    return(new TextureFileCreaterCompressAMD(pf));
}
