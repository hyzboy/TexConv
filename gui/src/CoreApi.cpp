#include "CoreApi.h"

namespace coreapi
{
    QString DefaultSlotFormat(int source_channels,
                              bool normal_map,
                              bool df_mode)
    {
        if(source_channels < 1 || source_channels > 4)
            return QString();

        if(normal_map)
            return QStringLiteral("BC5");

        // 与内核 pipeline.cpp 的 default_slot_name 一致
        static const char *slot_default[4] = { "BC4", "BC5", "BC7", "BC7" };

        // DF 模式下 1 通道默认 R8(避开 AMD 的 BC4 灰度源已知缺陷)
        if(df_mode && source_channels == 1)
            return QStringLiteral("R8");

        return QString::fromLatin1(slot_default[source_channels - 1]);
    }

    QStringList AllowedFormats(int source_channels)
    {
        QStringList result;

        if(source_channels < 1 || source_channels > 4)
            return result;

        const int count = TexFormat_Count();

        for(int i = 0; i < count; i++)
        {
            const TexPixelFormat *pf = TexFormat_At(i);

            if(!pf || pf->format == TEX_FMT_COMPRESS)   // 跳过分界伪条目
                continue;

            if(TexFormat_IsAllowed(source_channels, pf))
                result << QString::fromLatin1(pf->name);
        }

        return result;
    }

    bool FormatAllowed(int source_channels, const QString &format_name)
    {
        const QByteArray name = format_name.toLatin1();

        const TexPixelFormat *pf = TexFormat_Get(name.constData());

        if(!pf)
            return false;

        return TexFormat_IsAllowed(source_channels, pf) != 0;
    }

    QString ErrorText(int tex_result)
    {
        switch(tex_result)
        {
            case TEX_OK:                return QStringLiteral("成功");
            case TEX_ERR_PARAM:         return QStringLiteral("参数错误");
            case TEX_ERR_LOAD_IMAGE:    return QStringLiteral("非图片或图片损坏/无法解码");
            case TEX_ERR_UNSUPPORTED:   return QStringLiteral("源图通道数与目标格式组合不受支持");
            case TEX_ERR_ENCODE:        return QStringLiteral("编码失败");
            case TEX_ERR_IO:            return QStringLiteral("文件读写失败");
            case TEX_ERR_NO_PROVIDER:   return QStringLiteral("压缩后端不可用");
            case TEX_ERR_CANCELLED:     return QStringLiteral("已取消");
            case TEX_ERR_INTERNAL:      return QStringLiteral("内部错误");
            default:                    return QStringLiteral("未知错误(%1)").arg(tex_result);
        }
    }

    QString PixelTypeName(int pixel_type)
    {
        switch(pixel_type)
        {
            case TEX_PT_Int8:    return QStringLiteral("Int8");
            case TEX_PT_UInt8:   return QStringLiteral("UInt8");
            case TEX_PT_Int16:   return QStringLiteral("Int16");
            case TEX_PT_UInt16:  return QStringLiteral("UInt16");
            case TEX_PT_Int32:   return QStringLiteral("Int32");
            case TEX_PT_UInt32:  return QStringLiteral("UInt32");
            case TEX_PT_Float16: return QStringLiteral("Float16");
            case TEX_PT_Float32: return QStringLiteral("Float32");
            case TEX_PT_Float64: return QStringLiteral("Float64");
            default:             return QStringLiteral("?");
        }
    }

    QString LayoutName(int layout)
    {
        switch(layout)
        {
            case TEX_LAYOUT_Alpha:     return QStringLiteral("Alpha");
            case TEX_LAYOUT_Gray:      return QStringLiteral("Gray");
            case TEX_LAYOUT_GrayAlpha: return QStringLiteral("GrayAlpha");
            case TEX_LAYOUT_RGB:       return QStringLiteral("RGB");
            case TEX_LAYOUT_RGBA:      return QStringLiteral("RGBA");
            default:                   return QStringLiteral("?");
        }
    }
}//namespace coreapi
