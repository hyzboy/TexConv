// TexConvCore —— 转换流水线。
// 迁自原 ConvertImage.cpp + TextureFileCreaterR/RG/RGB/RGBA.cpp,行为逐字节对齐:
//   * 灰度/丢弃 alpha 在文件创建前转换(失败无文件残留)
//   * 未压缩格式的布局转换发生在文件头写出之后(失败留 32 字节头文件,对齐旧 InitFormat)
//   * mip 循环、压缩格式 4x4 下限、8 字节补齐均与旧版一致
//   * use_color_key 旧版仅解析未使用,此处同样不使用(保持产物一致)

#include "internal.h"
#include "texconv/tex_bitpack.h"
#include "texconv/tex_distance_field.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

namespace texcore
{
    static const char *default_slot_name[4] = {"BC4", "BC5", "BC7", "BC7"};

    /// hgl::GetMipLevel 的等价实现:floor(log2(size))+1
    static uint32_t get_mip_level(uint32_t size)
    {
        uint32_t level = 0;
        while(size)
        {
            ++level;
            size >>= 1;
        }
        return level;   // log2 向下取整 + 1(size 为 0 时返回 0,不会发生)
    }

    int CalcMipLevels(uint32_t width, uint32_t height, bool is_compress, bool gen_mipmaps)
    {
        int miplevel = 1;

        if(gen_mipmaps)
        {
            miplevel = int(get_mip_level((std::max)(width, height)));

            if(is_compress && (width > 4 || height > 4))
                miplevel -= 2;
        }

        return miplevel;
    }

    const TexEncoderProvider *SelectProvider(const char *short_name)
    {
        auto &list = Providers();

        if(list.empty())
            return nullptr;

        if(short_name && *short_name)
        {
            for(auto &p : list)
            {
                const char *a = p.provider->short_name;
                const char *b = short_name;

                // ASCII 大小写不敏感比较
                while(*a && *b)
                {
                    char ca = *a, cb = *b;
                    if(ca >= 'a' && ca <= 'z')ca -= 32;
                    if(cb >= 'a' && cb <= 'z')cb -= 32;
                    if(ca != cb)break;
                    ++a; ++b;
                }

                if(*a == *b)
                    return p.provider;
            }

            return nullptr;     // 指定的后端不存在(旧版 /Intel 缺失 → 报错退出)
        }

        // 未指定:优先 Intel(AMD BC7 对部分内容存在死锁/极慢问题),否则取第一个
        for(auto &p : list)
            if(strcmp(p.provider->short_name, "Intel") == 0)
                return p.provider;

        for(auto &p : list)
            if(strcmp(p.provider->short_name, "AMD") == 0)
                return p.provider;

        return list.front().provider;
    }

    bool EnsureProviderInited(const TexEncoderProvider *provider)
    {
        for(auto &p : Providers())
        {
            if(p.provider == provider)
            {
                if(!p.inited)
                {
                    CoreLog(TEX_LOG_INFO, std::string("provider init: ") + p.provider->short_name);
                    if(p.provider->Init() != TEX_OK)
                        return false;

                    p.inited = true;
                }

                return true;
            }
        }

        return false;
    }

    /// 解析输出路径:有 output_path 用之(无 .Tex2D 后缀则补);否则输入同目录同名换后缀
    static std::wstring resolve_output_path(const TexJobParams *params)
    {
        std::filesystem::path out;

        if(params->output_path && *params->output_path)
        {
            out = std::filesystem::path(params->output_path);

            std::wstring ext = out.extension().wstring();
            for(wchar_t &c : ext)
                c = (c >= L'A' && c <= L'Z') ? c + 32 : c;

            if(ext != L".tex2d")
                out += L".Tex2D";
        }
        else
        {
            out = std::filesystem::path(params->input_path);
            out.replace_extension(L".Tex2D");
        }

        return out.wstring();
    }

    static bool ascii_ieq(const char *a, const char *b)
    {
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

    namespace
    {
        /// 一次任务中的图像与格式上下文
        struct JobContext
        {
            TexImage              img         = nullptr;
            const TexPixelFormat *fmt         = nullptr;
            bool                  is_compress = false;
            int                   channels    = 0;
            uint32_t              width       = 0;
            uint32_t              height      = 0;
            int                   req_layout  = 0;      // 压缩:编码器要求的布局
            int                   req_pt      = 0;
            int                   unc_layout  = 0;      // 非压缩:转换后的布局
            int                   unc_pt      = 0;      // 非压缩:目标像素类型

            const TexEncoderProvider *provider = nullptr;
        };

        /// 未压缩格式的一级数据(含 RGB565/B10GR11UF/RGBA4 等位打包),迁自旧 Write()
        bool build_uncompressed_level(JobContext &ctx, std::vector<uint8_t> &payload)
        {
            return BuildUncompressedLevel(ctx.img, ctx.fmt, ctx.unc_layout, ctx.unc_pt, payload);
        }

        /// 压缩格式的一级数据
        bool build_compressed_level(JobContext &ctx, std::vector<uint8_t> &payload)
        {
            return BuildCompressedLevel(ctx.img, ctx.provider, ctx.fmt,
                                        ctx.req_layout, ctx.req_pt, payload);
        }
    }//namespace

    // ---------------------------------------------------------------- 共享实现(供 2D 与 Cube 流水线复用)

    bool BuildUncompressedLevel(TexImage img, const TexPixelFormat *fmt,
                                int layout, int pixel_type,
                                std::vector<uint8_t> &payload)
    {
        uint32_t cur_w = 0, cur_h = 0;
        TexImage_GetInfo(img, &cur_w, &cur_h, nullptr, nullptr, nullptr);
        const uint32_t pixel_total = cur_w * cur_h;

        const size_t raw_need = TexImage_GetBufferSize(img, layout, pixel_type);
        std::vector<uint8_t> raw(raw_need);

        if(TexImage_GetData(img, raw.data(), raw.size(), layout, pixel_type) != TEX_OK)
            return false;

        const uint32_t total_bytes = (fmt->total_bits * pixel_total) >> 3;

        payload.resize(total_bytes);

        switch(fmt->format)
        {
            case TEX_FMT_RGB32U:
            case TEX_FMT_RGB32I:
            case TEX_FMT_RGB32F:
            case TEX_FMT_R8:
            case TEX_FMT_R16:
            case TEX_FMT_R16U:
            case TEX_FMT_R16I:
            case TEX_FMT_R16F:
            case TEX_FMT_R32U:
            case TEX_FMT_R32I:
            case TEX_FMT_R32F:
            case TEX_FMT_RG8:
            case TEX_FMT_RG16:
            case TEX_FMT_RG16U:
            case TEX_FMT_RG16I:
            case TEX_FMT_RG16F:
            case TEX_FMT_RG32U:
            case TEX_FMT_RG32I:
            case TEX_FMT_RG32F:
            case TEX_FMT_RGBA8:
            case TEX_FMT_RGBA8SN:
            case TEX_FMT_RGBA8U:
            case TEX_FMT_RGBA8I:
            case TEX_FMT_RGBA16:
            case TEX_FMT_RGBA16SN:
            case TEX_FMT_RGBA16U:
            case TEX_FMT_RGBA16I:
            case TEX_FMT_RGBA16F:
            case TEX_FMT_RGBA32U:
            case TEX_FMT_RGBA32I:
            case TEX_FMT_RGBA32F:
                memcpy(payload.data(), raw.data(), total_bytes);
                return true;

            case TEX_FMT_RGB565:
                Tex_RGB8toRGB565_Array((uint16_t *)payload.data(), raw.data(), pixel_total);
                return true;

            case TEX_FMT_B10GR11UF:
                Tex_RGB16FtoB10GR11UF((uint32_t *)payload.data(), (uint16_t *)raw.data(), pixel_total);
                return true;

            case TEX_FMT_RGBA4:
                Tex_RGBA8toRGBA4((uint16_t *)payload.data(), raw.data(), pixel_total);
                return true;

            case TEX_FMT_BGRA4:
                Tex_RGBA8toBGRA4((uint16_t *)payload.data(), raw.data(), pixel_total);
                return true;

            case TEX_FMT_A1RGB5:
                Tex_RGBA8toA1RGB5((uint16_t *)payload.data(), raw.data(), pixel_total);
                return true;

            case TEX_FMT_A2BGR10:
                Tex_RGBA16toA2BGR10((uint32_t *)payload.data(), (uint16_t *)raw.data(), pixel_total);
                return true;

            default:
                return false;
        }
    }

    bool BuildCompressedLevel(TexImage img, const TexEncoderProvider *provider,
                              const TexPixelFormat *fmt,
                              int layout, int pixel_type,
                              std::vector<uint8_t> &payload)
    {
        fprintf(stderr, "[dbg2] BC7 build enter\n");
        const size_t raw_need = TexImage_GetBufferSize(img, layout, pixel_type);
        std::vector<uint8_t> raw(raw_need);

        if(TexImage_GetData(img, raw.data(), raw.size(), layout, pixel_type) != TEX_OK)
            return false;

        fprintf(stderr, "[dbg2] data fetched\n");

        uint32_t cur_w = 0, cur_h = 0;
        TexImage_GetInfo(img, &cur_w, &cur_h, nullptr, nullptr, nullptr);

        TexEncodeRequest req;
        memset(&req, 0, sizeof(req));

        req.src            = raw.data();
        req.width          = cur_w;
        req.height         = cur_h;
        req.src_layout     = layout;
        req.src_pixel_type = pixel_type;
        req.target_format  = fmt->name;
        req.quality        = 100;       // 旧版 fquality=1.0
        req.thread_count   = 0;         // 插件自定(BC4 单线程,其余 8 线程)

        uint8_t *out_data  = nullptr;
        size_t   out_bytes = 0;

        fprintf(stderr, "[dbg2] before Encode %ux%u\n", req.width, req.height);

        if(provider->Encode(&req, &out_data, &out_bytes) != TEX_OK)
            return false;

        fprintf(stderr, "[dbg2] after Encode %zu bytes\n", out_bytes);

        payload.assign(out_data, out_data + out_bytes);
        provider->FreeResult(out_data);

        return true;
    }


    int RunJobImpl(const TexJobParams *params, TexProgressFn progress, void *user)
    {
        if(!params || !params->input_path || !*params->input_path)
            return TEX_ERR_PARAM;

        // 压缩后端选择(默认 Intel;/AMD|/Intel 显式指定,缺失的后端 → 报错)
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

        JobContext ctx;
        ctx.provider = provider;

        // 1. 加载
        int rc = TexImage_Load(&ctx.img, params->input_path);

        if(rc != TEX_OK)
            return rc;

        int layout = 0, pt = 0;
        TexImage_GetInfo(ctx.img, &ctx.width, &ctx.height, nullptr, &layout, &pt);

        // 2. 灰度(对齐旧 ConvertImage:有 alpha 或未丢弃 alpha → 转 RG,否则转 Gray)
        if(params->force_grayscale)
        {
            if(TexImage_HasAlpha(ctx.img) || !params->discard_alpha)
                TexImage_Convert(ctx.img, TEX_LAYOUT_GrayAlpha, pt);
            else
                TexImage_Convert(ctx.img, TEX_LAYOUT_Gray, pt);

            TexImage_GetInfo(ctx.img, nullptr, nullptr, nullptr, &layout, &pt);
        }

        // 3. 丢弃 alpha
        if(params->discard_alpha)
        {
            if(layout == TEX_LAYOUT_RGBA)
                TexImage_Convert(ctx.img, TEX_LAYOUT_RGB, pt);
            else if(layout == TEX_LAYOUT_GrayAlpha)
                TexImage_Convert(ctx.img, TEX_LAYOUT_Gray, pt);

            TexImage_GetInfo(ctx.img, nullptr, nullptr, nullptr, &layout, &pt);
        }

        // 3.5 距离场模式:单通道源对灰度生成;带 Alpha 的源(RGBA/GrayAlpha)对
        //     Alpha 生成。生成后图像替换为单通道 8-bit 距离场,按 1 通道源继续。
        if(params->df_mode)
        {
            int df_layout = 0;

            if(layout == TEX_LAYOUT_Gray)
                df_layout = TEX_LAYOUT_Gray;
            else if(layout == TEX_LAYOUT_RGBA || layout == TEX_LAYOUT_GrayAlpha)
                df_layout = TEX_LAYOUT_Alpha;
            else
            {
                CoreLog(TEX_LOG_ERROR,
                        "distance field requires a 1-channel texture, or alpha channel (RGBA/GrayAlpha). "
                        "Use /discard_alpha for RGB sources without alpha.");
                TexImage_Free(ctx.img);
                return TEX_ERR_UNSUPPORTED;
            }

            uint32_t df_w = 0, df_h = 0;
            TexImage_GetInfo(ctx.img, &df_w, &df_h, nullptr, nullptr, nullptr);

            std::vector<uint8_t> df_src(size_t(df_w) * df_h);
            std::vector<uint8_t> df_dst(size_t(df_w) * df_h);

            if(TexImage_GetData(ctx.img, df_src.data(), df_src.size(),
                                df_layout, TEX_PT_UInt8) != TEX_OK)
            {
                CoreLog(TEX_LOG_ERROR, "distance field: failed to get source channel data.");
                TexImage_Free(ctx.img);
                return TEX_ERR_INTERNAL;
            }

            const uint8_t threshold = params->df_threshold > 0 && params->df_threshold <= 255
                                    ? uint8_t(params->df_threshold) : uint8_t(128);

            TexDF_Generate(df_src.data(), df_dst.data(), df_w, df_h,
                           threshold, 0, 0);     // scale/bias 取默认(3/128,对齐旧 DFGen)

            TexImage df_img = nullptr;

            if(TexImage_CreateFromData(&df_img, df_w, df_h, 1, TEX_PT_UInt8, df_dst.data()) != TEX_OK)
            {
                CoreLog(TEX_LOG_ERROR, "distance field: failed to create image from result.");
                TexImage_Free(ctx.img);
                return TEX_ERR_INTERNAL;
            }

            CoreLog(TEX_LOG_INFO, "distance field generated from "
                                  + std::string(df_layout == TEX_LAYOUT_Gray ? "gray" : "alpha")
                                  + " channel, threshold " + std::to_string(threshold) + ".");

            TexImage_Free(ctx.img);
            ctx.img = df_img;

            TexImage_GetInfo(ctx.img, nullptr, nullptr, nullptr, &layout, &pt);
        }

        ctx.channels = TexLayoutChannels(layout);

        if(ctx.channels <= 0 || ctx.channels > 4)
        {
            CoreLog(TEX_LOG_ERROR, "image format don't support ");
            TexImage_Free(ctx.img);
            return TEX_ERR_UNSUPPORTED;
        }

        // 4. 目标格式解析(槽位模式对齐旧 ParseParamFormat:名字非法回退默认)
        if(params->target_format)
        {
            ctx.fmt = TexFormat_Get(params->target_format);

            if(!ctx.fmt)
            {
                CoreLog(TEX_LOG_ERROR, std::string("[FORMAT ERROR] Don't support ")
                                       + params->target_format + " format.");
                TexImage_Free(ctx.img);
                return TEX_ERR_UNSUPPORTED;
            }
        }
        else
        {
            const int idx = ctx.channels - 1;

            // DF 模式下 1 通道默认 R8:AMD 的 BC4 灰度源路径存在已知问题
            //(CMP 返回 0 字节,见基线 g8_default),未压缩 R8 处处可用
            const char *slot_default = default_slot_name[idx];

            if(idx == 0 && params->df_mode)
                slot_default = "R8";

            const char *slot_name = params->normal_map
                                  ? "BC5"
                                  : (params->slot_format[idx] ? params->slot_format[idx]
                                                              : slot_default);

            ctx.fmt = TexFormat_Get(slot_name);

            if(!ctx.fmt)
            {
                // 对齐旧 ParseParamFormat:[FORMAT ERROR] 后回退默认格式
                CoreLog(TEX_LOG_INFO, std::string("[FORMAT ERROR] Don't support ")
                                      + slot_name + " format.");

                ctx.fmt = TexFormat_Get(default_slot_name[idx]);
            }
        }

        ctx.is_compress = TexFormat_IsCompress(ctx.fmt);

        CoreLog(TEX_LOG_INFO, std::string("job: fmt=") + ctx.fmt->name
                              + " compress=" + std::to_string(ctx.is_compress ? 1 : 0)
                              + " channels=" + std::to_string(ctx.channels));

        // 5. mip 级数(对齐旧版:压缩格式 4x4 下限,大于 4x4 的图少 2 级)
        const int miplevel = CalcMipLevels(ctx.width, ctx.height, ctx.is_compress,
                                           params->gen_mipmaps != 0);

        // 6. 压缩:向编码器询问源布局要求并转换(在文件创建前,失败无文件)
        if(ctx.is_compress)
        {
            rc = provider->QuerySourceLayout(ctx.fmt->name, ctx.channels, pt,
                                             &ctx.req_layout, &ctx.req_pt);

            if(rc != TEX_OK)
            {
                TexImage_Free(ctx.img);
                return rc == TEX_ERR_UNSUPPORTED ? TEX_ERR_UNSUPPORTED : rc;
            }

            if(ctx.req_layout != layout || ctx.req_pt != pt)
                TexImage_Convert(ctx.img, ctx.req_layout, ctx.req_pt);
        }

        // 7. 创建文件 + 头部(对齐旧 CreateTexFile/WriteSize2D/WritePixelFormat 顺序)
        const std::wstring output_path = resolve_output_path(params);

        FILE *f = nullptr;

        if(!ContainerOpen(output_path, f))
        {
            CoreLog(TEX_LOG_ERROR, "Create Texture failed.");
            TexImage_Free(ctx.img);
            return TEX_ERR_IO;
        }

        CoreLog(TEX_LOG_INFO, "Convert <"
              + std::filesystem::path(params->input_path).string()
              + "> to <"
              + std::filesystem::path(output_path).string()
              + ">.");

        bool header_ok = ContainerWriteHeader(f, TEX_VIEW_2D)
                      && ContainerWriteSize2D(f, ctx.width, ctx.height)
                      && ContainerWriteFormatBlock(f, ctx.fmt, miplevel);

        if(!header_ok)
        {
            ContainerClose(f);
            CoreLog(TEX_LOG_ERROR, "Create Texture failed.");
            TexImage_Free(ctx.img);
            return TEX_ERR_IO;
        }

        // 头部立刻落盘:编码阶段若进程崩溃(旧版存在,如 /gray+BC5 路径),
        // 残留文件必须与旧版一致为完整 32 字节头
        fflush(f);

        // 8. 非压缩:目标像素类型与布局转换(失败留 32 字节头,对齐旧 InitFormat 失败)
        if(!ctx.is_compress)
        {
            // "源通道数 × 目标格式"合法性矩阵(对齐旧 R/RG/RGB/RGBA 创建器的白名单;
            // 如 3 通道源不可写 RGBA8)。校验放在头写出之后,失败残留与旧版一致。
            bool convert_ok = TexFormat_IsAllowed(ctx.channels, ctx.fmt) != 0;

            ctx.unc_pt = convert_ok ? TexFormat_PixelType(ctx.fmt) : -1;

            convert_ok = convert_ok && ctx.unc_pt >= 0;

            if(convert_ok)
            {
                // 旧 CreateTFC 按源通道数选择创建器,转换行为与源通道数绑定
                switch(ctx.channels)
                {
                    case 1: convert_ok = TexImage_Convert(ctx.img, TEX_LAYOUT_Alpha,     ctx.unc_pt) == TEX_OK; break;
                    case 2: convert_ok = TexImage_Convert(ctx.img, TEX_LAYOUT_GrayAlpha, ctx.unc_pt) == TEX_OK; break;
                    case 3: convert_ok = TexImage_Convert(ctx.img, TEX_LAYOUT_RGB,       ctx.unc_pt) == TEX_OK; break;
                    case 4: convert_ok = TexImage_Convert(ctx.img, TEX_LAYOUT_RGBA,      ctx.unc_pt) == TEX_OK; break;
                }
            }

            if(convert_ok)
                TexImage_GetInfo(ctx.img, nullptr, nullptr, nullptr, &ctx.unc_layout, nullptr);
            else
            {
                // 旧版日志(带创建器源文件位置,此处简化为同义行);文件头残留与旧版一致
                CoreLog(TEX_LOG_ERROR, std::string("Don't support this format: ") + ctx.fmt->name);
                ContainerClose(f);
                TexImage_Free(ctx.img);
                return TEX_ERR_UNSUPPORTED;
            }
        }

        // 9. mip 循环
        uint32_t total = 0;
        const uint32_t min_size = ctx.is_compress ? 4 : 1;

        uint32_t width  = ctx.width;
        uint32_t height = ctx.height;

        for(int i = 0; i < miplevel; i++)
        {
            std::vector<uint8_t> payload;
            bool build_ok = ctx.is_compress
                          ? build_compressed_level(ctx, payload)
                          : build_uncompressed_level(ctx, payload);

            uint32_t bytes = 0;

            if(build_ok)
            {
                if(payload.size() > 0xFFFFFFFFull)
                    bytes = 0;
                else
                    bytes = ContainerWriteLevel(f, payload.data(), uint32_t(payload.size()));
            }

            if(bytes <= 0)
            {
                ContainerClose(f);
                ContainerDelete(output_path);
                CoreLog(TEX_LOG_ERROR, "Create Texture failed.");
                TexImage_Free(ctx.img);
                return TEX_ERR_ENCODE;
            }

            total += bytes;

            if(progress && progress(user, float(i + 1) / float(miplevel)))
            {
                ContainerClose(f);
                ContainerDelete(output_path);
                CoreLog(TEX_LOG_WARN, "cancelled.");
                TexImage_Free(ctx.img);
                return TEX_ERR_CANCELLED;
            }

            // 对齐旧版:循环内每次(包括最后一次)都尝试缩放
            if(miplevel > 1 && i < miplevel)
            {
                if(width  > min_size)width  >>= 1;
                if(height > min_size)height >>= 1;

                TexImage_Resize(ctx.img, width, height);
            }
        }

        CoreLog(TEX_LOG_INFO, "pixel total length: " + std::to_string(total) + " bytes.");

        ContainerClose(f);

        TexImage_Free(ctx.img);
        return TEX_OK;
    }
}//namespace texcore
