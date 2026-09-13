// TexEncIntel.dll —— Intel ISPC Texture Compressor 编码器插件。
// 编码逻辑逐行迁自原 TextureFileCreaterCompressIntel.cpp。
// 本 DLL 仅在 ISPCTextureCompressor kernel*.obj 存在时构建(CMake 探测)。

#include "texconv/texenc_plugin.h"
#include "texconv/tex_formats.h"
#include "texconv/tex_pixel.h"

#include "ispc_texcomp/ispc_texcomp.h"

#include <cstring>
#include <cstdlib>

namespace
{
    bool is_bc1(const char *name)
    {
        return strcmp(name, "BC1RGB") == 0 || strcmp(name, "BC1RGBA") == 0;
    }

    bool is_bc4(const char *name)
    {
        return strcmp(name, "BC4") == 0;
    }

    int pixel_bytes_of(int src_pixel_type)
    {
        switch(src_pixel_type)
        {
            case TEX_PT_UInt8:   return 1;
            case TEX_PT_UInt16:  return 2;
            case TEX_PT_UInt32:  return 4;
            case TEX_PT_Float16: return 2;
            case TEX_PT_Float32: return 4;
            default:             return 0;
        }
    }

    int Intel_Init(void)
    {
        return TEX_OK;
    }

    void Intel_Shutdown(void)
    {
    }

    int Intel_QueryTargets(const char **names, int names_count)
    {
        constexpr const char *list[] =
        {
            "BC1RGB", "BC1RGBA", "BC2", "BC3", "BC4", "BC5", "BC6H", "BC6H_SF", "BC7"
        };
        constexpr int n = int(sizeof(list) / sizeof(list[0]));

        if(!names)return n;
        if(names_count < n)return -1;

        for(int i = 0; i < n; i++)
            names[i] = list[i];

        return n;
    }

    int Intel_QuerySourceLayout(const char *target_format,
                                int src_channels, int src_pixel_type,
                                int *out_layout, int *out_pixel_type)
    {
        const TexPixelFormat *pf = TexFormat_Get(target_format);

        if(!pf || !TexFormat_IsCompress(pf))
            return TEX_ERR_UNSUPPORTED;

        if(pixel_bytes_of(src_pixel_type) <= 0)
            return TEX_ERR_UNSUPPORTED;

        // 旧版语义:1通道→Gray,2通道→GrayAlpha,3/4通道→RGBA;像素类型保持不变
        switch(src_channels)
        {
            case 1: *out_layout = TEX_LAYOUT_Gray;      break;
            case 2: *out_layout = TEX_LAYOUT_GrayAlpha; break;
            case 3:
            case 4: *out_layout = TEX_LAYOUT_RGBA;      break;
            default: return TEX_ERR_PARAM;
        }

        *out_pixel_type = src_pixel_type;
        return TEX_OK;
    }

    int Intel_Encode(const TexEncodeRequest *req, uint8_t **out_data, size_t *out_bytes)
    {
        if(!req || !req->src || !out_data || !out_bytes)
            return TEX_ERR_PARAM;

        const TexPixelFormat *pf = TexFormat_Get(req->target_format);

        if(!pf || !TexFormat_IsCompress(pf))
            return TEX_ERR_UNSUPPORTED;

        const char *target = req->target_format;

        const int pixel_bytes = pixel_bytes_of(req->src_pixel_type);

        if(pixel_bytes <= 0)
            return TEX_ERR_UNSUPPORTED;

        const int src_channels = (req->src_layout == TEX_LAYOUT_RGBA) ? 4
                               : (req->src_layout == TEX_LAYOUT_GrayAlpha) ? 2
                               : 1;

        // ISPC 压缩器输入表面
        rgba_surface src;
        memset(&src, 0, sizeof(src));

        src.ptr    = const_cast<uint8_t *>(req->src);
        src.width  = int(req->width);
        src.height = int(req->height);
        src.stride = int(req->width) * (pixel_bytes * src_channels);

        // 4x4 块;BC1/BC4 每块 8 字节,其余 16 字节
        const int block_x = (src.width  + 3) / 4;
        const int block_y = (src.height + 3) / 4;
        const int bytes_per_block = (is_bc1(target) || is_bc4(target)) ? 8 : 16;

        const size_t dst_size = size_t(block_x) * size_t(block_y) * size_t(bytes_per_block);

        uint8_t *dst = new uint8_t[dst_size];

        if(is_bc1(target))
        {
            CompressBlocksBC1(&src, dst);
        }
        else if(strcmp(target, "BC2") == 0)
        {
            // ISPC 无 BC2,旧实现以 BC3 兜底
            CompressBlocksBC3(&src, dst);
        }
        else if(strcmp(target, "BC3") == 0)
        {
            CompressBlocksBC3(&src, dst);
        }
        else if(is_bc4(target))
        {
            CompressBlocksBC4(&src, dst);
        }
        else if(strcmp(target, "BC5") == 0)
        {
            CompressBlocksBC5(&src, dst);
        }
        else if(strcmp(target, "BC6H") == 0 || strcmp(target, "BC6H_SF") == 0)
        {
            bc6h_enc_settings settings;
            memset(&settings, 0, sizeof(settings));
            GetProfile_bc6h_veryslow(&settings);
            CompressBlocksBC6H(&src, dst, &settings);
        }
        else if(strcmp(target, "BC7") == 0)
        {
            bc7_enc_settings settings;
            memset(&settings, 0, sizeof(settings));

            if(src_channels == 4)
                GetProfile_alpha_slow(&settings);
            else
                GetProfile_slow(&settings);

            CompressBlocksBC7(&src, dst, &settings);
        }
        else
        {
            delete[] dst;
            return TEX_ERR_UNSUPPORTED;
        }

        *out_data  = dst;
        *out_bytes = dst_size;
        return TEX_OK;
    }

    void Intel_FreeResult(uint8_t *data)
    {
        delete[] data;
    }

    TexEncoderProvider g_provider =
    {
        TEXENC_ABI_VERSION,
        "Intel ISPC Texture Compressor",
        "Intel",
        Intel_Init,
        Intel_Shutdown,
        Intel_QueryTargets,
        Intel_QuerySourceLayout,
        Intel_Encode,
        Intel_FreeResult,
    };
}//namespace

extern "C" TEXENC_EXPORT const TexEncoderProvider *TexGetEncoderProvider(void)
{
    return &g_provider;
}
