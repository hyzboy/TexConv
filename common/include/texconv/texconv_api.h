#pragma once
// TexConv DLL 导出宏。
// 构建 DLL 的模块在 target 上定义 TEXCONV_SHARED_BUILD,消费方定义 TEXCONV_SHARED_USE。

#if defined(_WIN32)
    #if defined(TEXCONV_SHARED_BUILD)
        #define TEXCONV_API __declspec(dllexport)
    #elif defined(TEXCONV_SHARED_USE)
        #define TEXCONV_API __declspec(dllimport)
    #else
        #define TEXCONV_API
    #endif
#else
    #define TEXCONV_API __attribute__((visibility("default")))
#endif
