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
                fprintf(stderr, "[dbg] level %d face %d: encoding\n", level, face);
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

        for(int i = 0; i < 6; i++)TexImage_Free(faces[i]);
        return TEX_OK;
    }
}//namespace texcore
