// TexConvCore end-to-end parity test.
//   usage: unit_core <inputs_dir> <ref_full_dir>
// Runs a matrix of jobs through TexCore_RunJob and byte-compares each .Tex2D
// with the reference file produced by the pre-refactor exe.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "texconv/tex_core.h"

static int failures = 0;
static int total = 0;

#define CHECK(cond, msg) do{ if(!(cond)){ printf("  FAIL: %s\n", msg); ++failures; } }while(0)

static void LogSink(void *user, int level, const char *line)
{
    (void)user;
    (void)level;
    printf("    [log] %s\n", line);
}

static std::wstring to_wide(const char *s)
{
    std::wstring r;
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if(n > 0) { r.resize(n - 1); MultiByteToWideChar(CP_UTF8, 0, s, -1, r.data(), n); }
    return r;
}

static bool read_file(const std::wstring &path, std::vector<uint8_t> &data)
{
    FILE *f = nullptr;
    if(_wfopen_s(&f, path.c_str(), L"rb") != 0 || !f)return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    data.resize(size_t(size));
    bool ok = size == 0 || fread(data.data(), 1, size_t(size), f) == size_t(size);
    fclose(f);
    return ok;
}

struct Case
{
    const char *ref_name;
    const wchar_t *input;
    const char *target_format;      // NULL = 槽位默认
    int gen_mipmaps;
    int force_grayscale;
    int normal_map;
    const char *provider;
};

static void run_case(const std::wstring &inputs, const std::wstring &ref_dir,
                     const std::wstring &work, const Case &c)
{
    ++total;
    printf("[%s]\n", c.ref_name);

    TexJobParams p;
    memset(&p, 0, sizeof(p));

    const std::wstring input_path = inputs + L"\\" + std::wstring(c.input);

    p.input_path      = input_path.c_str();
    p.target_format   = c.target_format;
    p.gen_mipmaps     = c.gen_mipmaps;
    p.force_grayscale = c.force_grayscale;
    p.normal_map      = c.normal_map;
    p.provider        = c.provider;

    std::wstring out_path = work + L"\\out.Tex2D";
    DeleteFileW(out_path.c_str());
    p.output_path = out_path.c_str();

    int rc = TexCore_RunJob(&p, nullptr, nullptr);
    CHECK(rc == TEX_OK, "RunJob");

    std::vector<uint8_t> got, want;
    bool got_ok = read_file(out_path, got);
    bool want_ok = read_file(ref_dir + L"\\" + to_wide(c.ref_name), want);

    CHECK(got_ok, "output exists");
    CHECK(want_ok, "reference exists");

    if(got_ok && want_ok)
    {
        CHECK(got.size() == want.size(), "size matches");
        if(got.size() == want.size())
            CHECK(memcmp(got.data(), want.data(), got.size()) == 0, "bytes match");
    }

    printf(got.size() == want.size() ? "  size: %zu ok\n" : "  size: got %zu want %zu\n",
           got.size(), want.size());
}

int main(int argc, char **argv)
{
    if(argc < 3)
    {
        printf("usage: unit_core <inputs_dir> <ref_full_dir>\n");
        return 2;
    }

    std::wstring inputs, ref_dir, work;
    {
        wchar_t buf[1024];
        MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, buf, 1024); inputs = buf;
        MultiByteToWideChar(CP_UTF8, 0, argv[2], -1, buf, 1024); ref_dir = buf;
        work = ref_dir + L"\\_work";
        CreateDirectoryW(work.c_str(), nullptr);
    }

    TexCoreCallbacks cb;
    cb.log = LogSink;
    cb.user = nullptr;

    CHECK(TexCore_Init(&cb) == TEX_OK, "TexCore_Init");

    TexProviderInfo providers[8];
    int n = TexCore_EnumProviders(providers, 8);
    printf("providers: %d\n", n);
    for(int i = 0; i < n; i++)
        printf("  [%d] %s (%s) via %s\n", i, providers[i].name,
               providers[i].short_name, providers[i].module_file);

    const wchar_t *rgb8   = L"rgb8.png";
    const wchar_t *rgba8  = L"rgba8.tga";
    const wchar_t *g8     = L"g8.png";
    const wchar_t *ga8    = L"ga8.png";

    const Case cases[] =
    {
        {"amd_bc7_3ch.Tex2D",     rgb8,  "BC7",      0, 0, 0, nullptr},
        {"intel_bc7_3ch.Tex2D",   rgb8,  "BC7",      0, 0, 0, "Intel"},
        {"amd_bc1rgba_3ch.Tex2D", rgb8,  "BC1RGBA",  0, 0, 0, nullptr},
        {"amd_bc3_4ch.Tex2D",     rgba8, "BC3",      0, 0, 0, nullptr},
        {"rgba8_4ch.Tex2D",       rgba8, "RGBA8",    0, 0, 0, nullptr},
        {"r8_1ch.Tex2D",          g8,    "R8",       0, 0, 0, nullptr},
        {"rgb565_3ch.Tex2D",      rgb8,  "RGB565",   0, 0, 0, nullptr},
        {"rgb32f_3ch.Tex2D",      rgb8,  "RGB32F",   0, 0, 0, nullptr},
        {"bc7_mip.Tex2D",         rgb8,  "BC7",      1, 0, 0, nullptr},
        {"rgba16f_mip.Tex2D",     rgba8, "RGBA16F",  1, 0, 0, nullptr},
        {"gray_rg_2ch.Tex2D",     ga8,   nullptr,    0, 1, 0, nullptr},
        {"normal_bc5.Tex2D",      rgb8,  nullptr,    0, 0, 1, nullptr},
    };

    // argv[3]/argv[4] = 可选:只跑 [from,to] 下标区间(0-based),用于隔离排查
    int from = (argc > 3) ? atoi(argv[3]) : 0;
    int to   = (argc > 4) ? atoi(argv[4]) : int(sizeof(cases)/sizeof(cases[0])) - 1;

    for(int ci = 0; ci < int(sizeof(cases)/sizeof(cases[0])); ci++)
    {
        if(ci < from || ci > to)continue;
        run_case(inputs, ref_dir, work, cases[ci]);
    }

    TexCore_Shutdown();

    printf("\n%d/%d cases passed (%d assertions failed)\n",
           total - (failures ? 1 : 0) - 0, total, failures);

    // 简化:按断言失败数判断
    return failures ? 1 : 0;
}
