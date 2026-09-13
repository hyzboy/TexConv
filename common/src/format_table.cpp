#include "texconv/tex_formats.h"
#include "texconv/tex_pixel.h"
#include <string.h>
#include <stdio.h>

namespace
{
    // 迁自原 pixel_format.cpp 的 pf_list;total_bits 对压缩格式沿用旧表的"每像素位"语义,
    // block_bytes 为新增字段(每 4x4 块字节数),供核心与编码器计算压缩后大小。
    constexpr TexPixelFormat pf_list[] =
    {
        {TEX_FMT_RGBA4,    "RGBA4",    4,{'R','G','B','A'},{ 4, 4, 4, 4}, 16,TEX_VBT_UNORM,  0},
        {TEX_FMT_BGRA4,    "BGRA4",    4,{'B','G','R','A'},{ 4, 4, 4, 4}, 16,TEX_VBT_UNORM,  0},
        {TEX_FMT_RGB565,   "RGB565",   3,{'R','G','B', 0 },{ 5, 6, 5, 0}, 16,TEX_VBT_UNORM,  0},
        {TEX_FMT_A1RGB5,   "A1RGB5",   4,{'A','R','G','B'},{ 1, 5, 5, 5}, 16,TEX_VBT_UNORM,  0},
        {TEX_FMT_R8,       "R8",       1,{'R', 0 , 0 , 0 },{ 8, 0, 0, 0},  8,TEX_VBT_UNORM,  0},
        {TEX_FMT_RG8,      "RG8",      2,{'R','G', 0 , 0 },{ 8, 8, 0, 0}, 16,TEX_VBT_UNORM,  0},
        {TEX_FMT_RGBA8,    "RGBA8",    4,{'R','G','B','A'},{ 8, 8, 8, 8}, 32,TEX_VBT_UNORM,  0},
        {TEX_FMT_RGBA8SN,  "RGBA8S",   4,{'R','G','B','A'},{ 8, 8, 8, 8}, 32,TEX_VBT_SNORM,  0},
        {TEX_FMT_RGBA8U,   "RGBA8U",   4,{'R','G','B','A'},{ 8, 8, 8, 8}, 32,TEX_VBT_UINT,   0},
        {TEX_FMT_RGBA8I,   "RGBA8I",   4,{'R','G','B','A'},{ 8, 8, 8, 8}, 32,TEX_VBT_SINT,   0},
        {TEX_FMT_ABGR8,    "ABGR8",    4,{'A','B','G','R'},{ 8, 8, 8, 8}, 32,TEX_VBT_UNORM,  0},
        {TEX_FMT_A2BGR10,  "A2BGR10",  4,{'A','B','G','R'},{ 2,10,10,10}, 32,TEX_VBT_UNORM,  0},
        {TEX_FMT_R16,      "R16",      1,{'R', 0 , 0 , 0 },{16, 0, 0, 0}, 16,TEX_VBT_UNORM,  0},
        {TEX_FMT_R16U,     "R16U",     1,{'R', 0 , 0 , 0 },{16, 0, 0, 0}, 16,TEX_VBT_UINT,   0},
        {TEX_FMT_R16I,     "R16I",     1,{'R', 0 , 0 , 0 },{16, 0, 0, 0}, 16,TEX_VBT_SINT,   0},
        {TEX_FMT_R16F,     "R16F",     1,{'R', 0 , 0 , 0 },{16, 0, 0, 0}, 16,TEX_VBT_SFLOAT, 0},
        {TEX_FMT_RG16,     "RG16",     2,{'R','G', 0 , 0 },{16,16, 0, 0}, 32,TEX_VBT_UNORM,  0},
        {TEX_FMT_RG16U,    "RG16U",    2,{'R','G', 0 , 0 },{16,16, 0, 0}, 32,TEX_VBT_UINT,   0},
        {TEX_FMT_RG16I,    "RG16I",    2,{'R','G', 0 , 0 },{16,16, 0, 0}, 32,TEX_VBT_SINT,   0},
        {TEX_FMT_RG16F,    "RG16F",    2,{'R','G', 0 , 0 },{16,16, 0, 0}, 32,TEX_VBT_SFLOAT, 0},
        {TEX_FMT_RGBA16,   "RGBA16",   4,{'R','G','B','A'},{16,16,16,16}, 64,TEX_VBT_UNORM,  0},
        {TEX_FMT_RGBA16SN, "RGBA16S",  4,{'R','G','B','A'},{16,16,16,16}, 64,TEX_VBT_SNORM,  0},
        {TEX_FMT_RGBA16U,  "RGBA16U",  4,{'R','G','B','A'},{16,16,16,16}, 64,TEX_VBT_UINT,   0},
        {TEX_FMT_RGBA16I,  "RGBA16I",  4,{'R','G','B','A'},{16,16,16,16}, 64,TEX_VBT_SINT,   0},
        {TEX_FMT_RGBA16F,  "RGBA16F",  4,{'R','G','B','A'},{16,16,16,16}, 64,TEX_VBT_SFLOAT, 0},
        {TEX_FMT_R32U,     "R32U",     1,{'R', 0 , 0 , 0 },{32, 0, 0, 0}, 32,TEX_VBT_UINT,   0},
        {TEX_FMT_R32I,     "R32I",     1,{'R', 0 , 0 , 0 },{32, 0, 0, 0}, 32,TEX_VBT_SINT,   0},
        {TEX_FMT_R32F,     "R32F",     1,{'R', 0 , 0 , 0 },{32, 0, 0, 0}, 32,TEX_VBT_SFLOAT, 0},
        {TEX_FMT_RG32U,    "RG32U",    2,{'R','G', 0 , 0 },{32,32, 0, 0}, 64,TEX_VBT_UINT,   0},
        {TEX_FMT_RG32I,    "RG32I",    2,{'R','G', 0 , 0 },{32,32, 0, 0}, 64,TEX_VBT_SINT,   0},
        {TEX_FMT_RG32F,    "RG32F",    2,{'R','G', 0 , 0 },{32,32, 0, 0}, 64,TEX_VBT_SFLOAT, 0},
        {TEX_FMT_RGB32U,   "RGB32U",   3,{'R','G','B', 0 },{32,32,32, 0}, 96,TEX_VBT_UINT,   0},
        {TEX_FMT_RGB32I,   "RGB32I",   3,{'R','G','B', 0 },{32,32,32, 0}, 96,TEX_VBT_SINT,   0},
        {TEX_FMT_RGB32F,   "RGB32F",   3,{'R','G','B', 0 },{32,32,32, 0}, 96,TEX_VBT_SFLOAT, 0},
        {TEX_FMT_RGBA32U,  "RGBA32U",  4,{'R','G','B','A'},{32,32,32,32},128,TEX_VBT_UINT,   0},
        {TEX_FMT_RGBA32I,  "RGBA32I",  4,{'R','G','B','A'},{32,32,32,32},128,TEX_VBT_SINT,   0},
        {TEX_FMT_RGBA32F,  "RGBA32F",  4,{'R','G','B','A'},{32,32,32,32},128,TEX_VBT_SFLOAT, 0},
        {TEX_FMT_B10GR11UF,"B10GR11UF",3,{'B','G','R', 0 },{10,11,11, 0}, 32,TEX_VBT_UFLOAT, 0},

        {TEX_FMT_COMPRESS, "COMPRESS", 0,{ 0 , 0 , 0 , 0 },{ 0, 0, 0, 0},  0,TEX_VBT_NONE,   0},

        {TEX_FMT_BC1RGB,   "BC1RGB",   0,{ 0 , 0 , 0 , 0 },{ 0, 0, 0, 0},  4,TEX_VBT_NONE,   8},
        {TEX_FMT_BC1RGBA,  "BC1RGBA",  0,{ 0 , 0 , 0 , 0 },{ 0, 0, 0, 0},  4,TEX_VBT_NONE,   8},
        {TEX_FMT_BC2,      "BC2",      0,{ 0 , 0 , 0 , 0 },{ 0, 0, 0, 0},  8,TEX_VBT_NONE,  16},
        {TEX_FMT_BC3,      "BC3",      0,{ 0 , 0 , 0 , 0 },{ 0, 0, 0, 0},  8,TEX_VBT_NONE,  16},
        {TEX_FMT_BC4,      "BC4",      0,{ 0 , 0 , 0 , 0 },{ 0, 0, 0, 0},  4,TEX_VBT_NONE,   8},
        {TEX_FMT_BC5,      "BC5",      0,{ 0 , 0 , 0 , 0 },{ 0, 0, 0, 0},  8,TEX_VBT_NONE,  16},
        {TEX_FMT_BC6H,     "BC6H",     0,{ 0 , 0 , 0 , 0 },{ 0, 0, 0, 0},  8,TEX_VBT_NONE,  16},
        {TEX_FMT_BC6H_SF,  "BC6H_SF",  0,{ 0 , 0 , 0 , 0 },{ 0, 0, 0, 0},  8,TEX_VBT_NONE,  16},
        {TEX_FMT_BC7,      "BC7",      0,{ 0 , 0 , 0 , 0 },{ 0, 0, 0, 0},  8,TEX_VBT_NONE,  16},
    };

    constexpr int PF_COUNT = int(sizeof(pf_list) / sizeof(TexPixelFormat));

    constexpr const char *vulkan_type_name[] =
    {
        "NONE",
        "UINT", "SINT", "UNORM", "SNORM",
        "USCALED", "SSCALED", "UFLOAT", "SFLOAT", "sRGB"
    };

    bool ascii_ieq(const char *a, const char *b)
    {
        if(!a || !b)return false;
        while(*a && *b)
        {
            char ca = *a, cb = *b;
            if(ca >= 'a' && ca <= 'z')ca -= 32;
            if(cb >= 'a' && cb <= 'z')cb -= 32;
            if(ca != cb)return false;
            ++a; ++b;
        }
        return *a == *b;
    }

    int to_pixel_type_via_bits(uint8_t bits, uint8_t vbt)
    {
        // 对齐原 ToImagePixelType:8/16/32 位 × (U/S)INT/(U/S)NORM/(U/S)FLOAT
        if(bits <= 8)
        {
            switch(vbt)
            {
                case TEX_VBT_UINT:
                case TEX_VBT_UNORM:  return TEX_PT_UInt8;
                case TEX_VBT_SINT:
                case TEX_VBT_SNORM:  return TEX_PT_Int8;
                default:             return -1;
            }
        }
        else if(bits <= 16)
        {
            switch(vbt)
            {
                case TEX_VBT_UINT:
                case TEX_VBT_UNORM:  return TEX_PT_UInt16;
                case TEX_VBT_SINT:
                case TEX_VBT_SNORM:  return TEX_PT_Int16;
                case TEX_VBT_SFLOAT: return TEX_PT_Float16;
                default:             return -1;
            }
        }
        else if(bits <= 32)
        {
            switch(vbt)
            {
                case TEX_VBT_UINT:
                case TEX_VBT_UNORM:  return TEX_PT_UInt32;
                case TEX_VBT_SINT:
                case TEX_VBT_SNORM:  return TEX_PT_Int32;
                case TEX_VBT_SFLOAT: return TEX_PT_Float32;
                default:             return -1;
            }
        }
        return -1;
    }
}//namespace

extern "C"
{
    const TexPixelFormat *TexFormat_Get(const char *name)
    {
        if(!name || !*name)return nullptr;

        for(int i = 0; i < PF_COUNT; i++)
            if(ascii_ieq(name, pf_list[i].name))
                return &pf_list[i];

        return nullptr;
    }

    const TexPixelFormat *TexFormat_GetByEnum(int fmt)
    {
        if(fmt <= TEX_FMT_NONE || fmt >= TEX_FMT_END)return nullptr;

        for(int i = 0; i < PF_COUNT; i++)
            if(pf_list[i].format == fmt)
                return &pf_list[i];

        return nullptr;
    }

    const TexPixelFormat *TexFormat_At(int index)
    {
        if(index < 0 || index >= PF_COUNT)return nullptr;
        return &pf_list[index];
    }

    int TexFormat_Count(void)
    {
        return PF_COUNT;
    }

    int TexFormat_PixelType(const TexPixelFormat *fmt)
    {
        if(!fmt)return -1;

        switch(fmt->format)
        {
            // 原 RGBA/RGB 创建器中的固定类型特例
            case TEX_FMT_RGBA4:
            case TEX_FMT_BGRA4:
            case TEX_FMT_A1RGB5:
            case TEX_FMT_RGB565:    return TEX_PT_UInt8;
            case TEX_FMT_A2BGR10:   return TEX_PT_UInt16;
            case TEX_FMT_B10GR11UF: return TEX_PT_Float16;

            default:
                if(fmt->format >= TEX_FMT_COMPRESS)
                    return -1;
                return to_pixel_type_via_bits(fmt->bits[0], fmt->vulkan_type);
        }
    }

    int TexFormat_IsAllowed(int source_channels, const TexPixelFormat *fmt)
    {
        if(!fmt)return 0;

        if(fmt->format > TEX_FMT_COMPRESS)      // 压缩格式:1-4 通道源均可
            return source_channels >= 1 && source_channels <= 4;

        if(source_channels < 1 || source_channels > 4)return 0;

        switch(source_channels)
        {
            case 1:
                switch(fmt->format)
                {
                    case TEX_FMT_R8:
                    case TEX_FMT_R16:
                    case TEX_FMT_R16U:
                    case TEX_FMT_R16I:
                    case TEX_FMT_R16F:
                    case TEX_FMT_R32U:
                    case TEX_FMT_R32I:
                    case TEX_FMT_R32F:  return TexFormat_PixelType(fmt) >= 0;
                    default:            return 0;
                }

            case 2:
                switch(fmt->format)
                {
                    case TEX_FMT_RG8:
                    case TEX_FMT_RG16:
                    case TEX_FMT_RG16U:
                    case TEX_FMT_RG16I:
                    case TEX_FMT_RG16F:
                    case TEX_FMT_RG32U:
                    case TEX_FMT_RG32I:
                    case TEX_FMT_RG32F: return TexFormat_PixelType(fmt) >= 0;
                    default:            return 0;
                }

            case 3:
                switch(fmt->format)
                {
                    case TEX_FMT_RGB32U:
                    case TEX_FMT_RGB32I:
                    case TEX_FMT_RGB32F:
                    case TEX_FMT_RGB565:
                    case TEX_FMT_B10GR11UF: return 1;
                    default:                return 0;
                }

            case 4:
                switch(fmt->format)
                {
                    case TEX_FMT_RGBA8:
                    case TEX_FMT_RGBA16:
                    case TEX_FMT_RGBA16U:
                    case TEX_FMT_RGBA16I:
                    case TEX_FMT_RGBA16F:
                    case TEX_FMT_RGBA32U:
                    case TEX_FMT_RGBA32I:
                    case TEX_FMT_RGBA32F:
                    case TEX_FMT_RGBA4:
                    case TEX_FMT_BGRA4:
                    case TEX_FMT_A1RGB5:
                    case TEX_FMT_A2BGR10:   return 1;
                    default:                return 0;
                }
        }

        return 0;
    }

    void TexFormat_PrintList(void (*emit)(void *user, const char *line), void *user)
    {
        char buf[64];

        auto output = [&](const char *s)
        {
            if(emit)emit(user, s);
        };

        for(int i = 0; i < PF_COUNT; i++)
        {
            const TexPixelFormat *pf = pf_list + i;

            if(pf->format < TEX_FMT_COMPRESS)
            {
                snprintf(buf, sizeof(buf), "%d: %10s %3d bits %s",
                         int(pf->channels), pf->name, int(pf->total_bits),
                         vulkan_type_name[pf->vulkan_type]);
            }
            else if(pf->format > TEX_FMT_COMPRESS)
            {
                snprintf(buf, sizeof(buf), "%13s Compress Format", pf->name);
            }
            else
            {
                output("");
                continue;
            }

            output(buf);
        }
    }
}//extern "C"
