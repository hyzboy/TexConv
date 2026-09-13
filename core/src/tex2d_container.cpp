// TexConvCore —— .Tex2D 容器写出/读回。
// 逐行迁自原 TextureFileCreater.cpp,格式保持不变(引擎零改动)。

#include "internal.h"

namespace texcore
{
    bool ContainerOpen(const std::wstring &path, FILE *&out)
    {
        FILE *f = nullptr;
        if(_wfopen_s(&f, path.c_str(), L"wb") != 0 || !f)
            return false;

        out = f;
        return true;
    }

    bool ContainerWriteHeader(FILE *f, int view_type)
    {
        if(fwrite(TEX2D_HEADER_MAGIC, 1, TEX2D_HEADER_MAGIC_LEN, f) != TEX2D_HEADER_MAGIC_LEN)
            return false;

        const uint8_t version  = TEX2D_VERSION;
        const uint8_t type     = (uint8_t)view_type;

        if(fwrite(&version, 1, 1, f) != 1)return false;
        if(fwrite(&type,    1, 1, f) != 1)return false;

        return true;
    }

    bool ContainerWriteSize2D(FILE *f, uint32_t width, uint32_t height)
    {
        const uint32_t depth = 0;

        if(fwrite(&width,  4, 1, f) != 1)return false;
        if(fwrite(&height, 4, 1, f) != 1)return false;
        if(fwrite(&depth,  4, 1, f) != 1)return false;

        return true;
    }

    bool ContainerWriteFormatBlock(FILE *f, const TexPixelFormat *fmt, int mip_level)
    {
        if(!fmt)return false;

        if(fmt->format > TEX_FMT_COMPRESS)
        {
            const uint8_t zero = 0;
            char spaces[10] = {};

            if(fwrite(&zero, 1, 1, f) != 1)return false;

            // 压缩格式名字没有超过 9 字节的(对齐旧实现)
            strncpy(spaces, fmt->name, 9);

            if(fwrite(spaces, 1, 9, f) != 9)return false;
        }
        else
        {
            const uint8_t channels = fmt->channels;

            if(fwrite(&channels, 1, 1, f) != 1)return false;
            if(fwrite(fmt->color, 1, 4, f) != 4)return false;
            if(fwrite(fmt->bits,  1, 4, f) != 4)return false;
            if(fwrite(&fmt->vulkan_type, 1, 1, f) != 1)return false;
        }

        const uint8_t mip = (uint8_t)mip_level;

        if(fwrite(&mip, 1, 1, f) != 1)return false;

        return true;
    }

    uint32_t ContainerWriteLevel(FILE *f, const void *data, uint32_t bytes)
    {
        if(fwrite(data, 1, bytes, f) != bytes)
            return 0;

        // 对齐旧 TextureFileCreater::Write:不足 8 字节补 0
        if(bytes < 8)
        {
            static const uint64_t space = 0;
            const uint32_t pad = 8 - bytes;

            if(fwrite(&space, 1, pad, f) != pad)
                return 0;

            return 8;
        }

        return bytes;
    }

    void ContainerClose(FILE *f)
    {
        if(f)
            fclose(f);
    }

    bool ContainerDelete(const std::wstring &path)
    {
        return _wremove(path.c_str()) == 0;
    }
}//namespace texcore
