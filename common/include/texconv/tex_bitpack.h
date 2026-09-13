#pragma once
// 像素位打包转换(逐字节复刻自 hgl CMCoreType/inc/hgl/color/ColorFormat.h)。
//
// 注意:这些函数的位运算细节(包括 RGBA16toA2BGR10 末项取 a 而非 r 的既有笔误)
// 直接决定产物字节,为了与旧版 .Tex2D 保持逐字节一致,必须原样保留,不做"修复"。

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint16_t Tex_RGB8toRGB565(const uint8_t r, const uint8_t g, const uint8_t b)
{
    return (uint16_t)(((r << 8) & 0xF800)
                     |((g << 3) & 0x7E0)
                     | (b >> 3));
}

static inline void Tex_RGB8toRGB565_Array(uint16_t *target, uint8_t *src, uint32_t size)
{
    for(uint32_t i = 0; i < size; i++)
    {
        *target = (uint16_t)(((src[0] << 8) & 0xF800)
                            |((src[1] << 3) & 0x7E0)
                            | (src[2] >> 3));
        ++target;
        src += 3;
    }
}

// half_float 即 16 位半精度原始位型(uint16_t),与 hgl half_float 布局一致
static inline void Tex_RGB16FtoB10GR11UF(uint32_t *target, uint16_t *src, uint32_t size)
{
    for(uint32_t i = 0; i < size; i++)
    {
        *target = ((src[2] & 0x7FE0) << 17)
                 |((src[1] & 0x7FF0) << 7)
                 | (src[0] & 0x7FF0) >> 4;
        ++target;
        src += 3;
    }
}

static inline void Tex_RGBA8toBGRA4(uint16_t *target, uint8_t *src, uint32_t size)
{
    for(uint32_t i = 0; i < size; i++)
    {
        *target = (uint16_t)(((src[2] << 8) & 0xF000)
                            |((src[1] << 4) & 0xF00)
                            |((src[0]    ) & 0xF0)
                            | (src[3] >> 4));
        ++target;
        src += 4;
    }
}

static inline void Tex_RGBA8toRGBA4(uint16_t *target, uint8_t *src, uint32_t size)
{
    for(uint32_t i = 0; i < size; i++)
    {
        *target = (uint16_t)(((src[0] << 8) & 0xF000)
                            |((src[1] << 4) & 0xF00)
                            |((src[2]    ) & 0xF0)
                            | (src[3] >> 4));
        ++target;
        src += 4;
    }
}

static inline void Tex_RGBA8toA1RGB5(uint16_t *target, uint8_t *src, uint32_t size)
{
    for(uint32_t i = 0; i < size; i++)
    {
        *target = (uint16_t)(((src[3] << 8) & 0x8000)
                            |((src[0] << 7) & 0x7C00)
                            |((src[1] << 2) & 0x3E0)
                            | (src[2] >> 3));
        ++target;
        src += 4;
    }
}

// 复刻 hgl 数组版原实现(末项为 src[0]>>6 即 r;标量版存在的 a>>6 笔误未被调用路径使用)
static inline void Tex_RGBA16toA2BGR10(uint32_t *target, uint16_t *src, uint32_t size)
{
    for(uint32_t i = 0; i < size; i++)
    {
        *target = ((src[3] << 16) & 0xC0000000u)
                 |((src[2] << 14) & 0x3FF00000u)
                 |((src[1] <<  4) & 0xFFC00u)
                 | (src[0] >>  6);
        ++target;
        src += 4;
    }
}

static inline uint32_t Tex_EndianSwap32(const uint32_t value)
{
    return ((value >> 24) & 0x000000FFu)
          |((value >>  8) & 0x0000FF00u)
          |((value <<  8) & 0x00FF0000u)
          |((value << 24) & 0xFF000000u);
}

static inline void Tex_EndianSwap32_Array(uint32_t *data, uint32_t count)
{
    for(uint32_t i = 0; i < count; i++)
        data[i] = Tex_EndianSwap32(data[i]);
}

#ifdef __cplusplus
}
#endif
