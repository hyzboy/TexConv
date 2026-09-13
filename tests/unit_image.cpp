// TexImage.dll smoke test: load / info / resize / convert / getdata / save.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "texconv/tex_image.h"

static void LogSink(void *user, int level, const char *line)
{
    (void)user;
    const char *tag = level >= TEX_LOG_ERROR ? "E" : level >= TEX_LOG_WARN ? "W" : "I";
    printf("[%s] %s\n", tag, line);
}

static int failures = 0;

#define CHECK(cond, msg) do{ if(!(cond)){ printf("FAIL: %s\n", msg); ++failures; } else { printf("ok: %s\n", msg);} }while(0)

int main(int argc, char **argv)
{
    if(argc < 2)
    {
        printf("usage: unit_image <image_path>\n");
        return 2;
    }

    TexImageCallbacks cb;
    cb.log = LogSink;
    cb.user = nullptr;

    CHECK(TexImage_Init(&cb) == TEX_OK, "TexImage_Init");

    wchar_t path[1024];
    MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, path, 1024);

    TexImage img = nullptr;
    int rc = TexImage_Load(&img, path);
    CHECK(rc == TEX_OK, "TexImage_Load");

    if(rc == TEX_OK)
    {
        uint32_t w = 0, h = 0, depth = 0;
        int layout = -1, pt = -1;
        TexImage_GetInfo(img, &w, &h, &depth, &layout, &pt);
        printf("info: %ux%u depth=%u layout=%d pixel_type=%d alpha=%d\n",
               w, h, depth, layout, pt, TexImage_HasAlpha(img));
        CHECK(w > 0 && h > 0, "dims valid");

        CHECK(TexImage_Resize(img, 16, 16) == TEX_OK, "Resize 16x16");
        TexImage_GetInfo(img, &w, &h, nullptr, nullptr, nullptr);
        CHECK(w == 16 && h == 16, "dims after resize");

        CHECK(TexImage_Convert(img, TEX_LAYOUT_RGBA, TEX_PT_UInt8) == TEX_OK, "Convert RGBA8");
        size_t need = TexImage_GetBufferSize(img, TEX_LAYOUT_RGBA, TEX_PT_UInt8);
        CHECK(need == size_t(16) * 16 * 4, "RGBA8 buffer size");
        static uint8_t buf[16 * 16 * 4];
        CHECK(TexImage_GetData(img, buf, sizeof(buf), TEX_LAYOUT_RGBA, TEX_PT_UInt8) == TEX_OK,
              "GetData RGBA8");

        CHECK(TexImage_Convert(img, TEX_LAYOUT_Gray, TEX_PT_UInt8) == TEX_OK, "Convert Gray8");
        need = TexImage_GetBufferSize(img, TEX_LAYOUT_Gray, TEX_PT_UInt8);
        CHECK(need == size_t(16) * 16, "Gray8 buffer size");

        wchar_t out[1024];
        MultiByteToWideChar(CP_UTF8, 0, "unit_image_out.png", -1, out, 1024);
        CHECK(TexImage_SaveImage(out, 16, 16, 1.0f, 3, TEX_PT_UInt8, buf) == TEX_OK, "SaveImage");
    }

    TexImage_Shutdown();

    if(failures)
        printf("\n%d FAILURES\n", failures);
    else
        printf("\nALL PASS\n");
    return failures ? 1 : 0;
}
