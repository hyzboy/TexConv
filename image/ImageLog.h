#pragma once
// TexImage.dll 的轻量日志 —— 不依赖 hgl。
//
// 每行以 UTF-8 交给宿主回调（契约见 tex_callback.h:不含换行、\0 结尾）,
// 同时写一份诊断文件到 %LOCALAPPDATA%\.cmgdk\<project>.<stamp>.<n>.log,
// 供用户机器上事后排查「no decode delegate」这类 ImageMagick 初始化问题。

#include <stddef.h>
#include <string>

#include "texconv/tex_image.h"

namespace teximage
{
    /// 绑定宿主日志回调(可为 nullptr)并创建诊断文件(project_code 可为 nullptr)。
    void LogInit(TexLogFn fn, void *user, const wchar_t *project_code);

    /// 关闭诊断文件。之后 LogLine 仍会转发给回调,但不再落盘。
    void LogShutdown(void);

    void LogLine(int level, const char *utf8, size_t len);
    void LogLine(int level, const std::string &line);
    void LogLine(int level, const std::wstring &line);

    template<typename T> inline void LogVerbose(const T &line) { LogLine(TEX_LOG_VERBOSE, line); }
    template<typename T> inline void LogInfo   (const T &line) { LogLine(TEX_LOG_INFO,    line); }
    template<typename T> inline void LogWarn   (const T &line) { LogLine(TEX_LOG_WARN,    line); }
    template<typename T> inline void LogError  (const T &line) { LogLine(TEX_LOG_ERROR,   line); }
}//namespace teximage
