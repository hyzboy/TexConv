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

/// 面签名:主文件名首字母 P/N + 尾字母 X/Y/Z(大小写不敏感)→ 面序 0..5(+X,-X,+Y,-Y,+Z,-Z)
static int face_index_from_name(const std::wstring &name)
{
    if(name.size() < 2)return -1;

    const wchar_t f = towlower(name.front());
    const wchar_t l = towlower(name.back());

    const int sign = (f == L'p') ? 0 : (f == L'n') ? 1 : -1;
    const int axis = (l == L'x') ? 0 : (l == L'y') ? 1 : (l == L'z') ? 2 : -1;

    if(sign < 0 || axis < 0)return -1;

    return axis * 2 + sign;
}

static int cube_face_index(const std::wstring &path)
{
    std::wstring name = path;

    const size_t sep = name.find_last_of(L"/\\");
    if(sep != std::wstring::npos)name = name.substr(sep + 1);

    const size_t dot = name.find_last_of(L'.');
    if(dot != std::wstring::npos)name = name.substr(0, dot);

    // 规则 1:整个主文件名的 首字母+尾字母(PosX / NY 等直接命中)
    const int direct = face_index_from_name(name);
    if(direct >= 0)return direct;

    // 规则 2(兜底):首 token 或末 token 是面缩写 —— 2 字符(PX/NY/...)
    // 或 4 字符(PosX/NegZ/...),避免 noisy 之类误含 n..y 的词误报
    std::wstring first_token, last_token;
    std::wstring token;

    for(size_t i = 0; i <= name.size(); i++)
    {
        const wchar_t ch = (i < name.size()) ? name[i] : L'_';

        if(ch == L'_' || ch == L'-' || ch == L'.' || ch == L' ')
        {
            if(!token.empty())
            {
                if(first_token.empty())first_token = token;
                last_token = token;
                token.clear();
            }
        }
        else
        {
            token += towlower(ch);
        }
    }

    auto token_face = [](const std::wstring &t) -> int
    {
        if(t.size() == 2)
        {
            const int sign = (t[0] == L'p') ? 0 : (t[0] == L'n') ? 1 : -1;
            const int axis = (t[1] == L'x') ? 0 : (t[1] == L'y') ? 1 : (t[1] == L'z') ? 2 : -1;
            return (sign >= 0 && axis >= 0) ? axis * 2 + sign : -1;
        }

        if(t.size() == 4)
        {
            const int sign = (t.compare(0, 3, L"pos") == 0) ? 0
                           : (t.compare(0, 3, L"neg") == 0) ? 1 : -1;
            const int axis = (t[3] == L'x') ? 0 : (t[3] == L'y') ? 1 : (t[3] == L'z') ? 2 : -1;
            return (sign >= 0 && axis >= 0) ? axis * 2 + sign : -1;
        }

        return -1;
    };

    if(!first_token.empty())
    {
        const int r = token_face(first_token);
        if(r >= 0)return r;

    }

    if(!last_token.empty())
    {
        const int r = token_face(last_token);
        if(r >= 0)return r;
    }

    return -1;
}

/// 6 个面文件名的最长公共前缀(去尾部分隔符),作为输出基名
static std::wstring cube_common_prefix(const std::wstring paths[6])
{
    std::wstring bases[6];

    for(int i = 0; i < 6; i++)
    {
        std::wstring n = paths[i];
        const size_t sep = n.find_last_of(L"/\\");
        if(sep != std::wstring::npos)n = n.substr(sep + 1);
        const size_t dot = n.find_last_of(L'.');
        if(dot != std::wstring::npos)n = n.substr(0, dot);
        bases[i] = n;
    }

    std::wstring p = bases[0];

    bool changed = true;

    while(changed)
    {
        changed = false;

        for(int i = 1; i < 6; i++)
        {
            if(_wcsnicmp(p.c_str(), bases[i].c_str(), p.size()) != 0)
            {
                p.pop_back();
                changed = true;
                break;
            }
        }
    }

    while(!p.empty() && (p.back() == L'_' || p.back() == L'-' || p.back() == L' ' || p.back() == L'.'))
        p.pop_back();

    return p;
}

/// /cube 模式:inputs 为 6 个面文件,或 1 个目录(目录内按签名自动分组,可多组)
static int run_cube(const std::vector<std::wstring> &inputs,
                    const char *const slot_names_in[4],
                    TexJobParams &ref_params,
                    const wchar_t *out_base,
                    bool gen_mipmaps,
                    const char *provider)
{
    std::vector<std::wstring> faces;

    if(inputs.size() == 1)
    {
        // 目录扫描(/cube 忽略 /s,恒递归;内核跳过 .Tex2D)
        TexCore_EnumDirectory(inputs[0].c_str(), 1,
                              [](void *user, const wchar_t *path) -> int
                              {
                                  static_cast<std::vector<std::wstring> *>(user)->push_back(path);
                                  return 0;
                              }, &faces);
    }
    else
    {
        faces = inputs;
    }

    if(faces.size() < 6)
    {
        printf("[CUBE] need 6 face files (PosX/PosY/PosZ/NegX/NegY/NegZ or PX/PY/PZ/NX/NY/NZ), got %zu.\n",
               faces.size());
        return 1;
    }

    // 分组:目录 + 面签名
    struct CubeGroup
    {
        std::wstring dir;
        std::wstring paths[6];
        int count = 0;
    };

    std::vector<CubeGroup> groups;
    size_t used = 0;

    for(const std::wstring &f : faces)
    {
        const int face = cube_face_index(f);

        if(face < 0)continue;      // 非面文件,忽略

        std::wstring dir = f;
        const size_t sep = dir.find_last_of(L"/\\");
        dir = (sep == std::wstring::npos) ? std::wstring(L".") : dir.substr(0, sep);

        bool placed = false;

        for(CubeGroup &g : groups)
        {
            if(g.dir != dir)continue;
            if(g.paths[face].empty())
            {
                g.paths[face] = f;
                g.count++;
                placed = true;
                break;
            }
        }

        if(!placed)
        {
            CubeGroup g;
            g.dir = dir;
            g.paths[face] = f;
            g.count = 1;
            groups.push_back(g);
        }

        used++;
    }

    int exit_code = 0;
    int group_index = 0;

    for(CubeGroup &g : groups)
    {
        if(g.count < 6)
        {
            printf("[CUBE] skip incomplete group in %ls (%d/6 faces).\n", g.dir.c_str(), g.count);
            exit_code = 1;
            continue;
        }

        ++group_index;

        // 输出名:/out: 优先(单组);否则公共前缀 + .TexCube
        std::wstring output;

        if(out_base && *out_base && groups.size() == 1)
            output = out_base;
        else
            output = g.dir + L"\\" + cube_common_prefix(g.paths);

        output += L".TexCube";

        // 面通道数 → 目标格式(显式槽位优先,否则内核默认)
        TexImageInfo info{};
        const int rc = TexCore_ProbeImage(g.paths[0].c_str(), &info);

        const char *target = nullptr;

        if(rc == TEX_OK && info.channels >= 1 && info.channels <= 4)
            target = slot_names_in[info.channels - 1];

        TexCubeJobParams cube{};
        for(int i = 0; i < 6; i++)
            cube.face_paths[i] = g.paths[i].c_str();

        cube.output_path   = output.c_str();
        cube.target_format = target;
        cube.provider      = provider;
        cube.gen_mipmaps   = gen_mipmaps ? 1 : 0;

        constexpr const char *face_name[6] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};

        for(int i = 0; i < 6; i++)
            printf("    %s: %ls\n", face_name[i], g.paths[i].c_str());

        printf("output: %ls\n", output.c_str());

        if(TexCore_RunCubeJob(&cube, nullptr, nullptr) != TEX_OK)
        {
            printf("[CUBE] group %d convert failed.\n", group_index);
            exit_code = 1;
        }
    }

    (void)ref_params;
    return exit_code;
}

int wmain(int argc, wchar_t **argv)
{
    printf("Image to Texture Convert tools 1.52\n\n");

    if(argc <= 1)
    {
        printf( "Command format:\n"
                "\tTexConv [/AMD|Intel] [/R:][/RG:][/RGB:][/RGBA:] [/normal] [/ColorKey:rrggbb] [/s] [/mip] [/gray] [/discard_alpha] [/DF[:threshold]] [/out:<new_name_without_ext>] <pathname or filename>\n"
                "\n"
                "Params:\n"
                "\t/R: /RG: /RGB: /RGBA: : target compressed format for 1/2/3/4-channel source (e.g. /RGB:BC5)\n"
                "\t/normal : normal map mode - always store as 2-channel BC5 (XY); Z is rebuilt in the shader\n"
                "\t/s : proc sub-directory\n"
                "\t/mip : generate mipmaps\n"
                "\t/gray: convert to grayscale\n"
                "\t/DF[:threshold] : generate distance field then save\n"
                "\t                    (1-channel: from gray; RGBA/GrayAlpha: from alpha;\n"
                "\t                     result is a single-channel texture, default slot R8)\n"
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

    // 距离场模式:/DF 开启;/DF:threshold 指定内外判定阈值(默认 128)
    params.df_mode = cp.Contains(L"/DF");

    if(params.df_mode)
    {
        const wchar_t *dft = nullptr;

        if(cp.GetString(L"/DF:", &dft) && dft && *dft)
            params.df_threshold = int(wcstol(dft, nullptr, 10));
    }

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
        else if(i == 0 && params.df_mode)
            nm = "R8";      // DF 模式 1 通道默认 R8(见 tex_core.h/DEPLOY 说明)
        else
            nm = slot_defs[i].def;

        printf("%d: %s\n", i + 1, nm);
    }

    // Cubemap 模式:/cube(输入为 6 个面文件或 1 个目录)
    if(cp.Contains(L"/cube"))
    {
        std::vector<std::wstring> inputs;

        for(int i = 1; i < argc; i++)
            if(argv[i][0] != L'/')
                inputs.push_back(argv[i]);

        if(inputs.empty())
        {
            printf("[CUBE] no input. usage: TexConv /cube [/mip] [/R:/RG:/RGB:/RGBA:] [/out:name] <6 face files | directory>\n");
            TexCore_Shutdown();
            return 1;
        }

        const int rc = run_cube(inputs, params.slot_format, params,
                                (has_out ? out_base : nullptr),
                                params.gen_mipmaps, provider);

        TexCore_Shutdown();
        return rc;
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
