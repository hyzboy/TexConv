#pragma once
// IEEE 754 单精度(float32) → 半精度(float16)位模式转换。
//
// 两处使用:
//   1. 编码插件:ISPC 的 BC6H kernel 按 RGBA16F 交错读取源数据,需要真正的半精度位模式;
//      图像层的 TEX_PT_Float16 实际是 16 位整数存储(Magick ShortPixel),因此由插件自行转换。
//   2. 核心 IBL 烘焙(core/src/cube_pipeline.cpp):BC6H/RGBA16F 产物直接由 float32 量化。
// 因此本头文件放在 common(而非某个插件)以便两边共用 —— 转换规则必须只有一份。

#include <cstdint>
#include <cstring>

/// 就近舍入到偶数,含上下溢 / Inf / NaN 处理
inline uint16_t TexFloatToHalfBits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));

    const uint32_t sign     = (bits >> 16) & 0x8000u;
    const uint32_t exponent = (bits >> 23) & 0xFFu;
    uint32_t       mantissa = bits & 0x7FFFFFu;

    if(exponent == 0xFFu)                       // Inf / NaN
        return uint16_t(sign | 0x7C00u | (mantissa ? 0x200u : 0u));

    const int32_t half_exponent = int32_t(exponent) - 127 + 15;

    if(half_exponent >= 0x1F)                   // 超出半精度可表示范围
        return uint16_t(sign | 0x7C00u);

    if(half_exponent <= 0)                      // 半精度非规格化数
    {
        if(half_exponent < -10)                 // 过小 → ±0
            return uint16_t(sign);

        mantissa |= 0x800000u;

        const int32_t  shift         = 14 - half_exponent;
        uint32_t       half_mantissa = mantissa >> shift;
        const uint32_t round_bit     = (mantissa >> (shift - 1)) & 1u;
        const uint32_t sticky        = mantissa & ((uint32_t(1) << (shift - 1)) - 1u);

        if(round_bit && (sticky || (half_mantissa & 1u)))
            ++half_mantissa;

        // 进位到 0x400 时位模式正好是最小规格化数
        return uint16_t(sign | half_mantissa);
    }

    uint32_t       half_mantissa = mantissa >> 13;
    const uint32_t round_bit     = (mantissa >> 12) & 1u;
    const uint32_t sticky        = mantissa & 0xFFFu;

    if(round_bit && (sticky || (half_mantissa & 1u)))
        ++half_mantissa;                        // 进位溢出到指数位时位模式依然正确

    // 必须用加法:尾数进位到 0x400 时是进到指数域,按位或不会产生进位
    return uint16_t(sign | ((uint32_t(half_exponent) << 10) + half_mantissa));
}
