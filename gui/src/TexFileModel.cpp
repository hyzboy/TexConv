#include "TexFileModel.h"
#include "CoreApi.h"

#include <QFileInfo>
#include <QColor>

namespace
{
    QString StateText(TexFileModel::FileState state)
    {
        using S = TexFileModel::FileState;

        switch(state)
        {
            case S::Pending:    return QStringLiteral("待检测");
            case S::Probing:    return QStringLiteral("检测中");
            case S::Ready:      return QStringLiteral("已就绪");
            case S::NotImage:   return QStringLiteral("非图片");
            case S::Converting: return QStringLiteral("转换中");
            case S::Done:       return QStringLiteral("完成");
            case S::Failed:     return QStringLiteral("失败");
            case S::Cancelled:  return QStringLiteral("已取消");
        }

        return QStringLiteral("?");
    }

    QColor StateColor(TexFileModel::FileState state)
    {
        using S = TexFileModel::FileState;

        switch(state)
        {
            case S::Ready:      return QColor(0, 128, 0);
            case S::NotImage:
            case S::Failed:     return QColor(192, 0, 0);
            case S::Converting: return QColor(0, 96, 160);
            case S::Done:       return QColor(0, 128, 64);
            case S::Cancelled:  return QColor(160, 96, 0);
            default:            return QColor(128, 128, 128);
        }
    }

    /// 按文件名识别法线贴图:含 normal/nmap,或以 _n/-n/ n 结尾(大小写不敏感)
    bool FilenameLooksNormal(const QString &path)
    {
        const QString name = QFileInfo(path).completeBaseName().toLower();

        if(name.contains(QStringLiteral("normal"))
         ||name.contains(QStringLiteral("nmap")))
            return true;

        return name.endsWith(QStringLiteral("_n"))
             ||name.endsWith(QStringLiteral("-n"))
             ||name.endsWith(QStringLiteral(" n"));
    }

    /// 该行全部已启用选项的汇总标记
    QString FlagsText(const TexFileModel::Item &item)
    {
        QStringList flags;

        if(item.single_channel) flags << QStringLiteral("单通道");
        if(item.gen_mipmaps)    flags << QStringLiteral("mip");
        if(item.force_grayscale) flags << QStringLiteral("灰度");
        if(item.discard_alpha)  flags << QStringLiteral("去α");
        if(item.normal_map)     flags << QStringLiteral("法线");

        if(item.df_mode)
            flags << QStringLiteral("距离场(%1)").arg(item.df_threshold);

        if(!item.provider.isEmpty())
            flags << item.provider;

        return flags.join(QStringLiteral(" · "));
    }

    /// 按文件名识别单通道语义贴图(Roughness/Displacement/Metallic/Alpha/
    /// Opacity/Luminance 等):命中则自动按单通道转换(等价 灰度+丢弃Alpha)。
    /// 法线优先:调用方保证法线命中的文件不再做本检测。
    bool FilenameLooksSingleChannel(const QString &path)
    {
        const QString name = QFileInfo(path).completeBaseName().toLower();

        // 长词子串匹配(误报风险低)
        static const QString contains_words[] =
        {
            QStringLiteral("roughness"), QStringLiteral("rough"),
            QStringLiteral("displacement"), QStringLiteral("disp"),
            QStringLiteral("metallic"), QStringLiteral("metalness"), QStringLiteral("metal"),
            QStringLiteral("alpha"), QStringLiteral("opacity"),
            QStringLiteral("luminance"), QStringLiteral("luma"),
            QStringLiteral("height"), QStringLiteral("bump"),
            QStringLiteral("gloss"), QStringLiteral("cavity"),
            QStringLiteral("occlusion"),
        };

        for(const QString &w : contains_words)
            if(name.contains(w))
                return true;

        // 短词按独立词匹配,避免子串误报(如 "column" 误含 "lum"、"chaos" 误含 "ao")
        QString token;

        for(const QChar ch : name)
        {
            if(ch.isLetterOrNumber())
            {
                token.append(ch);
                continue;
            }

            if(token == QLatin1String("ao") || token == QLatin1String("lum"))
                return true;

            token.clear();
        }

        return token == QLatin1String("ao") || token == QLatin1String("lum");
    }
}//namespace

TexFileModel::TexFileModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int TexFileModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(items_.size());
}

int TexFileModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColCount;
}

QVariant TexFileModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid() || index.row() >= int(items_.size()))
        return QVariant();

    const Item &item = items_[size_t(index.row())];

    switch(role)
    {
        case Qt::DisplayRole:
        {
            switch(index.column())
            {
                case ColFile:       return QFileInfo(item.path).fileName();
                case ColState:      return StateText(item.state);
                case ColSize:       return item.width ? QStringLiteral("%1×%2").arg(item.width).arg(item.height)
                                                      : QStringLiteral("-");
                case ColChannels:   return item.channels ? QString::number(item.channels)
                                                         : QStringLiteral("-");
                case ColPixelType:  return item.channels ? coreapi::PixelTypeName(item.pixel_type)
                                                         : QStringLiteral("-");
                case ColFlags:      return FlagsText(item);
                case ColTarget:     return item.state == NotImage ? QStringLiteral("-")
                                                                  : item.target_format;
                case ColResult:     return item.result_text;
            }
            break;
        }

        case Qt::ForegroundRole:
        {
            if(item.state == NotImage)
                return QColor(128, 128, 128);

            return StateColor(item.state);
        }

        case Qt::TextAlignmentRole:
        {
            if(index.column() == ColChannels)
                return int(Qt::AlignCenter);
            break;
        }

        case Qt::ToolTipRole:
        {
            if(index.column() == ColFile)
            {
                if(item.state == NotImage && !item.probe_fail_text.isEmpty())
                    return item.path + QStringLiteral("\n") + item.probe_fail_text;

                return item.path;
            }

            if(index.column() == ColTarget)
                return item.target_format;

            break;
        }

        case ChannelsRole:          return item.channels;
        case StateRole:             return int(item.state);
        case PathRole:              return item.path;
        case EffectiveChannelsRole: return EffectiveChannels(item);
        case NormalLockedRole:      return item.normal_map;
    }

    return QVariant();
}

QVariant TexFileModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch(section)
    {
        case ColFile:       return QStringLiteral("文件");
        case ColState:      return QStringLiteral("状态");
        case ColSize:       return QStringLiteral("尺寸");
        case ColChannels:   return QStringLiteral("通道");
        case ColPixelType:  return QStringLiteral("像素类型");
        case ColFlags:      return QStringLiteral("标记");
        case ColTarget:     return QStringLiteral("目标格式");
        case ColResult:     return QStringLiteral("结果");
    }

    return QVariant();
}

Qt::ItemFlags TexFileModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = QAbstractTableModel::flags(index);

    if(index.isValid())
        f |= Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    return f;
}

bool TexFileModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if(!index.isValid() || index.column() != ColTarget || role != Qt::EditRole)
        return false;

    const int row = index.row();

    if(row >= int(items_.size()))
        return false;

    Item &item = items_[size_t(row)];

    if(!coreapi::FormatAllowed(EffectiveChannels(item), value.toString()))
        return false;

    item.target_format = value.toString();

    Q_EMIT dataChanged(index, index, { Qt::DisplayRole, Qt::ToolTipRole });
    return true;
}

int TexFileModel::AddPaths(const QStringList &paths)
{
    int added = 0;

    for(const QString &p : paths)
    {
        const QString norm = QFileInfo(p).absoluteFilePath();
        const QString key  = norm.toLower();

        if(path_set_.contains(key))
            continue;

        path_set_.insert(key);

        const int row = int(items_.size());

        beginInsertRows(QModelIndex(), row, row);

        Item item;
        item.path       = norm;
        item.normal_map = FilenameLooksNormal(norm);    // 文件名自动识别法线贴图

        if(!item.normal_map)                            // 其余按单通道语义文件名识别
            item.single_channel = FilenameLooksSingleChannel(norm);

        items_.push_back(item);

        endInsertRows();

        ++added;
    }

    BumpCounts();
    return added;
}

bool TexFileModel::RemoveRows(const QModelIndexList &selected)
{
    if(locked_ || selected.isEmpty())
        return false;

    // 自后向前删,避免行号位移
    QList<int> rows;

    for(const QModelIndex &idx : selected)
        if(idx.isValid())
            rows << idx.row();

    std::sort(rows.begin(), rows.end(), std::greater<int>());

    for(int row : rows)
    {
        if(row < 0 || row >= int(items_.size()))
            continue;

        beginRemoveRows(QModelIndex(), row, row);

        path_set_.remove(QFileInfo(items_[size_t(row)].path).absoluteFilePath().toLower());
        items_.erase(items_.begin() + row);

        endRemoveRows();
    }

    BumpCounts();
    return true;
}

void TexFileModel::Clear()
{
    if(locked_)
        return;

    beginResetModel();
    items_.clear();
    path_set_.clear();
    endResetModel();

    BumpCounts();
}

QString TexFileModel::DeriveTarget(const Item &item) const
{
    return coreapi::DefaultSlotFormat(EffectiveChannels(item),
                                      item.normal_map, item.df_mode);
}

/// 对齐内核 ConvertImage 的预处理顺序:
///   1) 灰度:有 alpha 或未丢弃 alpha → 2 通道,否则 1 通道
///   2) 丢弃 alpha:RGBA→RGB,GrayAlpha→Gray
///   3) 距离场:生成后恒为 1 通道
int TexFileModel::EffectiveChannels(const Item &item) const
{
    if(item.channels < 1)
        return 0;

    int ch = item.channels;

    if(item.force_grayscale)
        ch = (item.has_alpha || !item.discard_alpha) ? 2 : 1;

    if(item.discard_alpha)
    {
        if(ch == 4)      ch = 3;
        else if(ch == 2) ch = 1;
    }

    if(item.single_channel)
        ch = 1;

    if(item.df_mode)
        ch = 1;

    return ch;
}

void TexFileModel::RefreshItem(int row)
{
    Q_EMIT dataChanged(index(row, 0), index(row, ColCount - 1));
}

void TexFileModel::MarkProbing(int row)
{
    if(row < 0 || row >= int(items_.size()))
        return;

    items_[size_t(row)].state = Probing;

    Q_EMIT dataChanged(index(row, ColState), index(row, ColState));
}

void TexFileModel::SetProbeResult(int row, quint32 w, quint32 h,
                                  int channels, int layout, int pixel_type, bool has_alpha)
{
    if(row < 0 || row >= int(items_.size()))
        return;

    Item &item = items_[size_t(row)];

    item.state       = Ready;
    item.width       = w;
    item.height      = h;
    item.channels    = channels;
    item.layout      = layout;
    item.pixel_type  = pixel_type;
    item.has_alpha   = has_alpha;

    item.target_format = DeriveTarget(item);

    RefreshItem(row);
    BumpCounts();
}

void TexFileModel::SetProbeFailed(int row, const QString &reason)
{
    if(row < 0 || row >= int(items_.size()))
        return;

    Item &item = items_[size_t(row)];

    item.state          = NotImage;
    item.channels       = 0;
    item.width          = 0;
    item.height         = 0;
    item.probe_fail_text = reason;
    item.target_format  = QString();
    item.result_text    = QString();

    RefreshItem(row);
    BumpCounts();
}

void TexFileModel::MarkConverting(int row)
{
    if(row < 0 || row >= int(items_.size()))
        return;

    items_[size_t(row)].state = Converting;
    items_[size_t(row)].result_text.clear();

    RefreshItem(row);
}

void TexFileModel::SetConvertDone(int row, qint64 output_size)
{
    if(row < 0 || row >= int(items_.size()))
        return;

    Item &item = items_[size_t(row)];

    item.state       = Done;
    item.output_size = output_size;

    item.result_text = QStringLiteral("%1 -> %2 (%3 字节)")
                           .arg(item.target_format,
                                QFileInfo(item.path).completeBaseName() + ".Tex2D")
                           .arg(output_size);

    RefreshItem(row);
}

void TexFileModel::SetConvertFailed(int row, const QString &reason)
{
    if(row < 0 || row >= int(items_.size()))
        return;

    Item &item = items_[size_t(row)];

    item.state       = Failed;
    item.result_text = reason;

    RefreshItem(row);
}

void TexFileModel::SetConvertCancelled(int row)
{
    if(row < 0 || row >= int(items_.size()))
        return;

    items_[size_t(row)].state = Cancelled;
    items_[size_t(row)].result_text = QStringLiteral("已取消(半成品已删除)");

    RefreshItem(row);
}

void TexFileModel::SetFilesMipmaps(const QModelIndexList &rows, bool on)
{
    if(locked_)
        return;

    for(const QModelIndex &idx : rows)
    {
        if(!idx.isValid() || idx.row() >= int(items_.size()))
            continue;

        items_[size_t(idx.row())].gen_mipmaps = on;

        RefreshItem(idx.row());
    }
}

void TexFileModel::SetFilesGrayscale(const QModelIndexList &rows, bool on)
{
    if(locked_)
        return;

    for(const QModelIndex &idx : rows)
    {
        if(!idx.isValid() || idx.row() >= int(items_.size()))
            continue;

        Item &item = items_[size_t(idx.row())];

        item.force_grayscale = on;
        item.target_format = DeriveTarget(item);    // 通道数可能变化,重推默认

        RefreshItem(idx.row());
    }
}

void TexFileModel::SetFilesDiscardAlpha(const QModelIndexList &rows, bool on)
{
    if(locked_)
        return;

    for(const QModelIndex &idx : rows)
    {
        if(!idx.isValid() || idx.row() >= int(items_.size()))
            continue;

        Item &item = items_[size_t(idx.row())];

        item.discard_alpha = on;
        item.target_format = DeriveTarget(item);    // 通道数可能变化,重推默认

        RefreshItem(idx.row());
    }
}

void TexFileModel::SetFilesProvider(const QModelIndexList &rows, const QString &provider)
{
    if(locked_)
        return;

    for(const QModelIndex &idx : rows)
    {
        if(!idx.isValid() || idx.row() >= int(items_.size()))
            continue;

        items_[size_t(idx.row())].provider = provider;

        RefreshItem(idx.row());
    }
}

void TexFileModel::SetFilesNormal(const QModelIndexList &rows, bool on)
{
    if(locked_)
        return;

    for(const QModelIndex &idx : rows)
    {
        if(!idx.isValid() || idx.row() >= int(items_.size()))
            continue;

        Item &item = items_[size_t(idx.row())];

        item.normal_map = on;
        item.target_format = DeriveTarget(item);    // 开→BC5;关→重推默认

        RefreshItem(idx.row());
    }
}

void TexFileModel::SetFilesSingleChannel(const QModelIndexList &rows, bool on)
{
    if(locked_)
        return;

    for(const QModelIndex &idx : rows)
    {
        if(!idx.isValid() || idx.row() >= int(items_.size()))
            continue;

        Item &item = items_[size_t(idx.row())];

        item.single_channel = on;
        item.target_format = DeriveTarget(item);    // 有效通道数变为 1,重推默认(BC4)

        RefreshItem(idx.row());
    }
}

void TexFileModel::SetFilesDF(const QModelIndexList &rows, bool on)
{
    if(locked_)
        return;

    for(const QModelIndex &idx : rows)
    {
        if(!idx.isValid() || idx.row() >= int(items_.size()))
            continue;

        Item &item = items_[size_t(idx.row())];

        item.df_mode = on;
        item.target_format = DeriveTarget(item);    // 开→按 1 通道重推;关→恢复

        RefreshItem(idx.row());
    }
}

void TexFileModel::SetFilesDFThreshold(const QModelIndexList &rows, int threshold)
{
    if(locked_)
        return;

    for(const QModelIndex &idx : rows)
    {
        if(!idx.isValid() || idx.row() >= int(items_.size()))
            continue;

        Item &item = items_[size_t(idx.row())];

        item.df_threshold = threshold;

        RefreshItem(idx.row());
    }
}

void TexFileModel::SetFilesFormat(const QModelIndexList &rows, const QString &format)
{
    if(locked_)
        return;

    for(const QModelIndex &idx : rows)
    {
        if(!idx.isValid() || idx.row() >= int(items_.size()))
            continue;

        Item &item = items_[size_t(idx.row())];

        if(item.state != Ready)
            continue;

        // 仅设置对该行有效的格式;法线行保持 BC5 锁定
        if(item.normal_map)
            continue;

        if(!coreapi::FormatAllowed(EffectiveChannels(item), format))
            continue;

        item.target_format = format;

        RefreshItem(idx.row());
    }
}

std::vector<TexFileModel::JobDesc> TexFileModel::CollectReadyJobs() const
{
    std::vector<JobDesc> jobs;

    for(const Item &item : items_)
    {
        if(item.state != Ready)
            continue;

        JobDesc job;
        job.row            = int(&item - items_.data());
        job.path           = item.path;
        job.format         = item.target_format;
        job.gen_mipmaps    = item.gen_mipmaps;
        job.force_grayscale = item.force_grayscale;
        job.discard_alpha  = item.discard_alpha;
        job.normal_map     = item.normal_map;
        job.single_channel = item.single_channel;
        job.df_mode        = item.df_mode;
        job.df_threshold   = item.df_threshold;
        job.provider       = item.provider;
        jobs.push_back(job);
    }

    return jobs;
}

int TexFileModel::ReadyCount() const
{
    int n = 0;

    for(const Item &item : items_)
        if(item.state == Ready)
            ++n;

    return n;
}

QModelIndexList TexFileModel::AllRowIndexes() const
{
    QModelIndexList list;

    for(int row = 0; row < int(items_.size()); row++)
        list.append(index(row, 0));

    return list;
}

void TexFileModel::BumpCounts()
{
    Q_EMIT CountsChanged(TotalCount(), ReadyCount());
}
