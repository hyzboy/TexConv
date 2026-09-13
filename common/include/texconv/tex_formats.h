#pragma once
// 目标像素格式表(迁自原 pixel_format.h/cpp,去 hgl 依赖)。
//
// TexColorFormat 的枚举值序与 hgl 的 ColorFormat 保持一致:
//   1) AMD 编码器内部用 (format - TEX_FMT_BC1RGB) 做索引;
//   2) .Tex2D 头部非压缩格式块会写入 vulkan_type 等字段,值必须与旧版一致。
// 不得重排、不得在中间插入新值(新格式只能追加到 END 之前)。

#include <stdint.h>
#include "texconv_api.h"

#ifdef __cplusplus
extern "C" {
#endif

enum TexColorFormat
{
    TEX_FMT_NONE = 0,

    TEX_FMT_RGBA4,
    TEX_FMT_BGRA4,
    TEX_FMT_RGB565,
    TEX_FMT_A1RGB5,
    TEX_FMT_R8,
    TEX_FMT_RG8,
    TEX_FMT_RGBA8,
    TEX_FMT_RGBA8SN,
    TEX_FMT_RGBA8U,
    TEX_FMT_RGBA8I,
    TEX_FMT_ABGR8,
    TEX_FMT_A2BGR10,
    TEX_FMT_R16,
    TEX_FMT_R16U,
    TEX_FMT_R16I,
    TEX_FMT_R16F,
    TEX_FMT_RG16,
    TEX_FMT_RG16U,
    TEX_FMT_RG16I,
    TEX_FMT_RG16F,
    TEX_FMT_RGBA16,
    TEX_FMT_RGBA16SN,
    TEX_FMT_RGBA16U,
    TEX_FMT_RGBA16I,
    TEX_FMT_RGBA16F,
    TEX_FMT_R32U,
    TEX_FMT_R32I,
    TEX_FMT_R32F,
    TEX_FMT_RG32U,
    TEX_FMT_RG32I,
    TEX_FMT_RG32F,
    TEX_FMT_RGB32U,
    TEX_FMT_RGB32I,
    TEX_FMT_RGB32F,
    TEX_FMT_RGBA32U,
    TEX_FMT_RGBA32I,
    TEX_FMT_RGBA32F,
    TEX_FMT_B10GR11UF,

    TEX_FMT_COMPRESS,           // 分界标记:值小于它=非压缩,大于它=块压缩

    TEX_FMT_BC1RGB,
    TEX_FMT_BC1RGBA,
    TEX_FMT_BC2,
    TEX_FMT_BC3,
    TEX_FMT_BC4,
    TEX_FMT_BC5,
    TEX_FMT_BC6H,
    TEX_FMT_BC6H_SF,
    TEX_FMT_BC7,

    TEX_FMT_END
};

// 对齐 hgl VulkanBaseType,写入 .Tex2D 头部
enum TexVulkanBaseType
{
    TEX_VBT_NONE    = 0,
    TEX_VBT_UINT    = 1,
    TEX_VBT_SINT    = 2,
    TEX_VBT_UNORM   = 3,
    TEX_VBT_SNORM   = 4,
    TEX_VBT_USCALED = 5,
    TEX_VBT_SSCALED = 6,
    TEX_VBT_UFLOAT  = 7,
    TEX_VBT_SFLOAT  = 8,
    TEX_VBT_SRGB    = 9,
};

typedef struct TexPixelFormat
{
    TexColorFormat   format;
    const char      *name;          // ASCII,最长 9 字节(.Tex2D 头部按 9 字节定长写)
    uint8_t          channels;      // 格式自身通道数;压缩格式为 0
    char             color[4];      // 通道标记,如 'R','G','B','A';压缩格式全 0
    uint8_t          bits[4];       // 各通道位数;压缩格式全 0
    uint32_t         total_bits;    // 每像素总位数;压缩格式为每块字节数*8/16 语义见旧表
    uint8_t          vulkan_type;   // TexVulkanBaseType
    uint16_t         block_bytes;   // 压缩格式每 4x4 块字节数(BC1/BC4=8,其余=16);非压缩为 0
} TexPixelFormat;

/// 按名称查找(大小写不敏感);未找到返回 NULL
const TexPixelFormat *TexFormat_Get(const char *name);

/// 按枚举查找;越界返回 NULL
const TexPixelFormat *TexFormat_GetByEnum(int fmt);

/// 遍历:返回表中第 index 项,越界返回 NULL
const TexPixelFormat *TexFormat_At(int index);

int TexFormat_Count(void);

inline int TexFormat_IsCompress(const TexPixelFormat *pf)
{
    return pf && pf->format > TEX_FMT_COMPRESS;
}

/// "源图通道数 × 目标格式"合法性矩阵(编码自原 TextureFileCreaterR/RG/RGB/RGBA 的校验):
///   压缩格式对 1-4 通道源均可用;
///   1 通道源:R8/R16/R16U/R16I/R16F/R32U/R32I/R32F
///   2 通道源:RG8/RG16/RG16U/RG16I/RG16F/RG32U/RG32I/RG32F
///   3 通道源:仅 RGB32U/RGB32I/RGB32F/RGB565/B10GR11UF
///   4 通道源:RGBA8/RGBA16/RGBA16U/RGBA16I/RGBA16F/RGBA32U/RGBA32I/RGBA32F/
///             RGBA4/BGRA4/A1RGB5/A2BGR10
int TexFormat_IsAllowed(int source_channels, const TexPixelFormat *fmt);

/// 非压缩格式在核心流水线中使用的像素类型(即原 ToImagePixelType 的映射结果);
/// 返回 -1 表示该组合无法转换。
int TexFormat_PixelType(const TexPixelFormat *fmt);

/// 输出格式清单(供 CLI PrintFormatList / GUI 填下拉框)。
/// emit 逐行回调,UTF-8;emit 为 NULL 时仅做遍历。
void TexFormat_PrintList(void (*emit)(void *user, const char *line), void *user);

#ifdef __cplusplus
}
#endif
