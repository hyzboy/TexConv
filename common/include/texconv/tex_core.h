#pragma once
// TexConvCore.dll —— 编排核心(流水线 / .Tex2D 容器 / 编码器插件管理)C API。
// CLI 与 Qt GUI 均只通过本头文件使用核心,不直接接触 ImageMagick / 编码器。

#include <stdint.h>
#include <wchar.h>
#include "texconv_api.h"
#include "tex_result.h"
#include "tex_pixel.h"
#include "tex_callback.h"
#include "tex_formats.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TexCoreCallbacks
{
    TexLogFn    log;        // 可为 NULL;编码器/流水线/图像库的日志统一走这里
    void       *user;
} TexCoreCallbacks;

typedef struct TexProviderInfo
{
    const char *name;           // 显示名
    const char *short_name;     // 选择用名("AMD"/"Intel")
    const char *module_file;    // DLL 文件名(不含路径)
} TexProviderInfo;

/// 一次转换任务的完整参数。
/// 字符串指针须在 TexCore_RunJob 返回前保持有效。
typedef struct TexJobParams
{
    const wchar_t *input_path;      ///< 必填:单文件或目录
    const wchar_t *output_path;     ///< 可为 NULL;NULL=与输入同目录同名换 .Tex2D 后缀
                                    ///< (仅 input 为文件时有效;相对路径按进程 CWD 解析)

    const char    *target_format;   ///< 非 NULL:显式目标格式(逐文件配置,GUI 用),
                                      ///< 且必须通过 TexFormat_IsAllowed(channels,fmt) 校验;
                                      ///< NULL:按源图通道数取 slot_format 槽位
    const char    *slot_format[4];  ///< 槽位模式:1/2/3/4 通道源各自的目标格式名;
                                      ///< NULL 项 = 默认 BC4/BC5/BC7/BC7

    const char    *provider;        ///< 编码后端 short_name;NULL=默认第一个可用(旧版=AMD)

    int            gen_mipmaps;     ///< 生成 mipmap(压缩格式 4x4 下限,少 2 级)
    int            force_grayscale;
    int            discard_alpha;
    int            normal_map;      ///< 法线贴图模式:所有槽位强制 BC5(等价旧 /normal)

    int            use_color_key;
    uint8_t        color_key[3];    ///< RGB,use_color_key 时生效

    int            df_mode;         ///< 距离场模式:单通道源对灰度生成;
                                    ///< 带 Alpha 的源(RGBA/GrayAlpha)对 Alpha 生成;
                                    ///< 生成后图像替换为单通道 8-bit 距离场,
                                    ///  后续按 1 通道源继续(默认槽位 R8 —— 避开 AMD
                                    ///  的 BC4 灰度源已知缺陷;显式给 slot_format[0] 可覆盖)
    int            df_threshold;    ///< 距离场内外判定阈值(0-255,<=0 取默认 128)
} TexJobParams;

/// 快速读取源图元数据(不产生转换)。成功后 channels 可用于 GUI 预过滤格式。
typedef struct TexImageInfo
{
    uint32_t    width;
    uint32_t    height;
    uint32_t    depth;
    int         layout;             ///< TexImageLayout
    int         pixel_type;         ///< TexPixelType
    int         channels;           ///< 1-4
    int         has_alpha;
} TexImageInfo;

/// .Tex2D 产物读回信息(供 GUI 校验/预览)
typedef struct Tex2DInfo
{
    uint32_t    width;
    uint32_t    height;
    int         view_type;          ///< Tex2DViewType
    char        format_name[10];    ///< 压缩:格式名;非压缩:组装出的可读名
    int         is_compress;
    int         channels;           ///< 非压缩有效
    uint8_t     bits[4];            ///< 非压缩有效
    int         vulkan_type;        ///< 非压缩有效,TexVulkanBaseType
    uint8_t     mip_levels;
    int64_t     file_size;
} Tex2DInfo;

/// 初始化核心:加载自身目录下的 texenc_*.dll 编码器插件,初始化图像库。
TEXCONV_API int  TexCore_Init(const TexCoreCallbacks *cb);

TEXCONV_API void TexCore_Shutdown(void);

/// 枚举可用编码后端。返回个数;out_count 不足时仅写满 out_count 个。
TEXCONV_API int  TexCore_EnumProviders(TexProviderInfo *out, int out_count);

/// 读取源图元数据(快速,失败返回错误码)。
TEXCONV_API int  TexCore_ProbeImage(const wchar_t *path, TexImageInfo *out);

/// 枚举目录下的图像文件(跳过 .Tex2D;recursive 非 0 时含子目录)。
/// cb 对每个文件调用一次,返回非 0 停止枚举;返回实际交给 cb 的文件数。
TEXCONV_API int  TexCore_EnumDirectory(const wchar_t *dir, int recursive,
                                       int (*cb)(void *user, const wchar_t *path),
                                       void *user);

/// 同步执行一次转换(阻塞至完成;线程化由外壳负责)。
/// 进度通过 progress 回调汇报(逐 mip 级);回调非 0 取消并删除半成品。
/// 返回 TEX_OK 或错误码;失败时不保证没有残留文件(与旧版行为一致)。
TEXCONV_API int  TexCore_RunJob(const TexJobParams *params, TexProgressFn progress, void *user);

/// Cubemap 转换:6 张面图合并为一个 .TexCube 文件。
/// 文件内布局:mip-major(每级 6 面连续,面序 +X,-X,+Y,-Y,+Z,-Z,即 Vulkan 层 0..5),
/// 每面数据不足 8 字节补 0 —— 与引擎 CommitTextureCubeMipmaps 的读取步进一致。
typedef struct TexCubeJobParams
{
    const wchar_t *face_paths[6];   ///< 必填,顺序固定:+X,-X,+Y,-Y,+Z,-Z
    const wchar_t *output_path;     ///< 必填:.TexCube 输出路径(建议显式给出完整路径)

    const char    *target_format;   ///< NULL = 按面通道数取默认槽位(BC4/BC5/BC7/BC7)
    const char    *provider;        ///< NULL = 默认(优先 AMD)
    int            gen_mipmaps;     ///< 每面独立生成 mip 链(压缩格式 4x4 下限规则同 2D)
} TexCubeJobParams;

TEXCONV_API int  TexCore_RunCubeJob(const TexCubeJobParams *params, TexProgressFn progress, void *user);

/// 读取 .Tex2D 产物信息。文件不存在/头部非法返回 TEX_ERR_IO。
TEXCONV_API int  TexCore_ReadInfo(const wchar_t *tex2d_path, Tex2DInfo *out);

#ifdef __cplusplus
}
#endif
