// TexConvCore.dll —— 模块生命周期 / 插件扫描 / 查询接口。

#include "internal.h"

#include <filesystem>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

namespace texcore
{
    static TexLogFn     g_log_fn    = nullptr;
    static void        *g_log_user  = nullptr;
    static bool         g_inited    = false;

    void CoreLog(int level, const std::string &utf8_line)
    {
        if(g_log_fn)
            g_log_fn(g_log_user, level, utf8_line.c_str());
    }

    std::vector<Provider> &Providers()
    {
        static std::vector<Provider> list;
        return list;
    }

    /// 扫描本 DLL 所在目录下的 texenc*.dll
    static void ScanPlugins()
    {
        wchar_t self[MAX_PATH];
        if(!GetModuleFileNameW((HMODULE)&__ImageBase, self, MAX_PATH))
            return;

        std::filesystem::path dir = std::filesystem::path(self).parent_path();

        {
            int n = WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string dbg(n > 0 ? n - 1 : 0, 0);
            if(n > 0)
                WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), -1, dbg.data(), n, nullptr, nullptr);
            CoreLog(TEX_LOG_INFO, "plugin scan dir: " + dbg);
        }

        WIN32_FIND_DATAW fd;
        HANDLE find = FindFirstFileW((dir / L"texenc*.dll").c_str(), &fd);

        if(find == INVALID_HANDLE_VALUE)
        {
            CoreLog(TEX_LOG_WARN, "plugin scan: no texenc*.dll found ("
                                  + std::to_string(GetLastError()) + ")");
            return;
        }

        do
        {
            std::filesystem::path dll = dir / fd.cFileName;

            HMODULE mod = LoadLibraryW(dll.c_str());

            if(!mod)
            {
                CoreLog(TEX_LOG_WARN, "failed to load encoder plugin: "
                                      + std::filesystem::path(fd.cFileName).string());
                continue;
            }

            auto get = (TexGetEncoderProviderFn)GetProcAddress(mod, "TexGetEncoderProvider");

            if(!get)
            {
                CoreLog(TEX_LOG_WARN, "encoder plugin has no TexGetEncoderProvider: "
                                      + std::filesystem::path(fd.cFileName).string());
                FreeLibrary(mod);
                continue;
            }

            const TexEncoderProvider *provider = get();

            if(!provider || provider->abi_version != TEXENC_ABI_VERSION)
            {
                CoreLog(TEX_LOG_WARN, "encoder plugin abi mismatch: "
                                      + std::filesystem::path(fd.cFileName).string());
                FreeLibrary(mod);
                continue;
            }

            Provider p;
            p.module      = mod;
            p.provider    = provider;
            p.inited      = false;

            {
                // 供 GUI 展示的模块文件名(UTF-8 稳定存储)
                std::wstring fn(fd.cFileName);
                int n = WideCharToMultiByte(CP_UTF8, 0, fn.c_str(), int(fn.size()),
                                            nullptr, 0, nullptr, nullptr);
                p.utf8_file.resize(n);
                WideCharToMultiByte(CP_UTF8, 0, fn.c_str(), int(fn.size()),
                                    p.utf8_file.data(), n, nullptr, nullptr);
            }

            Providers().push_back(p);

            CoreLog(TEX_LOG_INFO, std::string("encoder plugin loaded: ") + p.utf8_file);
        }
        while(FindNextFileW(find, &fd));

        FindClose(find);
    }
}//namespace texcore

extern "C"
{
    int TexCore_Init(const TexCoreCallbacks *cb)
    {
        if(texcore::g_inited)
            return TEX_OK;

        if(!texcore::Providers().empty())
            return TEX_OK;      // 已扫描过(理论上不会发生,防御重复 Init)

        if(cb)
        {
            texcore::g_log_fn   = cb->log;
            texcore::g_log_user = cb->user;
        }

        // 图像库(日志路由到回调)
        TexImageCallbacks icb;
        icb.log  = cb ? cb->log  : nullptr;
        icb.user = cb ? cb->user : nullptr;

        int rc = TexImage_Init(&icb);

        if(rc != TEX_OK)
            return rc;

        texcore::ScanPlugins();

        texcore::g_inited = true;
        return TEX_OK;
    }

    void TexCore_Shutdown(void)
    {
        for(auto &p : texcore::Providers())
        {
            if(p.inited && p.provider)
                p.provider->Shutdown();

            if(p.module)
                FreeLibrary(p.module);
        }

        texcore::Providers().clear();
        TexImage_Shutdown();
        texcore::g_inited = false;
    }

    int TexCore_EnumProviders(TexProviderInfo *out, int out_count)
    {
        const auto &list = texcore::Providers();

        if(out && out_count > 0)
        {
            const int n = (out_count < int(list.size())) ? out_count : int(list.size());

            for(int i = 0; i < n; i++)
            {
                out[i].name        = list[i].provider->name;
                out[i].short_name  = list[i].provider->short_name;
                out[i].module_file = list[i].utf8_file.c_str();
            }
        }

        return int(list.size());
    }

    int TexCore_ProbeImage(const wchar_t *path, TexImageInfo *out)
    {
        if(!path || !out)
            return TEX_ERR_PARAM;

        TexImage img = nullptr;
        int rc = TexImage_Load(&img, path);

        if(rc != TEX_OK)
            return rc;

        uint32_t w = 0, h = 0, depth = 0;
        int layout = 0, pt = 0;

        TexImage_GetInfo(img, &w, &h, &depth, &layout, &pt);

        out->width      = w;
        out->height     = h;
        out->depth      = depth;
        out->layout     = layout;
        out->pixel_type = pt;
        out->channels   = TexLayoutChannels(layout);
        out->has_alpha  = TexImage_HasAlpha(img);

        TexImage_Free(img);
        return TEX_OK;
    }

    int TexCore_EnumDirectory(const wchar_t *dir, int recursive,
                              int (*cb)(void *user, const wchar_t *path),
                              void *user)
    {
        if(!dir || !cb)
            return TEX_ERR_PARAM;

        namespace fs = std::filesystem;
        fs::path root(dir);

        if(!fs::is_directory(root))
            return TEX_ERR_PARAM;

        int count = 0;
        const auto lower_ext = [](const fs::path &p)
        {
            std::wstring ext = p.extension().wstring();
            for(wchar_t &c : ext)
                c = (c >= L'A' && c <= L'Z') ? c + 32 : c;
            return ext;
        };

        auto emit = [&](const fs::directory_entry &entry)
        {
            if(!entry.is_regular_file())
                return;

            if(lower_ext(entry.path()) == L".tex2d")    // 已是纹理文件,跳过(对齐旧 EnumConvertImage)
                return;

            if(cb(user, entry.path().c_str()))
                count = -count - 1;     // 回调要求停止

            ++count;
        };

        std::error_code ec;

        if(recursive)
        {
            for(fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
                !ec && it != fs::recursive_directory_iterator();
                it.increment(ec))
            {
                if(count < 0)break;
                emit(*it);
            }
        }
        else
        {
            for(fs::directory_iterator it(root, ec);
                !ec && it != fs::directory_iterator();
                it.increment(ec))
            {
                if(count < 0)break;
                emit(*it);
            }
        }

        if(count < 0)
            count = -count - 1;

        return count;
    }

    int TexCore_RunJob(const TexJobParams *params, TexProgressFn progress, void *user)
    {
        return texcore::RunJobImpl(params, progress, user);
    }

    int TexCore_ReadInfo(const wchar_t *tex2d_path, Tex2DInfo *out)
    {
        if(!tex2d_path || !out)
            return TEX_ERR_PARAM;

        FILE *f = nullptr;

        if(_wfopen_s(&f, tex2d_path, L"rb") != 0 || !f)
            return TEX_ERR_IO;

        uint8_t header[TEX2D_HEADER_SIZE];
        size_t got = fread(header, 1, sizeof(header), f);

        _fseeki64(f, 0, SEEK_END);
        int64_t file_size = _ftelli64(f);
        fclose(f);

        if(got != sizeof(header))
            return TEX_ERR_IO;

        if(memcmp(header, TEX2D_HEADER_MAGIC, TEX2D_HEADER_MAGIC_LEN) != 0)
            return TEX_ERR_IO;

        memset(out, 0, sizeof(*out));

        out->view_type = header[8];
        out->file_size = file_size;

        if(out->view_type == TEX_VIEW_2D || out->view_type == TEX_VIEW_CUBE)
        {
            memcpy(&out->width,  header + 9,  4);
            memcpy(&out->height, header + 13, 4);
        }
        else
        {
            // 其它视图类型的大小字段布局不同,仅给出原始值
            memcpy(&out->width,  header + 9,  4);
        }

        const uint8_t first = header[21];

        if(first == 0)      // 压缩:0 + 9 字节格式名
        {
            out->is_compress = 1;
            memcpy(out->format_name, header + 22, 9);
            out->format_name[9] = 0;
        }
        else                // 非压缩:通道数 + color[4] + bits[4] + vulkan_type
        {
            out->is_compress = 0;
            out->channels    = first;
            memcpy(out->bits, header + 26, 4);

            // 可读名:通道标记拼接(如 RGBA)
            int n = 0;
            for(int i = 0; i < 4; i++)
                if(header[22 + i])
                    out->format_name[n++] = char(header[22 + i]);
            out->format_name[n] = 0;

            out->vulkan_type = header[30];
        }

        out->mip_levels = header[31];
        return TEX_OK;
    }
}//extern "C"
