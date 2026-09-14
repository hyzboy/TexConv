// TexImage.dll —— TexImage C API 实现(包装 MagickImage / ImageMagick++)。
// hgl 仅在本 DLL 内使用;对外一律 C ABI。

#include "MagickImage.h"
#include "texconv/tex_image.h"

#include <hgl/log/Logger.h>
#include <hgl/log/LogMessage.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/plugin/PlugIn.h>

#include <windows.h>
#include <string>
#include <mutex>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

using namespace hgl;

// hgl::logger::AddLogger / InitLog / CreateLoggerFile 在 CMCore 中已定义但未公开声明,此处补齐
namespace hgl::logger
{
    bool    AddLogger(Logger *log);
    PlugIn *InitLog();
    void    CloseLog();
    Logger *CreateLoggerFile(const OSString &project_code, hgl::LogLevel ll);
}//namespace hgl::logger

namespace
{
    TexLogFn    g_log_fn    = nullptr;
    void       *g_log_user  = nullptr;
    bool        g_inited    = false;

    int MapLogLevel(hgl::LogLevel level)
    {
        using hgl::LogLevel;

        if(level >= LogLevel::Error)    return TEX_LOG_ERROR;
        if(level >= LogLevel::Warning)  return TEX_LOG_WARN;
        if(level >= LogLevel::Info)     return TEX_LOG_INFO;
        return TEX_LOG_VERBOSE;
    }

    /// 把 hgl 日志系统的输出转发给宿主回调
    class ForwardLogger:public hgl::logger::Logger
    {
    public:

        using Logger::Logger;

        void Close() override {}

        void Write(const hgl::logger::LogMessage *msg) override
        {
            if(!msg || !g_log_fn)
                return;

            std::string line;

            if(msg->text.message_u8 && msg->text.message_u8_length > 0)
            {
                line.assign((const char *)msg->text.message_u8, msg->text.message_u8_length);
            }
            else if(msg->text.message_u16 && msg->text.message_u16_length > 0)
            {
                const wchar_t *w = (const wchar_t *)msg->text.message_u16;
                int n = WideCharToMultiByte(CP_UTF8, 0, w, msg->text.message_u16_length,
                                            nullptr, 0, nullptr, nullptr);
                if(n > 0)
                {
                    line.resize(n);
                    WideCharToMultiByte(CP_UTF8, 0, w, msg->text.message_u16_length,
                                        &line[0], n, nullptr, nullptr);
                }
            }

            if(!line.empty())
                g_log_fn(g_log_user, MapLogLevel(msg->meta.level), line.c_str());
        }
    };

    ImagePixelType ToHglPixelType(int pt)
    {
        return (ImagePixelType)pt;      // 值序对齐,见 tex_pixel.h
    }

    ImageChannelLayout ToHglLayout(int layout)
    {
        return (ImageChannelLayout)layout;
    }
}//namespace

extern "C"
{
    int TexImage_Init(const TexImageCallbacks *cb)
    {
        static std::mutex mtx;
        std::lock_guard<std::mutex> lock(mtx);

        if(g_inited)
            return TEX_OK;

        if(cb)
        {
            g_log_fn    = cb->log;
            g_log_user  = cb->user;
        }

        // 显式给出模块路径:nullptr 在 Debug 配置下会导致 IM 的
        // coder/配置解析异常("no decode delegate"),与部署位置绑定才稳定
        {
            wchar_t module_path[MAX_PATH];
            GetModuleFileNameW((HMODULE)&__ImageBase, module_path, MAX_PATH);

            std::wstring dir(module_path);
            const size_t sep = dir.find_last_of(L"\\");
            if(sep != std::wstring::npos)
                dir.resize(sep);

            const int n = WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), int(dir.size()),
                                              nullptr, 0, nullptr, nullptr);
            std::string dir_utf8(size_t(n > 0 ? n : 0), 0);

            if(n > 0)
                WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), int(dir.size()),
                                    &dir_utf8[0], n, nullptr, nullptr);

            Magick::InitializeMagick(dir_utf8.c_str());
        }

        // 诊断文件日志(与旧版 InitLogger 行为一致,写入 %LOCALAPPDATA%\.cmgdk\TexConv*.log);
        // 控制台输出不再由本库负责,由外壳打印 log 回调的行,避免 GUI/CLI 双重输出。
        hgl::logger::Logger *file_logger =
            hgl::logger::CreateLoggerFile(OS_TEXT("TexConv"), hgl::LogLevel::Verbose);

        if(file_logger)
            hgl::logger::AddLogger(file_logger);

        hgl::logger::AddLogger(new ForwardLogger(hgl::LogLevel::Verbose));

        hgl::logger::InitLog();

        g_inited = true;
        return TEX_OK;
    }

    void TexImage_Shutdown(void)
    {
        hgl::logger::CloseLog();
        g_inited = false;
    }

    int TexImage_Load(TexImage *out, const wchar_t *path)
    {
        if(!out || !path || !*path)
            return TEX_ERR_PARAM;

        MagickImage *img = new MagickImage;

        if(!img->LoadFile(OSString((const os_char *)path)))
        {
            delete img;
            return TEX_ERR_LOAD_IMAGE;
        }

        *out = (TexImage)img;
        return TEX_OK;
    }

    void TexImage_Free(TexImage img)
    {
        delete (MagickImage *)img;
    }

    void TexImage_GetInfo(TexImage img,
                          uint32_t *width, uint32_t *height, uint32_t *depth,
                          int *layout, int *pixel_type)
    {
        MagickImage *mi = (MagickImage *)img;
        if(!mi)return;

        if(width)       *width      = mi->width();
        if(height)      *height     = mi->height();
        if(depth)       *depth      = mi->depth();
        if(layout)      *layout     = (int)mi->layout();
        if(pixel_type)  *pixel_type = (int)mi->pixelType();
    }

    int TexImage_HasAlpha(TexImage img)
    {
        MagickImage *mi = (MagickImage *)img;
        return mi ? (mi->hasAlpha() ? 1 : 0) : 0;
    }

    int TexImage_Resize(TexImage img, uint32_t width, uint32_t height)
    {
        MagickImage *mi = (MagickImage *)img;
        if(!mi)return TEX_ERR_PARAM;

        return mi->Resize(width, height) ? TEX_OK : TEX_ERR_INTERNAL;
    }

    int TexImage_Convert(TexImage img, int layout, int pixel_type)
    {
        MagickImage *mi = (MagickImage *)img;
        if(!mi)return TEX_ERR_PARAM;

        ImagePixelType pt = ToHglPixelType(pixel_type);

        bool ok = false;

        switch((TexImageLayout)layout)
        {
            case TEX_LAYOUT_Alpha:     ok = mi->ConvertToR(pt);    break;  // 旧版语义:非Gray转Alpha,Gray保持Gray
            case TEX_LAYOUT_Gray:      ok = mi->ConvertToGray(pt); break;
            case TEX_LAYOUT_GrayAlpha: ok = mi->ConvertToRG(pt);   break;
            case TEX_LAYOUT_RGB:       ok = mi->ConvertToRGB(pt);  break;
            case TEX_LAYOUT_RGBA:      ok = mi->ConvertToRGBA(pt); break;
            default:                                                return TEX_ERR_PARAM;
        }

        return ok ? TEX_OK : TEX_ERR_UNSUPPORTED;
    }

    size_t TexImage_GetBufferSize(TexImage img, int layout, int pixel_type)
    {
        MagickImage *mi = (MagickImage *)img;
        if(!mi)return 0;

        const int channels = TexLayoutChannels(layout);
        const int bytes    = TexPixelTypeBytes(pixel_type);

        if(channels <= 0 || bytes <= 0)return 0;

        return size_t(mi->width()) * size_t(mi->height()) * size_t(channels) * size_t(bytes);
    }

    int TexImage_GetData(TexImage img, void *buf, size_t buf_size, int layout, int pixel_type)
    {
        MagickImage *mi = (MagickImage *)img;
        if(!mi || !buf)return TEX_ERR_PARAM;

        void *data = nullptr;

        switch((TexImageLayout)layout)
        {
            case TEX_LAYOUT_Alpha:     data = mi->GetAlpha(ToHglPixelType(pixel_type)); break;
            case TEX_LAYOUT_Gray:      data = mi->GetGray(ToHglPixelType(pixel_type));  break;
            case TEX_LAYOUT_GrayAlpha: data = mi->GetRG(ToHglPixelType(pixel_type));    break;
            case TEX_LAYOUT_RGB:       data = mi->GetRGB(ToHglPixelType(pixel_type));   break;
            case TEX_LAYOUT_RGBA:      data = mi->GetRGBA(ToHglPixelType(pixel_type));  break;
            default:                                                                    return TEX_ERR_PARAM;
        }

        if(!data)
            return TEX_ERR_UNSUPPORTED;

        const size_t need = TexImage_GetBufferSize(img, layout, pixel_type);

        if(buf_size < need)
        {
            delete[](uint8_t *)data;
            return TEX_ERR_PARAM;
        }

        memcpy(buf, data, need);
        delete[](uint8_t *)data;

        return TEX_OK;
    }

    int TexImage_CreateFromData(TexImage *out,
                                uint32_t width, uint32_t height,
                                int channels, int pixel_type,
                                const void *data)
    {
        if(!out || width == 0 || height == 0 || !data)
            return TEX_ERR_PARAM;

        MagickImage *img = new MagickImage;

        if(!img->CreateFromData(width, height, channels,
                                ToHglPixelType(pixel_type), data))
        {
            delete img;
            return TEX_ERR_INTERNAL;
        }

        *out = (TexImage)img;
        return TEX_OK;
    }

    int TexImage_SaveImage(const wchar_t *path,
                           uint32_t width, uint32_t height, float scale,
                           int channels, int pixel_type, const void *data)
    {
        if(!path)return TEX_ERR_PARAM;

        return SaveImageToFile(OSString((const os_char *)path),
                               width, height, scale,
                               channels, ToHglPixelType(pixel_type),
                               const_cast<void *>(data))
            ? TEX_OK : TEX_ERR_IO;
    }
}//extern "C"
