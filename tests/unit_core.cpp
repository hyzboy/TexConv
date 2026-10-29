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
#include "texconv/tex2d_container.h"

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

static void run_color_to_normal_case(const std::wstring &inputs, const std::wstring &work)
{
    ++total;
    printf("[color_to_normal]\n");

    const std::wstring input_path = inputs + L"\\rgb8.png";
    TexImageInfo source_info{};
    CHECK(TexCore_ProbeImage(input_path.c_str(), &source_info) == TEX_OK,
          "normal generation source probe");

    const std::wstring output_path = work + L"\\color_to_normal.Tex2D";
    DeleteFileW(output_path.c_str());

    TexJobParams params{};
    params.input_path = input_path.c_str();
    params.output_path = output_path.c_str();
    params.target_format = "RGBA8"; // generation must force two-channel BC5
    params.generate_normal_map = 1;

    CHECK(TexCore_RunJob(&params, nullptr, nullptr) == TEX_OK, "color to normal job");

    Tex2DInfo output_info{};
    CHECK(TexCore_ReadInfo(output_path.c_str(), &output_info) == TEX_OK,
          "normal generation output read");
    CHECK(output_info.width == source_info.width && output_info.height == source_info.height,
          "normal generation dimensions");
    CHECK(strcmp(output_info.format_name, "BC5") == 0,
          "normal generation forces BC5 output");
    CHECK(output_info.mip_levels == 1, "normal generation mip count");
}

static void run_normal_to_roughness_case(const std::wstring &inputs, const std::wstring &work)
{
    ++total;
    printf("[normal_to_roughness]\n");

    const wchar_t *inputs_to_check[] = {L"ga8.png", L"rgb8.png"};

    for(size_t i = 0; i < sizeof(inputs_to_check) / sizeof(inputs_to_check[0]); ++i)
    {
        const std::wstring input_path = inputs + L"\\" + inputs_to_check[i];
        TexImageInfo source_info{};
        CHECK(TexCore_ProbeImage(input_path.c_str(), &source_info) == TEX_OK,
              "roughness source probe");

        const std::wstring output_path = work + L"\\normal_to_roughness_"
                                       + std::to_wstring(i) + L".Tex2D";
        DeleteFileW(output_path.c_str());

        TexJobParams params{};
        params.input_path = input_path.c_str();
        params.output_path = output_path.c_str();
        params.generate_roughness_map = 1;

        CHECK(TexCore_RunJob(&params, nullptr, nullptr) == TEX_OK,
              "normal to roughness job");

        Tex2DInfo output_info{};
        CHECK(TexCore_ReadInfo(output_path.c_str(), &output_info) == TEX_OK,
              "roughness output read");
        CHECK(output_info.width == source_info.width && output_info.height == source_info.height,
              "roughness output dimensions");
        CHECK(strcmp(output_info.format_name, "BC4") == 0,
              "roughness output uses scalar BC4 format");
        CHECK(output_info.mip_levels == 1, "roughness output mip count");
    }
}

static void run_octa_case(const std::wstring &inputs, const std::wstring &work)
{
    ++total;
    printf("[cube_to_octa]\n");

    const std::wstring input_path = inputs + L"\\rgb8.png";
    TexImageInfo source_info{};
    CHECK(TexCore_ProbeImage(input_path.c_str(), &source_info) == TEX_OK, "octa source probe");

    TexCubeJobParams params{};
    for(const wchar_t *&face_path : params.face_paths)
        face_path = input_path.c_str();

    const std::wstring output_path = work + L"\\octa.Tex2D";
    params.output_path = output_path.c_str();
    params.target_format = "RGB565";

    CHECK(TexCore_RunCubeToOctaJob(&params, nullptr, nullptr) == TEX_OK, "cube to octa job");

    Tex2DInfo output_info{};
    CHECK(TexCore_ReadInfo(output_path.c_str(), &output_info) == TEX_OK, "octa output read");
    CHECK(output_info.view_type == 1, "octa output is 2D");
    CHECK(output_info.width == source_info.width * 2, "octa output width");
    CHECK(output_info.height == source_info.width * 2, "octa output height");
    CHECK(output_info.mip_levels == 1, "octa output mip count");

    TexCubeJobParams ibl_params{};
    for(const wchar_t *&face_path : ibl_params.face_paths)
        face_path = input_path.c_str();

    const std::wstring ibl_output_path = work + L"\\octa_ibl.Tex2D";
    const std::wstring irradiance_path = work + L"\\octa_ibl_irradiance.Tex2D";
    const std::wstring prefilter_path = work + L"\\octa_ibl_prefilter.Tex2D";
    DeleteFileW(ibl_output_path.c_str());
    DeleteFileW(irradiance_path.c_str());
    DeleteFileW(prefilter_path.c_str());
    ibl_params.output_path = ibl_output_path.c_str();
    ibl_params.target_format = "RGB565";
    ibl_params.ibl_mode = 1;
    ibl_params.ibl_format = "RGBA8";

    CHECK(TexCore_RunCubeToOctaJob(&ibl_params, nullptr, nullptr) == TEX_OK,
          "cube to octa with IBL job");

    Tex2DInfo irradiance_info{};
    CHECK(TexCore_ReadInfo(irradiance_path.c_str(), &irradiance_info) == TEX_OK,
          "octa IBL irradiance read");
    CHECK(irradiance_info.view_type == TEX_VIEW_2D, "octa IBL irradiance is 2D");
    CHECK(irradiance_info.width == 64 && irradiance_info.height == 64,
          "octa IBL irradiance dimensions");

    Tex2DInfo prefilter_info{};
    CHECK(TexCore_ReadInfo(prefilter_path.c_str(), &prefilter_info) == TEX_OK,
          "octa IBL prefilter read");
    CHECK(prefilter_info.view_type == TEX_VIEW_2D, "octa IBL prefilter is 2D");
    CHECK(prefilter_info.width == source_info.width * 2
          && prefilter_info.height == source_info.width * 2,
          "octa IBL prefilter dimensions");
}

static void run_laea_case(const std::wstring &inputs, const std::wstring &work)
{
    ++total;
    printf("[cube_to_laea]\n");

    const std::wstring input_path = inputs + L"\\rgb8.png";
    TexImageInfo source_info{};
    CHECK(TexCore_ProbeImage(input_path.c_str(), &source_info) == TEX_OK, "laea source probe");

    TexCubeJobParams params{};
    for(const wchar_t *&face_path : params.face_paths)
        face_path = input_path.c_str();

    const std::wstring output_path = work + L"\\laea.Tex2D";
    DeleteFileW(output_path.c_str());
    params.output_path = output_path.c_str();
    params.target_format = "RGB565";
    params.gen_mipmaps = 1;

    CHECK(TexCore_RunCubeToLambertAzimuthalJob(&params, nullptr, nullptr) == TEX_OK,
          "cube to Lambert azimuthal job");

    Tex2DInfo output_info{};
    CHECK(TexCore_ReadInfo(output_path.c_str(), &output_info) == TEX_OK, "laea output read");
    CHECK(output_info.view_type == TEX_VIEW_2D, "laea output is 2D");
    CHECK(output_info.width == source_info.width * 2, "laea output width");
    CHECK(output_info.height == source_info.width * 2, "laea output height");
    CHECK(output_info.mip_levels > 1, "laea output mip chain");

    std::vector<uint8_t> data;
    CHECK(read_file(output_path, data), "laea output file read");

    if(source_info.width >= 2 && output_info.width > 0 && output_info.height > 0)
    {
        const size_t level_bytes = size_t(output_info.width) * output_info.height * 2;

        if(data.size() >= TEX2D_HEADER_SIZE + level_bytes)
        {
            CHECK(data[TEX2D_HEADER_SIZE] == 0 && data[TEX2D_HEADER_SIZE + 1] == 0,
                  "laea outside-disk top-left pixel is zero");
            const size_t last_pixel = TEX2D_HEADER_SIZE + level_bytes - 2;
            CHECK(data[last_pixel] == 0 && data[last_pixel + 1] == 0,
                  "laea outside-disk bottom-right pixel is zero");
        }
    }
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

    run_octa_case(inputs, work);
    run_laea_case(inputs, work);
    run_color_to_normal_case(inputs, work);
    run_normal_to_roughness_case(inputs, work);

    TexCore_Shutdown();

    printf("\n%d/%d cases passed (%d assertions failed)\n",
           total - (failures ? 1 : 0) - 0, total, failures);

    // 简化:按断言失败数判断
    return failures ? 1 : 0;
}
