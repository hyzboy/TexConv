#pragma once
// 块压缩编码器插件接口。
//
// 每个编码器 DLL(TexEncAMD.dll / TexEncIntel.dll / ...)唯一导出:
//     TexEncoderProvider *TexGetEncoderProvider(void);
// TexConvCore 运行时按 "texenc*.dll" 模式扫描自身目录并加载,不做链接期依赖。
//
// ABI 规则:
//   * 全 C ABI;插件内部不得使用宿主的 CRT 堆与宿主交换内存,
//     Encode 结果由插件自己的分配器分配,调用方用 FreeResult 释放。
//   * "源数据要求什么布局/像素类型"由编码器自己声明(QuerySourceLayout),
//     归一由核心用 TexImage 完成;这样不同后端(如 ISPC 的 BC6H 需要半精度源)
//     可以声明不同需求而不必改动核心。

#include <stdint.h>
#include <stddef.h>
#include "texconv_api.h"
#include "tex_result.h"
#include "tex_pixel.h"
#include "tex_callback.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TEXENC_ABI_VERSION  1

typedef struct TexEncodeRequest
{
    const uint8_t  *src;            // 源像素数据(布局/类型见下)
    uint32_t        width;
    uint32_t        height;
    int             src_layout;     // TexImageLayout,编码器经 QuerySourceLayout 声明的布局
    int             src_pixel_type; // TexPixelType
    const char     *target_format;  // 目标格式名,如 "BC7"(与 tex_formats.h 名称一致)
    int             quality;        // 0-100,现有实现固定 100
    int             thread_count;   // <=0 由插件自定(沿用旧版策略:BC4 单线程,其余 8 线程)
    TexProgressFn   progress;       // 可为 NULL;按 mip 级内部由核心汇报,插件内可按行进度汇报
    void           *user;
} TexEncodeRequest;

typedef struct TexEncoderProvider
{
    int          abi_version;       // 必须 = TEXENC_ABI_VERSION

    const char  *name;              // 显示名,如 "AMD Compressonator" / "Intel ISPC"
    const char  *short_name;        // 命令行/配置用名,如 "AMD" / "Intel"

    int        (*Init)(void);       // 成功返回 TEX_OK;Core 加载后、首次编码前调用
    void       (*Shutdown)(void);

    /// 枚举支持的目标格式名(与 TexFormat_Get 兼容的名字)。
    /// 返回写入的个数;names_count 不足时返回 -1。
    int        (*QueryTargets)(const char **names, int names_count);

    /// 声明对指定目标格式的源数据要求。
    /// 输入:target_format + 源图通道数/像素类型;输出:要求的布局与像素类型。
    /// out_pixel_type 可与 src_pixel_type 相同(表示无需像素类型转换)。
    /// 目标格式不支持返回 TEX_ERR_UNSUPPORTED。
    int        (*QuerySourceLayout)(const char *target_format,
                                   int src_channels, int src_pixel_type,
                                   int *out_layout, int *out_pixel_type);

    /// 编码。成功返回 TEX_OK 并写出 *out_data/*out_bytes(4x4 块序列)。
    int        (*Encode)(const TexEncodeRequest *req, uint8_t **out_data, size_t *out_bytes);

    void       (*FreeResult)(uint8_t *data);
} TexEncoderProvider;

#if defined(_WIN32)
    #define TEXENC_EXPORT __declspec(dllexport)
#else
    #define TEXENC_EXPORT __attribute__((visibility("default")))
#endif

/// LoadLibrary 后 GetProcAddress("TexGetEncoderProvider") 的函数指针类型
typedef const TexEncoderProvider *(*TexGetEncoderProviderFn)(void);

#ifdef __cplusplus
}
#endif
