// TexEncAMD.dll —— AMD Compressonator 编码器插件。
// 编码逻辑逐行迁自原 TextureFileCreaterCompressAMD.cpp(传统 CMP_Texture/CMP_ConvertTexture 路径)。

#include "texconv/texenc_plugin.h"
#include "texconv/tex_formats.h"
#include "texconv/tex_pixel.h"

#include "Compressonator.h"

#include <cstring>
#include <cstdlib>

namespace
{
    constexpr CMP_FORMAT target_cmp_format(int fmt_index)
    {
        constexpr CMP_FORMAT fmt_list[] =
        {
            CMP_FORMAT_BC1,     // TEX_FMT_BC1RGB
            CMP_FORMAT_BC1,     // TEX_FMT_BC1RGBA
            CMP_FORMAT_BC2,     // TEX_FMT_BC2
            CMP_FORMAT_BC3,     // TEX_FMT_BC3
            CMP_FORMAT_BC4,     // TEX_FMT_BC4
            CMP_FORMAT_BC5,     // TEX_FMT_BC5
            CMP_FORMAT_BC6H,    // TEX_FMT_BC6H
            CMP_FORMAT_BC6H_SF, // TEX_FMT_BC6H_SF
            CMP_FORMAT_BC7,     // TEX_FMT_BC7
        };
        return fmt_list[fmt_index];
    }

    bool is_bc4(const char *name)
    {
        return strcmp(name, "BC4") == 0;
    }

    /// 对齐旧版:源像素类型→CMP_FORMAT;不支持(整型 32 位等)返回 CMP_FORMAT_Unknown
    CMP_FORMAT source_cmp_format(int src_channels, int src_pixel_type)
    {
        switch(src_channels)
        {
            case 1:
                switch(src_pixel_type)
                {
                    case TEX_PT_UInt8:   return CMP_FORMAT_R_8;
                    case TEX_PT_UInt16:  return CMP_FORMAT_R_16;
                    case TEX_PT_Float16: return CMP_FORMAT_R_16F;
                    case TEX_PT_Float32: return CMP_FORMAT_R_32F;
                    default:             return CMP_FORMAT_Unknown;
                }

            case 2:
                switch(src_pixel_type)
                {
                    case TEX_PT_UInt8:   return CMP_FORMAT_RG_8;
                    case TEX_PT_UInt16:  return CMP_FORMAT_RG_16;
                    case TEX_PT_Float16: return CMP_FORMAT_RG_16F;
                    case TEX_PT_Float32: return CMP_FORMAT_RG_32F;
                    default:             return CMP_FORMAT_Unknown;
                }

            case 3:
            case 4:
                switch(src_pixel_type)
                {
                    case TEX_PT_UInt8:   return CMP_FORMAT_RGBA_8888;
                    case TEX_PT_UInt16:  return CMP_FORMAT_RGBA_16;
                    case TEX_PT_Float16: return CMP_FORMAT_RGBA_16F;
                    case TEX_PT_Float32: return CMP_FORMAT_RGBA_32F;
                    default:             return CMP_FORMAT_Unknown;
                }

            default:
                return CMP_FORMAT_Unknown;
        }
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

    const TexPixelFormat *lookup(const char *name)
    {
        return TexFormat_Get(name);
    }

    int AMD_Init(void)
    {
        CMP_InitializeBCLibrary();
        return TEX_OK;
    }

    void AMD_Shutdown(void)
    {
        CMP_ShutdownBCLibrary();
    }

    int AMD_QueryTargets(const char **names, int names_count)
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

    int AMD_QuerySourceLayout(const char *target_format,
                              int src_channels, int src_pixel_type,
                              int *out_layout, int *out_pixel_type)
    {
        const TexPixelFormat *pf = lookup(target_format);

        if(!pf || !TexFormat_IsCompress(pf))
            return TEX_ERR_UNSUPPORTED;

        if(source_cmp_format(src_channels, src_pixel_type) == CMP_FORMAT_Unknown)
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

    int AMD_Encode(const TexEncodeRequest *req, uint8_t **out_data, size_t *out_bytes)
    {
        if(!req || !req->src || !out_data || !out_bytes)
            return TEX_ERR_PARAM;

        const TexPixelFormat *pf = lookup(req->target_format);

        if(!pf || !TexFormat_IsCompress(pf))
            return TEX_ERR_UNSUPPORTED;

        const int fmt_index = size_t(pf->format) - size_t(TEX_FMT_BC1RGB);

        if(fmt_index < 0 || fmt_index > 8)
            return TEX_ERR_UNSUPPORTED;

        const int pixel_bytes = pixel_bytes_of(req->src_pixel_type);

        if(pixel_bytes <= 0)
            return TEX_ERR_UNSUPPORTED;

        const CMP_FORMAT source_fmt = source_cmp_format(req->src_layout, req->src_pixel_type);

        if(source_fmt == CMP_FORMAT_Unknown)
            return TEX_ERR_UNSUPPORTED;

        // 3/4 通道都按 RGBA 取(布局已由核心归一)
        const int src_channels = (req->src_layout == TEX_LAYOUT_RGBA) ? 4
                               : (req->src_layout == TEX_LAYOUT_GrayAlpha) ? 2
                               : 1;

        CMP_Texture src_tex;
        memset(&src_tex, 0, sizeof(src_tex));

        src_tex.dwSize      = sizeof(CMP_Texture);
        src_tex.dwWidth     = req->width;
        src_tex.dwHeight    = req->height;
        src_tex.dwPitch     = req->width * pixel_bytes * src_channels;
        src_tex.format      = source_fmt;
        src_tex.dwDataSize  = src_tex.dwPitch * req->height;
        src_tex.pData       = (CMP_BYTE *)req->src;

        // 注意:不复刻 dwDataSize==0 的防护——旧版遇到 CMP_CalculateBufferSize 返回 0
        // 时照样"成功"并写出 0 字节载荷(容器补齐 8 字节),基线要求保持一致。

        CMP_Texture dst_tex;
        memset(&dst_tex, 0, sizeof(dst_tex));

        dst_tex.dwSize      = sizeof(CMP_Texture);
        dst_tex.dwWidth     = req->width;
        dst_tex.dwHeight    = req->height;
        dst_tex.format      = target_cmp_format(fmt_index);
        dst_tex.dwDataSize  = CMP_CalculateBufferSize(&dst_tex);
        dst_tex.pData       = (CMP_BYTE *)malloc(dst_tex.dwDataSize);

        if(!dst_tex.pData)
            return TEX_ERR_INTERNAL;

        CMP_CompressOptions options;
        memset(&options, 0, sizeof(options));

        options.dwSize      = sizeof(CMP_CompressOptions);
        options.fquality    = 1.0f;
        options.dwnumThreads = (dst_tex.format == CMP_FORMAT_BC4) ? 1
                             : (req->thread_count > 0 ? req->thread_count : 8);

        // GPU 加速:优先 DirectX Compute,失败自动回退 CPU 多线程
        options.nEncodeWith = CMP_GPU_DXC;

        CMP_ERROR cmp_result = CMP_ConvertTexture(&src_tex, &dst_tex, &options, nullptr);

        if(cmp_result != CMP_OK)
        {
            // GPU 路径失败(驱动不支持/显存不足/compute 不可用)→ 回退 CPU
            options.nEncodeWith = CMP_CPU;
            cmp_result = CMP_ConvertTexture(&src_tex, &dst_tex, &options, nullptr);
        }

        // 编码失败必须报错返回:否则 dst_tex 里是"已按目标格式申请、从未写入"的内存,落盘即垃圾
        if(cmp_result != CMP_OK)
        {
            free(dst_tex.pData);
            return TEX_ERR_ENCODE;
        }

        *out_data  = (uint8_t *)dst_tex.pData;
        *out_bytes = size_t(dst_tex.dwDataSize);
        return TEX_OK;
    }

    void AMD_FreeResult(uint8_t *data)
    {
        free(data);
    }

    TexEncoderProvider g_provider =
    {
        TEXENC_ABI_VERSION,
        "AMD Compressonator",
        "AMD",
        AMD_Init,
        AMD_Shutdown,
        AMD_QueryTargets,
        AMD_QuerySourceLayout,
        AMD_Encode,
        AMD_FreeResult,
    };
}//namespace

extern "C" TEXENC_EXPORT const TexEncoderProvider *TexGetEncoderProvider(void)
{
    return &g_provider;
}
