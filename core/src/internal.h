#pragma once
// TexConvCore 内部共享声明(不对外导出)。

#include "texconv/tex_core.h"
#include "texconv/tex_image.h"
#include "texconv/texenc_plugin.h"
#include "texconv/tex_formats.h"
#include "texconv/tex2d_container.h"

#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>

namespace texcore
{
    struct Provider
    {
        HMODULE                    module    = nullptr;
        const TexEncoderProvider  *provider  = nullptr;
        bool                       inited    = false;
        std::string                utf8_file;    // 插件 DLL 文件名(UTF-8,稳定存储)
    };

    /// 已加载的编码器插件列表(按文件名序,TexEncAMD 在前 → 默认后端=AMD,对齐旧版)
    std::vector<Provider> &Providers();

    /// 核心日志入口(TexCore_Init 时设置的回调)
    void CoreLog(int level, const std::string &utf8_line);

    // ---------------------------------------------------------------- 容器

    bool ContainerOpen(const std::wstring &path, FILE *&out);
    bool ContainerWriteHeader(FILE *f, int view_type);
    bool ContainerWriteSize2D(FILE *f, uint32_t width, uint32_t height);
    bool ContainerWriteFormatBlock(FILE *f, const TexPixelFormat *fmt, int mip_level);
    /// 写入一级数据;不足 8 字节补 0 对齐(对齐旧 TextureFileCreater::Write);
    /// 返回实际写入(含补齐)字节数,失败返回 0
    uint32_t ContainerWriteLevel(FILE *f, const void *data, uint32_t bytes);
    void ContainerClose(FILE *f);
    bool ContainerDelete(const std::wstring &path);

    // ---------------------------------------------------------------- 流水线

    int RunJobImpl(const TexJobParams *params, TexProgressFn progress, void *user);
    int RunCubeJobImpl(const TexCubeJobParams *params, TexProgressFn progress, void *user);

    // ── IBL Bake ─────────────────────────────────────────────────────────

    bool BakeDiffuseIrradiance(const std::vector<float> src[6], uint32_t src_w, uint32_t src_h,
                               std::vector<float> dst[6], uint32_t dst_w, uint32_t dst_h,
                               int sample_count);

    bool BakeGGXPrefilter(const std::vector<float> src[6], uint32_t src_w, uint32_t src_h,
                          std::vector<float> dst[6], uint32_t dst_w, uint32_t dst_h,
                          float roughness, int sample_count);

    /// 按名称选择编码后端(空=优先 Intel,AMD BC7 对部分内容有死锁/极慢问题);不存在返回 nullptr
    const TexEncoderProvider *SelectProvider(const char *short_name);

    /// 确保后端已初始化(懒加载,幂等)
    bool EnsureProviderInited(const TexEncoderProvider *provider);

    /// 计算 mip 级数(压缩格式 4x4 下限少 2 级,对齐旧版)
    int CalcMipLevels(uint32_t width, uint32_t height, bool is_compress, bool gen_mipmaps);

    /// 构建非压缩格式的一级像素数据(含 RGB565/A1RGB5 等位打包),迁自 pipeline 内部
    bool BuildUncompressedLevel(TexImage img, const TexPixelFormat *fmt,
                                int layout, int pixel_type,
                                std::vector<uint8_t> &payload);

    /// 构建压缩格式的一级块数据(经编码器插件)
    bool BuildCompressedLevel(TexImage img, const TexEncoderProvider *provider,
                              const TexPixelFormat *fmt,
                              int layout, int pixel_type,
                              std::vector<uint8_t> &payload);
}//namespace texcore
