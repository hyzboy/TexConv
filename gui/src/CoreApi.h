#pragma once
// TexConv C API 的 Qt 薄封装:格式表访问、默认目标格式推导、错误码文案。
// 生命周期由 main.cpp 负责(TexCore_Init/Shutdown 各一次),这里不做。

#include <QString>
#include <QStringList>
#include <vector>

#include "texconv/tex_core.h"
#include "texconv/tex_formats.h"
#include "texconv/tex_pixel.h"

namespace coreapi
{
    /// 源图通道数(1-4)对应的默认目标槽位格式名。
    /// 与内核 pipeline.cpp 的决策一致:normal_map→BC5、DF 模式 1 通道→R8、
    /// 否则 BC4/BC5/BC7/BC7。(本步 GUI 未暴露 normal/DF 开关,但推导保持完整)
    QString DefaultSlotFormat(int source_channels,
                              bool normal_map = false,
                              bool df_mode = false);

    /// 指定源通道数的全部合法目标格式名(跳过 COMPRESS 伪条目,经
    /// TexFormat_IsAllowed 过滤 —— 顺序为格式表顺序)。
    QStringList AllowedFormats(int source_channels);

    /// 格式名是否为该源通道数的合法目标
    bool FormatAllowed(int source_channels, const QString &format_name);

    /// 错误码 → 可读文案(UTF-16)
    QString ErrorText(int tex_result);

    /// 像素类型枚举 → 可读名
    QString PixelTypeName(int pixel_type);

    /// 通道布局枚举 → 可读名(如 Gray/GrayAlpha/RGB/RGBA)
    QString LayoutName(int layout);
}//namespace coreapi
