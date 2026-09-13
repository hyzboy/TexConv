// DistanceField unit test.
//   usage: unit_df
// 1) TexDF_Generate exact-value checks on a synthetic pattern (64x64, bright
//    square cols/rows 20..43, threshold 128, scale=3 bias=128):
//      center (32,32) inside:  dist=12  -> 12*3+128 = 164
//      corner  (0,0)  outside: dist=sqrt(400+400)=28.28.. ->28 -> 128-84 = 44
//      edge    (20,32) inside: d_dark=1,d_bright=0 -> 1*3+128 = 131
//      edge    (19,32) outside:d_dark=0,d_bright=1 -> 128-3   = 125
// 2) TexImage_CreateFromData orientation round-trip (first row stays first).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include "texconv/tex_distance_field.h"
#include "texconv/tex_image.h"

static int failures = 0;
#define CHECK(cond, msg) do{ if(!(cond)){ printf("FAIL: %s\n", msg); ++failures; } }while(0)

static const uint32_t W = 64, H = 64;

static void make_square(std::vector<uint8_t> &img)
{
    img.assign(size_t(W) * H, 0);

    for(uint32_t y = 20; y <= 43; y++)
        for(uint32_t x = 20; x <= 43; x++)
            img[y * W + x] = 255;
}

int main()
{
    // ---------------- DF 算法 ----------------
    std::vector<uint8_t> src, dst(size_t(W) * H);
    make_square(src);

    CHECK(TexDF_Generate(src.data(), dst.data(), W, H, 128, 0, 0) == TEX_OK, "TexDF_Generate");

    CHECK(dst[32 * W + 32] == 164, "center inside value (164)");
    CHECK(dst[0 * W + 0] == 44, "corner outside value (44)");
    CHECK(dst[32 * W + 20] == 131, "edge inside value (131)");
    CHECK(dst[32 * W + 19] == 125, "edge outside value (125)");

    // 自定义阈值:阈值 200 时 255 仍是内部,100 图案变外部
    {
        std::vector<uint8_t> s2(size_t(W) * H, 100), d2(size_t(W) * H);
        s2[32 * W + 32] = 200;      // 一个 200 的点,阈值 200 时为内部

        CHECK(TexDF_Generate(s2.data(), d2.data(), W, H, 200, 0, 0) == TEX_OK, "TexDF threshold 200");
        CHECK(d2[32 * W + 32] == 131, "single inside pixel -> 131 (dist 1 to dark)");
    }

    // 非法参数
    CHECK(TexDF_Generate(nullptr, dst.data(), W, H, 128, 0, 0) == TEX_ERR_PARAM, "null src rejected");

    // ---------------- TexImage_CreateFromData ----------------
    TexImageCallbacks cb;
    cb.log = nullptr;
    cb.user = nullptr;

    CHECK(TexImage_Init(&cb) == TEX_OK, "TexImage_Init");

    // 方向一致性:首行 255、其余 0;经 CreateFromData -> GetGray 后首字节仍应为 255
    {
        std::vector<uint8_t> pat(size_t(W) * H, 0);
        for(uint32_t x = 0; x < W; x++)
            pat[0 * W + x] = 255;

        TexImage img = nullptr;
        CHECK(TexImage_CreateFromData(&img, W, H, 1, TEX_PT_UInt8, pat.data()) == TEX_OK,
              "CreateFromData gray");

        uint32_t w = 0, h = 0;
        int layout = -1, pt = -1;
        TexImage_GetInfo(img, &w, &h, nullptr, &layout, &pt);
        CHECK(w == W && h == H, "CreateFromData dims");
        CHECK(layout == TEX_LAYOUT_Gray, "CreateFromData layout");

        std::vector<uint8_t> back(size_t(W) * H, 0xAA);
        CHECK(TexImage_GetData(img, back.data(), back.size(), TEX_LAYOUT_Gray, TEX_PT_UInt8) == TEX_OK,
              "GetGray after CreateFromData");
        CHECK(back[0] == 255, "first row stays first (orientation)");
        CHECK(back[W] == 0, "second row intact");

        TexImage_Free(img);
    }

    // RGBA 创建 + Alpha 通道取回(供 DF Alpha 路径使用)
    {
        std::vector<uint8_t> rgba(size_t(W) * H * 4, 0);
        for(size_t i = 0; i < size_t(W) * H; i++)
        {
            rgba[i * 4 + 0] = 10;       // R
            rgba[i * 4 + 1] = 20;       // G
            rgba[i * 4 + 2] = 30;       // B
            rgba[i * 4 + 3] = uint8_t(i & 0xFF);    // A 渐变
        }

        TexImage img = nullptr;
        CHECK(TexImage_CreateFromData(&img, W, H, 4, TEX_PT_UInt8, rgba.data()) == TEX_OK,
              "CreateFromData rgba");

        int layout = -1;
        TexImage_GetInfo(img, nullptr, nullptr, nullptr, &layout, nullptr);
        CHECK(layout == TEX_LAYOUT_RGBA, "rgba layout");

        std::vector<uint8_t> alpha(size_t(W) * H, 0);
        CHECK(TexImage_GetData(img, alpha.data(), alpha.size(), TEX_LAYOUT_Alpha, TEX_PT_UInt8) == TEX_OK,
              "GetAlpha after CreateFromData");
        CHECK(alpha[1] == 1 && alpha[200] == 200, "alpha channel round-trip");

        TexImage_Free(img);
    }

    TexImage_Shutdown();

    if(failures)
        printf("\n%d FAILURES\n", failures);
    else
        printf("\nALL PASS\n");
    return failures ? 1 : 0;
}
