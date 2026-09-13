#pragma once
// TexImage.dll —— 图像核心(ImageMagick++ 加载/预处理)C API。
//
// 句柄不跨线程共享:同一 TexImage 句柄的所有操作必须在同一线程串行调用;
// 不同句柄可在不同线程并发使用。
// 所有路径为 UTF-16(Windows wchar_t);数据缓冲由调用方提供,模块不向调用方借出内存。

#include <stdint.h>
#include <stddef.h>
#include "texconv_api.h"
#include "tex_result.h"
#include "tex_pixel.h"
#include "tex_callback.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TexImage__ *TexImage;

typedef struct TexImageCallbacks
{
    TexLogFn    log;        // 可为 NULL
    void       *user;       // 透传给 log
} TexImageCallbacks;

/// 初始化图像库(Magick::InitializeMagick + 日志路由)。进程内只需成功调用一次。
/// 重复调用返回 TEX_OK 且不重复初始化。
TEXCONV_API int  TexImage_Init(const TexImageCallbacks *cb);

/// 关闭图像库。之后不可再调用其它 TexImage 接口。
TEXCONV_API void TexImage_Shutdown(void);

/// 加载图像文件。成功后 *out 需以 TexImage_Free 释放。
TEXCONV_API int  TexImage_Load(TexImage *out, const wchar_t *path);

TEXCONV_API void TexImage_Free(TexImage img);

/// 读取当前图像信息。任一输出指针可为 NULL。
TEXCONV_API void TexImage_GetInfo(TexImage img,
                                  uint32_t *width, uint32_t *height, uint32_t *depth,
                                  int *layout, int *pixel_type);

/// 当前图像是否带 alpha 通道(无论 layout)
TEXCONV_API int  TexImage_HasAlpha(TexImage img);

/// 缩放图像(内部使用 Lanczos,与旧版 Resize 行为一致)
TEXCONV_API int  TexImage_Resize(TexImage img, uint32_t width, uint32_t height);

/// 持久转换通道布局与像素类型(等价旧版 ConvertToGray/RG/RGB/RGBA/R)
/// layout=TEX_LAYOUT_Alpha 时等价旧版 ConvertToR。
TEXCONV_API int  TexImage_Convert(TexImage img, int layout, int pixel_type);

/// 取指定布局/像素类型数据所需的缓冲字节数;组合非法返回 0。
/// 按当前图像尺寸计算,调用方应在 Resize/Convert 之后重新查询。
TEXCONV_API size_t TexImage_GetBufferSize(TexImage img, int layout, int pixel_type);

/// 提取像素数据到调用方缓冲(不改变图像自身状态)。
/// buf_size 必须不小于 TexImage_GetBufferSize 的返回值。
/// layout=TEX_LAYOUT_Alpha 等价旧版 GetAlpha,Gray 等价 GetGray,以此类推。
TEXCONV_API int  TexImage_GetData(TexImage img, void *buf, size_t buf_size,
                                  int layout, int pixel_type);

/// 由原始像素数据创建图像句柄(等价 TexImage_Load 的"内存版")。
/// 数据布局与 TexImage_GetData 输出一致:width*height*channels 个像素,
/// 每像素按 pixel_type 大小连续存放,首行为顶部行。
/// 成功后 *out 需以 TexImage_Free 释放。
TEXCONV_API int  TexImage_CreateFromData(TexImage *out,
                                         uint32_t width, uint32_t height,
                                         int channels, int pixel_type,
                                         const void *data);

/// 保存像素数据为图像文件(迁自旧版 SaveImageToFile)。
/// scale 为输出前对数据的重采样比例(与旧版语义一致,1.0f=不缩放)。
TEXCONV_API int  TexImage_SaveImage(const wchar_t *path,
                                    uint32_t width, uint32_t height, float scale,
                                    int channels, int pixel_type, const void *data);

#ifdef __cplusplus
}
#endif
