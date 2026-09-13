#pragma once
// 各模块共用的回调与日志接口。
// 所有回调都在调用方线程上被调用;日志行 UTF-8 编码,以 \0 结尾,不含换行。

#include "texconv_api.h"

#ifdef __cplusplus
extern "C" {
#endif

enum TexLogLevel
{
    TEX_LOG_VERBOSE = 0,
    TEX_LOG_INFO,
    TEX_LOG_WARN,
    TEX_LOG_ERROR,
};

/// 日志回调。user 为调用方上下文;line 指向的缓冲仅在回调期间有效。
typedef void (*TexLogFn)(void *user, int level, const char *utf8_line);

/// 进度回调。fraction 取值 0.0~1.0;
/// 返回 0 继续,返回非 0 请求取消(取消后操作以 TEX_ERR_CANCELLED 结束)。
typedef int (*TexProgressFn)(void *user, float fraction);

#ifdef __cplusplus
}
#endif
