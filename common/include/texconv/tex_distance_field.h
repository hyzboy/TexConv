#pragma once
// 距离场生成(纯 CRT,无第三方依赖)。
//
// 算法与语义迁自原 DFGen(DistanceFieldGenerater.cpp,从未编译发布的 v1.1):
//   * 双网格两遍扫描 SDF:grid1 = 到暗部(值<threshold)的距离,grid2 = 到亮部
//     (值>=threshold)的距离,输出 = (d_dark - d_bright) —— 亮部内部为正;
//   * 输出编码:c = dist*scale + bias,截断到 [0,255](默认 scale=3,bias=128,
//     即 128 为形状边缘,内部增亮、外部减暗,约 ±42 像素达到饱和);
//   * 邻域为 8 向,距离为欧氏距离(截断取整)。

#include <stdint.h>
#include "tex_result.h"

#ifdef __cplusplus
extern "C" {
#endif

/// 由单通道 8-bit 图生成距离场。
/// src/dst 均为 width*height 字节;threshold 以下为"外部"。
/// scale/bias 为输出编码参数(默认 3/128,传 0 时使用默认)。
/// 返回 TEX_OK 或错误码。
int TexDF_Generate(const uint8_t *src, uint8_t *dst,
                   uint32_t width, uint32_t height,
                   uint8_t threshold, int scale, int bias);

#ifdef __cplusplus
}
#endif
