#include "ImageLog.h"

#include <windows.h>
#include <cstdio>
#include <mutex>

namespace
{
    std::mutex  g_mtx;
    TexLogFn    g_log_fn   = nullptr;
    void       *g_log_user = nullptr;
    FILE       *g_log_file = nullptr;

    /// 在 %LOCALAPPDATA%\.cmgdk\ 下新建一个不重名的日志文件(UTF-8 带 BOM)。
    FILE *CreateLogFile(const wchar_t *project_code)
    {
        wchar_t local_appdata[MAX_PATH];
        const DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", local_appdata, MAX_PATH);

        if(len == 0 || len >= MAX_PATH)
            return nullptr;

        std::wstring dir(local_appdata);
        dir += L"\\.cmgdk";
        CreateDirectoryW(dir.c_str(), nullptr);

        std::wstring base = dir;
        base += L"\\";
        base += (project_code && *project_code) ? project_code : L"TexConv";
        base += L".";
        base += std::to_wstring((unsigned long long)GetTickCount64());

        for(unsigned i = 0; i <= 0xFFFF; ++i)
        {
            wchar_t filename[MAX_PATH];
            swprintf_s(filename, L"%s.%u.log", base.c_str(), i);

            if(GetFileAttributesW(filename) != INVALID_FILE_ATTRIBUTES)
                continue;

            FILE *f = nullptr;

            if(_wfopen_s(&f, filename, L"wb") == 0 && f)
            {
                fwrite("\xEF\xBB\xBF", 1, 3, f);
                return f;
            }

            // 文件不存在却打不开 ⇒ 目录无写权限之类,不必再试下一个编号
            if(GetFileAttributesW(filename) == INVALID_FILE_ATTRIBUTES)
                return nullptr;
        }

        return nullptr;
    }

    /// 宽字符 → UTF-8
    std::string ToUTF8(const std::wstring &w)
    {
        if(w.empty())
            return std::string();

        const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                         nullptr, 0, nullptr, nullptr);

        if(n <= 0)
            return std::string();

        std::string out((size_t)n, 0);
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &out[0], n, nullptr, nullptr);
        return out;
    }
}//namespace

namespace teximage
{
    void LogInit(TexLogFn fn, void *user, const wchar_t *project_code)
    {
        std::lock_guard<std::mutex> lock(g_mtx);

        g_log_fn   = fn;
        g_log_user = user;

        if(!g_log_file)
            g_log_file = CreateLogFile(project_code);
    }

    void LogShutdown(void)
    {
        std::lock_guard<std::mutex> lock(g_mtx);

        if(g_log_file)
        {
            fclose(g_log_file);
            g_log_file = nullptr;
        }
    }

    void LogLine(int level, const char *utf8, size_t len)
    {
        std::string line;

        if(utf8 && len)
            line.assign(utf8, len);

        // 契约要求不含换行
        while(!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();

        if(line.empty())
            return;

        TexLogFn fn;
        void    *user;

        {
            std::lock_guard<std::mutex> lock(g_mtx);

            if(g_log_file)
            {
                fwrite(line.c_str(), 1, line.size(), g_log_file);
                fwrite("\n", 1, 1, g_log_file);
                fflush(g_log_file);
            }

            fn   = g_log_fn;
            user = g_log_user;
        }

        // 出锁再回调:宿主可能在回调里再次记录日志
        if(fn)
            fn(user, level, line.c_str());
    }

    void LogLine(int level, const std::string &line)
    {
        LogLine(level, line.c_str(), line.size());
    }

    void LogLine(int level, const std::wstring &line)
    {
        const std::string utf8 = ToUTF8(line);
        LogLine(level, utf8.c_str(), utf8.size());
    }
}//namespace teximage
