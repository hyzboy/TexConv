// TexCube 文件的引擎加载器验证:用 hgl::graph::TextureCubeLoader 解析
// TexConv 生成的 .TexCube,校验头部/格式/mip 级数/总字节数与文件大小一致。
// (纯文件解析,不需要 Vulkan 设备;引擎 GPU 上传走 CommitTextureCubeMipmaps)
//
// usage: unit_cubefile <file.TexCube> <expect_width> <expect_height> <expect_mips>
#include <stdio.h>
#include <vector>
#include <windows.h>
#include <hgl/graph/texture/TextureLoader.h>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    /// 补齐 OnBegin/OnEnd:把载荷读入内存缓冲(与引擎 VkTextureLoader 行为一致)
    class FileCubeLoader:public TextureCubeLoader
    {
        std::vector<char> data;

    protected:

        void *OnBegin(uint32 total_bytes, const VkFormat &format) override
        {
            data.resize(total_bytes);
            return data.data();
        }

        bool OnEnd() override { return true; }
        void OnError() override {}

    public:

        using TextureCubeLoader::GetTotalBytes;   // protected → public 转发
    };
}

static int failures = 0;
#define CHECK(cond, msg) do{ if(!(cond)){ printf("FAIL: %s\n", msg); ++failures; } }while(0)

int wmain(int argc, wchar_t **argv)
{
    if(argc < 5)
    {
        printf("usage: unit_cubefile <file.TexCube> <w> <h> <mips>\n");
        return 2;
    }

    const OSString filename((const os_char *)argv[1]);

    FileCubeLoader loader;

    CHECK(loader.Load(filename), "TextureCubeLoader::Load");

    const TextureFileHeader &header = loader.GetFileHeader();

    CHECK(header.type == 3, "view type == CUBE(3)");    // VkImageViewType_CUBE
    CHECK(header.version == 0, "version == 0");
    CHECK(header.width == (uint32)_wtoi(argv[2]), "width");
    CHECK(header.height == (uint32)_wtoi(argv[3]), "height");
    CHECK(header.depth == 0, "depth == 0");
    CHECK(header.mipmaps == (uint8)_wtoi(argv[4]), "mipmaps");

    if(header.pixel_format.channels == 0)
    {
        char name[10];
        memcpy(name, header.pixel_format.compress_format, 9);
        name[9] = 0;
        printf("format: %s (compress)\n", name);
    }
    else
    {
        printf("format: %d channels, %d bits\n", header.pixel_format.channels,
               header.pixel_format.bits[0] + header.pixel_format.bits[1] +
               header.pixel_format.bits[2] + header.pixel_format.bits[3]);
    }

    // 总字节数校验:loader 期望的 mip 链×6 与文件剩余大小一致
    HANDLE f = CreateFileW((LPCWSTR)filename.c_str(), GENERIC_READ,
                           FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);

    CHECK(f != INVALID_HANDLE_VALUE, "open file for size");

    if(f != INVALID_HANDLE_VALUE)
    {
        const LARGE_INTEGER size{{0}};
        LARGE_INTEGER li;
        GetFileSizeEx(f, &li);
        CloseHandle(f);

        const uint32 header_len = TextureFileHeaderLength;
        const uint32 payload = uint32(li.QuadPart) - header_len;
        const uint32 expect = loader.GetTotalBytes();

        CHECK(payload == expect, "payload == loader total bytes");

        printf("file: %ld bytes, header %u, payload %u, loader expects %u\n",
               (long)li.QuadPart, header_len, payload, expect);
    }

    if(failures)
        printf("\n%d FAILURES\n", failures);
    else
        printf("\nALL PASS\n");
    return failures ? 1 : 0;
}
