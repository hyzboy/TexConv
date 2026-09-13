// TexConv CLI 外壳 —— 仅做参数解析与流程编排,转换能力全部来自 TexConvCore/TexImage/编码器 DLL。
// 命令行与旧版完全兼容:
//   TexConv [/AMD|Intel] [/R:][/RG:][/RGB:][/RGBA:] [/normal] [/ColorKey:rrggbb]
//           [/s] [/mip] [/gray] [/discard_alpha] [/out:<name>] <path>
//
// 与旧版的唯一行为差异:退出码 —— 任一转换失败返回 1(旧版恒为 0)。

#include "cmdline.h"

#include <windows.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "texconv/tex_core.h"
#include "texconv/tex_formats.h"

static void LogSink(void *user, int level, const char *line)
{
    (void)user;
    (void)level;
    printf("%s\n", line);
}

static void PrintFormatList()
{
    TexFormat_PrintList([](void *user, const char *line)
    {
        printf("%s\n", line);
    }, nullptr);
}

/// 十六进制两位解析(对齐 hgl ParseHexStr)
static uint8_t hex2(const wchar_t *s)
{
    auto val = [](wchar_t c) -> int
    {
        if(c >= L'0' && c <= L'9')return int(c - L'0');
        if(c >= L'A' && c <= L'F')return int(c - L'A' + 10);
        if(c >= L'a' && c <= L'f')return int(c - L'a' + 10);
        return 0;
    };

    return uint8_t((val(s[0]) << 4) | val(s[1]));
}

/// ASCII 窄字符转换(格式名/后端名均为 ASCII)
static std::string to_narrow(const wchar_t *s)
{
    std::string r;
    while(*s)
    {
        r.push_back(char(*s));
        ++s;
    }
    return r;
}

/// 检查指定 short_name 的编码后端是否可用
static bool provider_available(const char *short_name)
{
    TexProviderInfo info[8];
    const int n = TexCore_EnumProviders(info, 8);

    for(int i = 0; i < n; i++)
    {
        const char *a = info[i].short_name;
        const char *b = short_name;

        while(*a && *b)
        {
            char ca = *a, cb = *b;
            if(ca >= 'a' && ca <= 'z')ca -= 32;
            if(cb >= 'a' && cb <= 'z')cb -= 32;
            if(ca != cb)break;
            ++a; ++b;
        }

        if(*a == *b)
            return true;
    }

    return false;
}

int wmain(int argc, wchar_t **argv)
{
    printf("Image to Texture Convert tools 1.52\n\n");

    if(argc <= 1)
    {
        printf( "Command format:\n"
                "\tTexConv [/AMD|Intel] [/R:][/RG:][/RGB:][/RGBA:] [/normal] [/ColorKey:rrggbb] [/s] [/mip] [/gray] [/discard_alpha] [/out:<new_name_without_ext>] <pathname or filename>\n"
                "\n"
                "Params:\n"
                "\t/R: /RG: /RGB: /RGBA: : target compressed format for 1/2/3/4-channel source (e.g. /RGB:BC5)\n"
                "\t/normal : normal map mode - always store as 2-channel BC5 (XY); Z is rebuilt in the shader\n"
                "\t/s : proc sub-directory\n"
                "\t/mip : generate mipmaps\n"
                "\t/gray: convert to grayscale\n"
                "\t/out: : specify new output file base name (single file mode only, extension auto set)\n"
                "\n");

        PrintFormatList();
        return 0;
    }

    CmdParse cp(argc, argv);

    TexJobParams params;
    memset(&params, 0, sizeof(params));

    const bool sub_folder    = cp.Contains(L"/s");          // 检测是否处理子目录
    params.gen_mipmaps     = cp.Contains(L"/mip");
    params.force_grayscale = cp.Contains(L"/gray");
    params.discard_alpha   = cp.Contains(L"/discard_alpha");
    params.normal_map      = cp.Contains(L"/normal");

    // 压缩后端(默认 AMD;指定的后端不可用 → fail-fast,对齐旧版 /Intel)
    const char *provider = nullptr;

    if(cp.Contains(L"/AMD"))
        provider = "AMD";
    else if(cp.Contains(L"/Intel"))
        provider = "Intel";

    params.provider = provider;

    // ColorKey(旧版仅解析未使用,保持一致)
    const wchar_t *ck = nullptr;

    if(cp.GetString(L"/ColorKey:", &ck) && ck && wcslen(ck) >= 6)
    {
        params.use_color_key = 1;
        params.color_key[0]  = hex2(ck + 0);
        params.color_key[1]  = hex2(ck + 2);
        params.color_key[2]  = hex2(ck + 4);
    }

    // 格式槽位(对齐旧 ParseParamFormat:非法格式名打印 [FORMAT ERROR] 并保持默认)
    struct SlotDef
    {
        const wchar_t *flag;
        const char    *def;
    };
    constexpr SlotDef slot_defs[4] =
    {
        {L"/R:",    "BC4"},
        {L"/RG:",   "BC5"},
        {L"/RGB:",  "BC7"},
        {L"/RGBA:", "BC7"},
    };

    std::string slot_narrow[4];

    for(int i = 0; i < 4; i++)
    {
        const wchar_t *name = nullptr;

        if(cp.GetString(slot_defs[i].flag, &name) && name && *name)
        {
            slot_narrow[i] = to_narrow(name);

            if(TexFormat_Get(slot_narrow[i].c_str()))
                params.slot_format[i] = slot_narrow[i].c_str();
            else
                printf("[FORMAT ERROR] Don't support %ls format.\n", name);
        }
    }

    // 输出基名(仅单文件模式使用)
    const wchar_t *out_base = nullptr;
    const bool has_out = cp.GetString(L"/out:", &out_base) && out_base && *out_base;

    // 初始化核心(加载编码器插件、图像库)
    TexCoreCallbacks cb;
    cb.log  = LogSink;
    cb.user = nullptr;

    if(TexCore_Init(&cb) != TEX_OK)
    {
        printf("[FATAL] TexCore init failed.\n");
        return 1;
    }

    // 指定的后端不可用 → fail-fast(对齐旧版 /Intel 缺失时直接退出)
    if(provider && !provider_available(provider))
    {
        printf("Intel ISPC texture compressor is not available in this build. Use /AMD.\n");
        TexCore_Shutdown();
        return 1;
    }

    // 旧版启动时打印所用后端(默认即 AMD)
    {
        const char *use = provider ? provider : "AMD";
        printf("Using %s Texture Compressor\n", use);
    }

    // 回显各通道槽位的实际格式(对齐旧版 std::cout 打印)
    for(int i = 0; i < 4; i++)
    {
        const char *nm;

        if(params.normal_map)
            nm = "BC5";
        else if(params.slot_format[i])
            nm = TexFormat_Get(params.slot_format[i])->name;
        else
            nm = slot_defs[i].def;

        printf("%d: %s\n", i + 1, nm);
    }

    const wchar_t *input_path = argv[argc - 1];

    const DWORD attr = GetFileAttributesW(input_path);
    const bool is_file = (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);

    int exit_code = 0;

    if(is_file)
    {
        // 单文件模式
        params.input_path  = input_path;
        params.output_path = has_out ? out_base : nullptr;

        if(TexCore_RunJob(&params, nullptr, nullptr) != TEX_OK)
            exit_code = 1;
    }
    else
    {
        // 目录枚举模式(不存在的路径 → 0 张,对齐旧 EnumFile 行为)
        if(has_out)
            printf("/out: only works in single file mode. Ignored for directory enumeration.\n");

        std::vector<std::wstring> files;

        TexCore_EnumDirectory(input_path, sub_folder ? 1 : 0,
                              [](void *user, const wchar_t *path) -> int
                              {
                                  ((std::vector<std::wstring> *)user)->push_back(path);
                                  return 0;
                              }, &files);

        const ULONGLONG start = GetTickCount64();

        uint32_t convert_count = 0;

        for(const std::wstring &f : files)
        {
            params.input_path  = f.c_str();
            params.output_path = nullptr;   // 输出写在输入同目录

            if(TexCore_RunJob(&params, nullptr, nullptr) == TEX_OK)
                ++convert_count;
            else
                exit_code = 1;
        }

        printf("Total converted %u textures for %.2f seconds.\n",
               convert_count,
               double(GetTickCount64() - start) / 1000.0);
    }

    TexCore_Shutdown();
    return exit_code;
}
