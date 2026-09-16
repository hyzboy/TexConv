// TexConvCore —— Cubemap 转换流水线。
// 6 张面图合并为一个 .TexCube:
//   * 文件头与 .Tex2D 相同,type = CUBE(VkImageViewType 3)
//   * 数据布局 mip-major:每级 6 面连续,面序 +X,-X,+Y,-Y,+Z,-Z(Vulkan 层 0..5),
//     每面数据不足 8 字节补 0 —— 与引擎 CommitTextureCubeMipmaps 的读取步进一致
//   * 6 面须尺寸/像素类型一致;每面独立缩放生成 mip 链

#include "internal.h"
#include "texconv/tex_distance_field.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

namespace texcore
{
    static const char *cube_default_slot_name[4] = {"BC4", "BC5", "BC7", "BC7"};

    namespace
    {
        /// 面数据的一级负载:压缩走编码器,非压缩走位打包/直取
        bool BuildFaceLevel(TexImage face, bool is_compress,
                            const TexPixelFormat *fmt, const TexEncoderProvider *provider,
                            int layout, int pt,
                            std::vector<uint8_t> &payload)
        {
            if(is_compress)
                return BuildCompressedLevel(face, provider, fmt, layout, pt, payload);

            return BuildUncompressedLevel(face, fmt, layout, pt, payload);
        }
    }//namespace

    namespace
    {
        /// 编码器是否支持指定目标格式名
        bool ProviderSupportsTarget(const TexEncoderProvider *provider, const char *name)
        {
            if(!provider || !provider->QueryTargets || !name)
                return false;

            const int n = provider->QueryTargets(nullptr, 0);

            if(n <= 0)
                return false;

            std::vector<const char *> names{ size_t(n) };

            if(provider->QueryTargets(names.data(), n) != n)
                return false;

            for(int i = 0; i < n; i++)
                if(names[size_t(i)] && strcmp(names[size_t(i)], name) == 0)
                    return true;

            return false;
        }
    }//namespace

    // 前向声明
    void WriteIBLCubeFile(const std::filesystem::path &base_path,
                          const wchar_t *suffix,
                          const std::vector<float> src[6],
                          uint32_t src_w, uint32_t src_h,
                          bool (*bake_fn)(const std::vector<float>*, uint32_t, uint32_t,
                                          std::vector<float>*, uint32_t, uint32_t),
                          const TexEncoderProvider *provider,
                          const char *ibl_format,
                          TexProgressFn progress, void *user);

    int RunCubeJobImpl(const TexCubeJobParams *params, TexProgressFn progress, void *user)
    {
        if(!params)
            return TEX_ERR_PARAM;

        for(int i = 0; i < 6; i++)
            if(!params->face_paths[i] || !*params->face_paths[i])
                return TEX_ERR_PARAM;

        if(!params->output_path || !*params->output_path)
            return TEX_ERR_PARAM;

        // 1. 压缩后端
        const TexEncoderProvider *provider = SelectProvider(params->provider);

        if(!provider)
        {
            CoreLog(TEX_LOG_ERROR, "texture compression provider is not available: "
                                   + std::string(params->provider ? params->provider : "(any)"));
            return TEX_ERR_NO_PROVIDER;
        }

        if(!EnsureProviderInited(provider))
        {
            CoreLog(TEX_LOG_ERROR, "texture compression provider init failed.");
            return TEX_ERR_NO_PROVIDER;
        }

        // 2. 加载 6 面,校验一致性
        TexImage faces[6] = {};
        uint32_t width = 0, height = 0;
        int layout = 0, pt = 0;

        for(int i = 0; i < 6; i++)
        {
            int rc = TexImage_Load(&faces[i], params->face_paths[i]);

            if(rc != TEX_OK)
            {
                CoreLog(TEX_LOG_ERROR, "load cube face " + std::to_string(i) + " failed.");
                for(int j = 0; j < i; j++)TexImage_Free(faces[j]);
                return rc;
            }

            uint32_t fw = 0, fh = 0;
            int fl = 0, fpt = 0;
            TexImage_GetInfo(faces[i], &fw, &fh, nullptr, &fl, &fpt);

            if(i == 0)
            {
                width = fw; height = fh; layout = fl; pt = fpt;
            }
            else if(fw != width || fh != height || fl != layout || fpt != pt)
            {
                CoreLog(TEX_LOG_ERROR, "cube face " + std::to_string(i)
                                       + " mismatch with face 0 (size/channel/type).");
                for(int j = 0; j <= i; j++)TexImage_Free(faces[j]);
                return TEX_ERR_UNSUPPORTED;
            }
        }

        const int channels = TexLayoutChannels(layout);

        if(channels <= 0 || channels > 4)
        {
            CoreLog(TEX_LOG_ERROR, "image format don't support ");
            for(int i = 0; i < 6; i++)TexImage_Free(faces[i]);
            return TEX_ERR_UNSUPPORTED;
        }

        // 5. Y-up(OpenGL/D3D cubemap 约定)→ Z-up(引擎世界)的 90° 旋转烘焙:
        //    加载时扭正,后续压缩/写出不再关心方向。90° 整倍数的面间映射为
        //    精确的像素一一对应(最近邻无损)。
        {
            const int bpp = channels * TexPixelTypeBytes(pt);

            std::vector<std::vector<uint8_t>> src(6);
            for(int i = 0; i < 6; i++)
            {
                src[i].resize(TexImage_GetBufferSize(faces[i], layout, pt));

                if(TexImage_GetData(faces[i], src[i].data(), src[i].size(), layout, pt) != TEX_OK)
                {
                    CoreLog(TEX_LOG_ERROR, "cube face data fetch failed.");
                    for(int j = 0; j < 6; j++)TexImage_Free(faces[j]);
                    return TEX_ERR_INTERNAL;
                }
            }

            // 每面的 (法线, s+方向, t+方向),与 OpenGL/Vulkan cubemap 约定一致
            static constexpr float face_axes[6][3][3] =
            {
                {{  1, 0, 0},{ 0, 0,-1},{ 0,-1, 0}},    // +X
                {{ -1, 0, 0},{ 0, 0, 1},{ 0,-1, 0}},    // -X
                {{  0, 1, 0},{ 1, 0, 0},{ 0, 0, 1}},    // +Y
                {{  0,-1, 0},{ 1, 0, 0},{ 0, 0,-1}},    // -Y
                {{  0, 0, 1},{ 1, 0, 0},{ 0,-1, 0}},    // +Z
                {{  0, 0,-1},{-1, 0, 0},{ 0,-1, 0}},    // -Z
            };

            std::vector<std::vector<uint8_t>> rotated(6);

            for(int dst = 0; dst < 6; dst++)
            {
                rotated[dst].resize(src[0].size());

                const auto &axes = face_axes[dst];

                for(uint32_t py = 0; py < height; py++)
                for(uint32_t px = 0; px < width;  px++)
                {
                    // 该像素在世界空间(Z-up)的方向
                    const float sc = (float(px) + 0.5f) / width  * 2.0f - 1.0f;
                    const float tc = (float(py) + 0.5f) / height * 2.0f - 1.0f;

                    const float dw[3] =
                    {
                        axes[0][0] + sc * axes[1][0] + tc * axes[2][0],
                        axes[0][1] + sc * axes[1][1] + tc * axes[2][1],
                        axes[0][2] + sc * axes[1][2] + tc * axes[2][2],
                    };

                    // 世界(Z-up)→ 源(Y-up):(x,y,z) → (x,z,-y)
                    // 加上绕 Z 轴的 90° 补偿旋转(x,y)→(y,-x):
                    const float wx = -dw[1], wy = dw[0];
                    const float ds[3] = { wx, dw[2], -wy };

                    // 主轴 → 源面与面内 (s,t)
                    int face; float s_src, t_src;

                    const float ax[3] = { std::fabs(ds[0]), std::fabs(ds[1]), std::fabs(ds[2]) };

                    if(ax[0] >= ax[1] && ax[0] >= ax[2])
                    {
                        face = ds[0] > 0 ? 0 : 1;              // ±X: s=∓z, t=-y
                        s_src = (ds[0] > 0) ? -ds[2] : ds[2];
                        t_src = -ds[1];
                    }
                    else if(ax[1] >= ax[2])
                    {
                        face = ds[1] > 0 ? 2 : 3;              // ±Y: s=x, t=±z
                        s_src = ds[0];
                        t_src = (ds[1] > 0) ? ds[2] : -ds[2];
                    }
                    else
                    {
                        face = ds[2] > 0 ? 4 : 5;              // ±Z: s=±x, t=-y
                        s_src = (ds[2] > 0) ? ds[0] : -ds[0];
                        t_src = -ds[1];
                    }

                    uint32_t sx = (std::min)(uint32_t((s_src + 1.0f) * 0.5f * width),  width - 1);
                    uint32_t sy = (std::min)(uint32_t((t_src + 1.0f) * 0.5f * height), height - 1);

                    memcpy(&rotated[dst][(size_t(py) * width + px) * bpp],
                           &src[face][(size_t(sy) * width + sx) * bpp],
                           bpp);
                }
            }

            // 用旋转后的面替换(原生类型/通道不变)
            TexImage replaced_handle[6] = {};
            for(int i = 0; i < 6; i++)
            {
                TexImage replaced = nullptr;

                if(TexImage_CreateFromData(&replaced, width, height, channels, pt,
                                           rotated[i].data()) != TEX_OK)
                {
                    CoreLog(TEX_LOG_ERROR, "cube rotation bake failed.");
                    for(int j = 0; j < 6; j++)TexImage_Free(faces[j]);
                    for(int j = 0; j <= i; j++)TexImage_Free(replaced_handle[j]);
                    return TEX_ERR_INTERNAL;
                }

                replaced_handle[i] = replaced;
                TexImage_Free(faces[i]);
                faces[i] = replaced;
            }

            // 旋转后重读布局/像素类型(RGBA8 化后可能与源不同)
            TexImage_GetInfo(faces[0], &width, &height, nullptr, &layout, &pt);
        }

        // 3. 目标格式(显式指定或按面通道数取默认槽位)
        const TexPixelFormat *fmt = nullptr;

        if(params->target_format)
        {
            fmt = TexFormat_Get(params->target_format);

            if(!fmt)
            {
                CoreLog(TEX_LOG_ERROR, std::string("[FORMAT ERROR] Don't support ")
                                       + params->target_format + " format.");
                for(int i = 0; i < 6; i++)TexImage_Free(faces[i]);
                return TEX_ERR_UNSUPPORTED;
            }
        }
        else
        {
            fmt = TexFormat_Get(cube_default_slot_name[channels - 1]);
        }

        const bool is_compress = TexFormat_IsCompress(fmt);

        // 4. mip 级数(压缩格式 4x4 下限规则同 2D)
        const int miplevel = CalcMipLevels(width, height, is_compress,
                                           params->gen_mipmaps != 0);

        // 5. 面数据的布局/类型准备(在文件创建前完成,失败无文件残留)
        int data_layout = layout;
        int data_pt     = pt;

        if(is_compress)
        {
            const int rc = provider->QuerySourceLayout(fmt->name, channels, pt,
                                                       &data_layout, &data_pt);

            if(rc != TEX_OK)
            {
                for(int i = 0; i < 6; i++)TexImage_Free(faces[i]);
                return rc == TEX_ERR_UNSUPPORTED ? TEX_ERR_UNSUPPORTED : rc;
            }
        }
        else
        {
            // 非压缩:"源通道数 × 目标格式"合法性矩阵 + 目标像素类型
            if(!TexFormat_IsAllowed(channels, fmt))
            {
                CoreLog(TEX_LOG_ERROR, std::string("Don't support this format: ") + fmt->name);
                for(int i = 0; i < 6; i++)TexImage_Free(faces[i]);
                return TEX_ERR_UNSUPPORTED;
            }

            data_pt = TexFormat_PixelType(fmt);

            if(data_pt < 0)
            {
                CoreLog(TEX_LOG_ERROR, std::string("Don't support this format: ") + fmt->name);
                for(int i = 0; i < 6; i++)TexImage_Free(faces[i]);
                return TEX_ERR_UNSUPPORTED;
            }
        }

        // 面转换到所需布局/类型(压缩:编码器要求;非压缩:按面通道数)
        if(is_compress)
        {
            for(int i = 0; i < 6; i++)
                if(data_layout != layout || data_pt != pt)
                    TexImage_Convert(faces[i], data_layout, data_pt);
        }
        else
        {
            static const int layout_by_channel[4] =
            {
                TEX_LAYOUT_Alpha, TEX_LAYOUT_GrayAlpha, TEX_LAYOUT_RGB, TEX_LAYOUT_RGBA
            };

            for(int i = 0; i < 6; i++)
            {
                if(TexImage_Convert(faces[i], layout_by_channel[channels - 1], data_pt) != TEX_OK)
                {
                    CoreLog(TEX_LOG_ERROR, std::string("convert face ") + std::to_string(i)
                                           + " failed: " + fmt->name);
                    for(int j = 0; j < 6; j++)TexImage_Free(faces[j]);
                    return TEX_ERR_UNSUPPORTED;
                }
            }

            // 以面 0 的实际布局为准(通道转换可能改变布局,如 Alpha→Gray)
            TexImage_GetInfo(faces[0], nullptr, nullptr, nullptr, &data_layout, nullptr);
        }

        // 5.5 IBL:提取 RGBA float32 源数据(主转换的 mip 循环会缩放 faces,
        //     所以必须在循环前取走原始分辨率数据)
        std::vector<float> ibl_src[6];

        if(params->ibl_mode)
        {
            const size_t buf_sz = TexImage_GetBufferSize(faces[0], TEX_LAYOUT_RGBA, TEX_PT_Float32);

            for(int i = 0; i < 6; i++)
            {
                ibl_src[i].resize(buf_sz / sizeof(float));

                if(TexImage_GetData(faces[i], ibl_src[i].data(), buf_sz,
                                    TEX_LAYOUT_RGBA, TEX_PT_Float32) != TEX_OK)
                {
                    CoreLog(TEX_LOG_ERROR, "IBL: face data fetch failed.");
                    for(int j = 0; j < 6; j++)TexImage_Free(faces[j]);
                    return TEX_ERR_INTERNAL;
                }
            }
        }

        // 6. 输出路径(补 .TexCube 后缀)
        std::filesystem::path out(params->output_path);

        {
            std::wstring ext = out.extension().wstring();

            for(wchar_t &c : ext)
                c = (c >= L'A' && c <= L'Z') ? c + 32 : c;

            if(ext != L".texcube")
                out += L".TexCube";
        }

        const std::wstring output_path = out.wstring();

        // 7. 写文件头
        FILE *f = nullptr;

        if(!ContainerOpen(output_path, f))
        {
            CoreLog(TEX_LOG_ERROR, "Create Texture failed.");
            for(int i = 0; i < 6; i++)TexImage_Free(faces[i]);
            return TEX_ERR_IO;
        }

        CoreLog(TEX_LOG_INFO, "Convert 6 cube faces to <"
              + std::filesystem::path(output_path).string() + ">.");

        if(!ContainerWriteHeader(f, TEX_VIEW_CUBE)
         ||!ContainerWriteSize2D(f, width, height)
         ||!ContainerWriteFormatBlock(f, fmt, miplevel))
        {
            ContainerClose(f);
            CoreLog(TEX_LOG_ERROR, "Create Texture failed.");
            for(int i = 0; i < 6; i++)TexImage_Free(faces[i]);
            return TEX_ERR_IO;
        }

        fflush(f);  // 头部立刻落盘,崩溃残留与 2D 行为一致

        // 8. mip-major 循环:每级 6 面(+X,-X,+Y,-Y,+Z,-Z),每面 8 字节补齐
        uint32_t total = 0;
        const uint32_t min_size = is_compress ? 4 : 1;
        uint32_t cw = width, ch = height;

        for(int level = 0; level < miplevel; level++)
        {
            for(int face = 0; face < 6; face++)
            {
                std::vector<uint8_t> payload;

                uint32_t bytes = 0;

                if(BuildFaceLevel(faces[face], is_compress, fmt, provider,
                                  data_layout, data_pt, payload)
                 &&!payload.empty() && payload.size() <= 0xFFFFFFFFull)
                {
                    bytes = ContainerWriteLevel(f, payload.data(), uint32_t(payload.size()));
                }

                if(bytes <= 0)
                {
                    ContainerClose(f);
                    ContainerDelete(output_path);
                    CoreLog(TEX_LOG_ERROR, "write cube face data failed.");
                    for(int i = 0; i < 6; i++)TexImage_Free(faces[i]);
                    return TEX_ERR_ENCODE;
                }

                total += bytes;

                if(progress && progress(user, float(level * 6 + face + 1) / float(miplevel * 6)))
                {
                    ContainerClose(f);
                    ContainerDelete(output_path);
                    CoreLog(TEX_LOG_WARN, "cancelled.");
                    for(int i = 0; i < 6; i++)TexImage_Free(faces[i]);
                    return TEX_ERR_CANCELLED;
                }
            }

            // 为下一级缩放全部面
            if(miplevel > 1 && level < miplevel)
            {
                if(cw > min_size)cw >>= 1;
                if(ch > min_size)ch >>= 1;

                for(int i = 0; i < 6; i++)
                    TexImage_Resize(faces[i], cw, ch);
            }
        }

        CoreLog(TEX_LOG_INFO, "pixel total length: " + std::to_string(total) + " bytes.");

        ContainerClose(f);

        // 9. IBL 后处理:原始 cubemap 已写出,再生成 _irradiance 和 _prefilter
        if(params->ibl_mode)
        {
            WriteIBLCubeFile(out, L"_irradiance", ibl_src, width, height,
                             [](const std::vector<float> s[6], uint32_t sw, uint32_t sh,
                                    std::vector<float> d[6], uint32_t dw, uint32_t dh)
                             {
                                 return BakeDiffuseIrradiance(s, sw, sh, d, dw, dh, 256);
                             }, provider, params->ibl_format, progress, user);

            WriteIBLCubeFile(out, L"_prefilter", ibl_src, width, height,
                             [](const std::vector<float> s[6], uint32_t sw, uint32_t sh,
                                    std::vector<float> d[6], uint32_t dw, uint32_t dh)
                             {
                                 return BakeGGXPrefilter(s, sw, sh, d, dw, dh, 0.5f, 256);
                             }, provider, params->ibl_format, progress, user);
        }

        for(int i = 0; i < 6; i++)TexImage_Free(faces[i]);
        return TEX_OK;
    }

    /// IBL 产物写出:烘焙 → half16 或 u8 → BC6H/BC7 压缩或未压缩 .TexCube
    /// 输出分辨率:irradiance 固定 32x32;prefilter 与源同尺寸
    /// 输出格式:ibl_format 非空 = 显式指定(BC6H/BC7/RGBA16F/RGBA8),校验失败
    ///          警告后回退自动;空 = 自动(BC6H 优先,不支持时 RGBA16F)。
    /// 编码源量化:BC6H/RGBA16F → half16(HDR);BC7/RGBA8 → u8(LDR,RGB8 源推荐)
    void WriteIBLCubeFile(const std::filesystem::path &base_path,
                          const wchar_t *suffix,
                          const std::vector<float> src[6],
                          uint32_t src_w, uint32_t src_h,
                          bool (*bake_fn)(const std::vector<float>*, uint32_t, uint32_t,
                                          std::vector<float>*, uint32_t, uint32_t),
                          const TexEncoderProvider *provider,
                          const char *ibl_format,
                          TexProgressFn progress, void *user)
    {
        const uint32_t dst_w = (suffix[1] == L'i') ? 32 : src_w;    // _irradiance=32, _prefilter=src
        const uint32_t dst_h = (suffix[1] == L'i') ? 32 : src_h;

        // 候选格式校验:压缩格式须编码器支持且源布局匹配(BC6H→half,BC7→u8);
        // 非压缩格式须 4 通道合法且为 Float16/UInt8。usable=false 表示该格式名存在但不可用
        auto try_format = [&provider](const char *name,
                                      const TexPixelFormat *&out_fmt,
                                      bool &is_compress, bool &quant_u8) -> bool
        {
            out_fmt = TexFormat_Get(name);

            if(!out_fmt)
                return false;

            if(TexFormat_IsCompress(out_fmt))
            {
                const bool half_src = (strcmp(name, "BC6H") == 0
                                    || strcmp(name, "BC6H_SF") == 0);
                const int src_pt = half_src ? TEX_PT_Float16 : TEX_PT_UInt8;

                if(provider)
                {
                    int data_layout = 0, data_pt = 0;

                    if(!ProviderSupportsTarget(provider, name)
                     || provider->QuerySourceLayout(name, 4, src_pt,
                                                    &data_layout, &data_pt) != TEX_OK
                     || data_layout != TEX_LAYOUT_RGBA
                     || data_pt != src_pt)
                        return false;
                }
                else
                    return false;

                is_compress = true;
                quant_u8    = !half_src;
                return true;
            }

            if(!TexFormat_IsAllowed(4, out_fmt))
                return false;

            const int pt = TexFormat_PixelType(out_fmt);

            if(pt != TEX_PT_Float16 && pt != TEX_PT_UInt8)
                return false;

            is_compress = false;
            quant_u8    = (pt == TEX_PT_UInt8);
            return true;
        };

        const TexPixelFormat *fmt = nullptr;
        bool is_compress = false;
        bool quant_u8    = false;

        if(ibl_format && *ibl_format)
        {
            if(try_format(ibl_format, fmt, is_compress, quant_u8))
                CoreLog(TEX_LOG_INFO, std::string("IBL: output format ") + ibl_format);
            else
                CoreLog(TEX_LOG_WARN, std::string("IBL: format ") + ibl_format
                                      + " is not available, fallback to auto.");
        }

        if(!fmt)
        {
            if(!try_format("BC6H", fmt, is_compress, quant_u8))
            {
                fmt         = TexFormat_Get("RGBA16F");
                is_compress = false;
                quant_u8    = false;
            }
        }

        if(!fmt)
        {
            CoreLog(TEX_LOG_ERROR, "IBL: no usable output format.");
            return;
        }

        std::vector<float> baked[6];

        for(int i = 0; i < 6; i++)
            baked[i].resize(size_t(dst_w) * dst_h * 4);

        if(!bake_fn(src, src_w, src_h, baked, dst_w, dst_h))
        {
            CoreLog(TEX_LOG_ERROR, "IBL bake failed.");
            return;
        }

        // float32 → half(简化截断转换,精度对 IBL 天空值足够)
        auto f32_to_f16 = [](float v) -> uint16_t
        {
            union { float f; uint32_t u; } fu;
            fu.f = v;
            const uint32_t sign = (fu.u >> 16) & 0x8000;
            int32_t exp = int32_t((fu.u >> 23) & 0xFF) - 127 + 15;
            uint32_t mant = (fu.u >> 13) & 0x3FF;

            if(exp <= 0)   return uint16_t(sign);
            if(exp >= 31)  return uint16_t(sign | 0x7C00);
            return uint16_t(sign | (uint32_t(exp) << 10) | mant);
        };

        // float32 → u8(UNORM,clamp 到 0-1;LDR 目标格式用)
        auto f32_to_u8 = [](float v) -> uint8_t
        {
            if(v <= 0.0f)  return 0;
            if(v >= 1.0f)  return 255;
            return uint8_t(v * 255.0f + 0.5f);
        };

        const size_t pixel_count = size_t(dst_w) * dst_h;

        std::vector<uint16_t> half_faces[6];
        std::vector<uint8_t>  u8_faces[6];

        if(quant_u8)
        {
            for(int face = 0; face < 6; face++)
            {
                u8_faces[face].resize(pixel_count * 4);

                const float *fp = baked[face].data();
                uint8_t    *up = u8_faces[face].data();

                for(size_t p = 0; p < pixel_count; p++)
                {
                    up[p*4+0] = f32_to_u8(fp[p*4+0]);
                    up[p*4+1] = f32_to_u8(fp[p*4+1]);
                    up[p*4+2] = f32_to_u8(fp[p*4+2]);
                    up[p*4+3] = f32_to_u8(fp[p*4+3]);
                }
            }
        }
        else
        {
            for(int face = 0; face < 6; face++)
            {
                half_faces[face].resize(pixel_count * 4);

                const float *fp = baked[face].data();
                uint16_t   *hp = half_faces[face].data();

                for(size_t p = 0; p < pixel_count; p++)
                {
                    hp[p*4+0] = f32_to_f16(fp[p*4+0]);
                    hp[p*4+1] = f32_to_f16(fp[p*4+1]);
                    hp[p*4+2] = f32_to_f16(fp[p*4+2]);
                    hp[p*4+3] = f32_to_f16(fp[p*4+3]);
                }
            }
        }

        // 输出格式:显式指定(BC6H/BC7/RGBA16F/RGBA8)或自动(BC6H→RGBA16F);
        // 编码器支持 BC6H 且声明接受 RGBA 半精度源 → 压缩,否则未压缩
        // (try_format 已在上面完成解析)

        // 输出路径:<base> 去扩展名 + 后缀 + .TexCube(sky34 → sky34_irradiance.TexCube;
        // 注意 replace_extension 会在无点前缀时自动补点,得到 "xxx._irradiance" 错误命名)
        std::filesystem::path out = base_path;
        out.replace_extension();
        out += std::wstring(suffix) + L".TexCube";

        const std::wstring out_w = out.wstring();

        FILE *f = nullptr;

        if(!ContainerOpen(out_w, f))
        {
            CoreLog(TEX_LOG_ERROR, "IBL: create file failed.");
            return;
        }

        if(!ContainerWriteHeader(f, TEX_VIEW_CUBE)
         ||!ContainerWriteSize2D(f, dst_w, dst_h)
         ||!ContainerWriteFormatBlock(f, fmt, 1))
        {
            ContainerClose(f);
            ContainerDelete(out_w);
            return;
        }

        uint32_t total = 0;

        for(int face = 0; face < 6; face++)
        {
            uint32_t bytes = 0;

            if(is_compress)
            {
                // 直接进编码器:RGBA 源(BC6H=half 8B/px,BC7=u8 4B/px)→ 块序列
                TexEncodeRequest req;
                memset(&req, 0, sizeof(req));

                req.src            = quant_u8
                                   ? reinterpret_cast<const uint8_t *>(u8_faces[face].data())
                                   : reinterpret_cast<const uint8_t *>(half_faces[face].data());
                req.width          = dst_w;
                req.height         = dst_h;
                req.src_layout     = TEX_LAYOUT_RGBA;
                req.src_pixel_type = quant_u8 ? TEX_PT_UInt8 : TEX_PT_Float16;
                req.target_format  = fmt->name;
                req.quality        = 100;
                req.thread_count   = 0;

                uint8_t *out_data = nullptr;
                size_t   out_bytes = 0;

                if(provider->Encode(&req, &out_data, &out_bytes) == TEX_OK
                 && out_data && out_bytes > 0)
                {
                    bytes = ContainerWriteLevel(f, out_data, uint32_t(out_bytes));
                    provider->FreeResult(out_data);
                }
                else
                {
                    if(out_data)provider->FreeResult(out_data);
                    CoreLog(TEX_LOG_ERROR, "IBL: block encode failed.");
                }
            }
            else if(quant_u8)
            {
                bytes = ContainerWriteLevel(f, u8_faces[face].data(),
                                            uint32_t(pixel_count * 4));
            }
            else
            {
                bytes = ContainerWriteLevel(f, half_faces[face].data(),
                                            uint32_t(pixel_count * 4 * 2));
            }

            if(bytes <= 0)
            {
                ContainerClose(f);
                ContainerDelete(out_w);
                CoreLog(TEX_LOG_ERROR, "IBL: write face data failed.");
                return;
            }

            total += bytes;

            if(progress && progress(user, float(face + 1) / 6.0f))
            {
                ContainerClose(f);
                ContainerDelete(out_w);
                CoreLog(TEX_LOG_WARN, "IBL: cancelled.");
                return;
            }
        }

        ContainerClose(f);

        CoreLog(TEX_LOG_INFO, "IBL: " + std::filesystem::path(out_w).string()
                              + " (" + fmt->name + ", " + std::to_string(total) + " bytes).");
    }
}//namespace texcore
