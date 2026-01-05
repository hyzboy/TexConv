#include"TextureFileCreater.h"
#include"ImageLoader.h"
#include<hgl/log/log.h>
#include"ISPCTextureCompressor/ispc_texcomp/ispc_texcomp.h"
#include<cstring>
#include<cstdlib>

class TextureFileCreaterCompressIntel:public TextureFileCreater
{
    int channels;
    int type;
    uint pixel_bytes;
    std::string target_fmt_name;

public:

    using TextureFileCreater::TextureFileCreater;

public:

    bool InitFormat(ImageLoader* img) override
    {
        image = img;

        channels=image->channels();
        type=image->type();
        
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

        target_fmt_name=fmt_name_list[fmt_index];

        // build OSString message
        AnsiString msg = "Compress Image to: ";
        msg += AnsiString::numberOf(image->width());
        msg += "x";
        msg += AnsiString::numberOf(image->height());
        msg += " ";
        msg += target_fmt_name.c_str();
        msg += " format.";

        LogInfo(msg.c_str());

        if(type==IL_UNSIGNED_BYTE   ){pixel_bytes=1;}else
        if(type==IL_UNSIGNED_SHORT  ){pixel_bytes=2;}else
        if(type==IL_UNSIGNED_INT    ){pixel_bytes=4;}else
        if(type==IL_HALF            ){pixel_bytes=2;}else
        if(type==IL_FLOAT           ){pixel_bytes=4;}else
        {
            LogError(OS_TEXT("unknow type: %d"), type);
            return(false);
        }
        
        if(channels==1)
        {
            image->ConvertToLum(type);
        }
        else
        if(channels==2)
        {
            image->ConvertToRG(type);
        }
        else
        if(channels==3)
        {
            // convert RGB -> RGBA for compressor convenience
            image->ConvertToRGBA(type);
        }
        else
        if(channels==4)
        {
            image->ConvertToRGBA(type);
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

        // get source data pointer and channel count after conversions
        void *source_data=nullptr;
        int src_channels = (channels>=3)?4:channels; // after ConvertToRGBA for 3/4

        if(src_channels==1)
            source_data=image->GetLum(type);
        else if(src_channels==2)
            source_data=image->GetRG(type);
        else // 3/4 -> GetRGBA
            source_data=image->GetRGBA(type);

        if(!source_data)
        {
            LogError(OS_TEXT("Failed to get image data for compression"));
            return(0);
        }

        // Prepare src surface for ISPC compressor
        rgba_surface src{};
        src.ptr = reinterpret_cast<uint8_t*>(source_data);
        src.width = width;
        src.height = height;
        src.stride = width * (pixel_bytes * src_channels);

        // compute destination size (in blocks of 4x4)
        const int block_x = (width + 3) / 4;
        const int block_y = (height + 3) / 4;
        const int blocks = block_x * block_y;

        // Determine bytes per compressed block. Most block-compression formats
        // use 4x4 blocks. BC1 and BC4 store 8 bytes per block; BC2/BC3/BC5/BC6H/BC7 store 16 bytes per block.
        int bytes_per_block = 16; // default to 16

        if(target_fmt_name == "BC1RGB" || target_fmt_name == "BC1RGBA" || target_fmt_name == "BC4")
            bytes_per_block = 8;
        else
            bytes_per_block = 16;

        const size_t dst_size = size_t(blocks) * size_t(bytes_per_block);

        uint8_t *dst = new uint8_t[dst_size];
        if(!dst)
        {
            LogError(OS_TEXT("Failed to allocate destination buffer for compression"));
            return(0);
        }

        // Choose compressor based on target format
        if(target_fmt_name == "BC1RGB" || target_fmt_name == "BC1RGBA")
        {
            CompressBlocksBC1(&src, dst);
        }
        else if(target_fmt_name == "BC2")
        {
            // No direct BC2 in ISPC compressor; use BC3 as fallback
            CompressBlocksBC3(&src, dst);
        }
        else if(target_fmt_name == "BC3")
        {
            CompressBlocksBC3(&src, dst);
        }
        else if(target_fmt_name == "BC4")
        {
            CompressBlocksBC4(&src, dst);
        }
        else if(target_fmt_name == "BC5")
        {
            CompressBlocksBC5(&src, dst);
        }
        else if(target_fmt_name == "BC6H"
              ||target_fmt_name == "BC6H_SF")
        {
            bc6h_enc_settings settings{};
            // use very slow profile for BC6H
            GetProfile_bc6h_veryslow(&settings);
            CompressBlocksBC6H(&src, dst, &settings);
        }
        else if(target_fmt_name == "BC7")
        {
            bc7_enc_settings settings{};

            // choose slow/very slow profile based on presence of alpha
            if(src_channels==4)
                GetProfile_alpha_slow(&settings);
            else
                GetProfile_slow(&settings);

            CompressBlocksBC7(&src, dst, &settings);
        }
        else
        {
            LogError(OS_TEXT("Don't support this compressed format"));
            delete [] dst;
            return(0);
        }

        // build message
        AnsiString msg = "Compress Image To: ";
        msg += AnsiString::numberOf(width);
        msg += "x";
        msg += AnsiString::numberOf(height);
        msg += " ";
        msg += AnsiString::numberOf((uint)dst_size);
        msg += " bytes.";
        
        LogInfo(msg.c_str());

        uint32 result = TextureFileCreater::Write(dst, (uint)dst_size);

        delete [] dst;

        return result;
    }
};//class TextureFileCreaterCompress:public TextureFileCreater

TextureFileCreater *CreateTextureFileCreaterCompressIntel(const PixelFormat *pf)
{
    return(new TextureFileCreaterCompressIntel(pf));
}
