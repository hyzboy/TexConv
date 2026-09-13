// Encoder plugin byte-parity test.
//   usage: unit_encoders <input_image> <ref_payload> <format> <provider_dll>
// Loads image via TexImage, encodes via provider, compares bytes with ref payload
// (produced by the pre-refactor exe, 32-byte header stripped).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include "texconv/tex_image.h"
#include "texconv/texenc_plugin.h"
#include "texconv/tex_formats.h"

typedef const TexEncoderProvider *(*TexGetEncoderProviderFn)(void);

static int failures = 0;
#define CHECK(cond, msg) do{ if(!(cond)){ printf("FAIL: %s\n", msg); ++failures; } }while(0)

static bool read_file(const char *path, std::vector<uint8_t> &data)
{
    FILE *f = fopen(path, "rb");
    if(!f)return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    data.resize(size_t(size));
    bool ok = size == 0 || fread(data.data(), 1, size_t(size), f) == size_t(size);
    fclose(f);
    return ok;
}

int main(int argc, char **argv)
{
    if(argc < 5)
    {
        printf("usage: unit_encoders <image> <ref_payload> <format> <provider_dll>\n");
        return 2;
    }

    const char *image_path = argv[1];
    const char *ref_path   = argv[2];
    const char *format     = argv[3];
    const char *dll_path   = argv[4];

    std::vector<uint8_t> ref_payload;
    CHECK(read_file(ref_path, ref_payload), "read ref payload");
    printf("ref payload: %zu bytes\n", ref_payload.size());

    HMODULE mod = LoadLibraryA(dll_path);
    CHECK(mod != nullptr, "LoadLibrary provider");
    if(!mod)return 1;

    auto get_provider = (TexGetEncoderProviderFn)GetProcAddress(mod, "TexGetEncoderProvider");
    CHECK(get_provider != nullptr, "GetProcAddress TexGetEncoderProvider");
    if(!get_provider)return 1;

    const TexEncoderProvider *provider = get_provider();
    CHECK(provider != nullptr, "provider != null");
    CHECK(provider->abi_version == TEXENC_ABI_VERSION, "abi version");
    printf("provider: %s (%s)\n", provider->name, provider->short_name);

    CHECK(provider->Init() == TEX_OK, "provider Init");

    TexImageCallbacks cb;
    cb.log = nullptr;
    cb.user = nullptr;
    CHECK(TexImage_Init(&cb) == TEX_OK, "TexImage_Init");

    wchar_t wpath[1024];
    MultiByteToWideChar(CP_UTF8, 0, image_path, -1, wpath, 1024);

    TexImage img = nullptr;
    CHECK(TexImage_Load(&img, wpath) == TEX_OK, "TexImage_Load");

    uint32_t w = 0, h = 0;
    int layout = 0, pt = 0;
    TexImage_GetInfo(img, &w, &h, nullptr, &layout, &pt);
    printf("image: %ux%u layout=%d pt=%d\n", w, h, layout, pt);

    // 由缓冲大小反推源通道数
    size_t cur_buf = TexImage_GetBufferSize(img, layout, pt);
    int src_channels = int(cur_buf / (size_t(w) * h * size_t(TexPixelTypeBytes(pt))));
    printf("source channels: %d\n", src_channels);

    int req_layout = -1, req_pt = -1;
    int qr = provider->QuerySourceLayout(format, src_channels, pt, &req_layout, &req_pt);
    CHECK(qr == TEX_OK, "QuerySourceLayout");

    if(qr == TEX_OK)
    {
        if(req_layout != layout || req_pt != pt)
            CHECK(TexImage_Convert(img, req_layout, req_pt) == TEX_OK, "Convert to required layout");

        std::vector<uint8_t> src(TexImage_GetBufferSize(img, req_layout, req_pt));
        CHECK(TexImage_GetData(img, src.data(), src.size(), req_layout, req_pt) == TEX_OK,
              "GetData");

        TexEncodeRequest req;
        memset(&req, 0, sizeof(req));
        req.src            = src.data();
        req.width          = w;
        req.height         = h;
        req.src_layout     = req_layout;
        req.src_pixel_type = req_pt;
        req.target_format  = format;
        req.quality        = 100;
        req.thread_count   = 0;

        uint8_t *out_data = nullptr;
        size_t   out_bytes = 0;
        int er = provider->Encode(&req, &out_data, &out_bytes);
        CHECK(er == TEX_OK, "Encode");
        printf("encoded: %zu bytes\n", out_bytes);

        if(er == TEX_OK)
        {
            CHECK(out_bytes == ref_payload.size(), "payload size matches old exe");
            if(out_bytes == ref_payload.size() && out_bytes > 0)
                CHECK(memcmp(out_data, ref_payload.data(), out_bytes) == 0,
                      "payload bytes match old exe");

            provider->FreeResult(out_data);
        }
    }

    provider->Shutdown();
    TexImage_Free(img);
    TexImage_Shutdown();
    FreeLibrary(mod);

    if(failures)
        printf("\n%d FAILURES\n", failures);
    else
        printf("\nALL PASS\n");
    return failures ? 1 : 0;
}
