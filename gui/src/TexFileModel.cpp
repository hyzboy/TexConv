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

        case ChannelsRole:  return item.channels;
        case StateRole:     return int(item.state);
        case PathRole:      return item.path;
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

    if(!coreapi::FormatAllowed(item.channels, value.toString()))
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
        item.path = norm;
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

void TexFileModel::MarkProbing(int row)
{
    if(row < 0 || row >= int(items_.size()))
        return;

    items_[size_t(row)].state = Probing;

    const QModelIndex idx = index(row, ColState);
    Q_EMIT dataChanged(idx, idx);
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

    item.target_format = coreapi::DefaultSlotFormat(EffectiveChannels(item),
                                                    normal_map_, df_mode_);

    Q_EMIT dataChanged(index(row, 0), index(row, ColCount - 1));
    BumpCounts();
}

void TexFileModel::SetOptionFlags(bool normal_map, bool df_mode)
{
    if(locked_)
        return;

    normal_map_ = normal_map;
    df_mode_    = df_mode;

    // 全部已就绪行重推默认格式(法线→BC5;DF→按 1 通道推导)
    for(size_t i = 0; i < items_.size(); i++)
    {
        Item &item = items_[i];

        if(item.state != Ready)
            continue;

        item.target_format = coreapi::DefaultSlotFormat(EffectiveChannels(item),
                                                        normal_map_, df_mode_);

        Q_EMIT dataChanged(index(int(i), 0), index(int(i), ColCount - 1));
    }
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

    Q_EMIT dataChanged(index(row, 0), index(row, ColCount - 1));
    BumpCounts();
}

void TexFileModel::MarkConverting(int row)
{
    if(row < 0 || row >= int(items_.size()))
        return;

    items_[size_t(row)].state = Converting;
    items_[size_t(row)].result_text.clear();

    Q_EMIT dataChanged(index(row, 0), index(row, ColCount - 1));
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

    Q_EMIT dataChanged(index(row, 0), index(row, ColCount - 1));
}

void TexFileModel::SetConvertFailed(int row, const QString &reason)
{
    if(row < 0 || row >= int(items_.size()))
        return;

    Item &item = items_[size_t(row)];

    item.state       = Failed;
    item.result_text = reason;

    Q_EMIT dataChanged(index(row, 0), index(row, ColCount - 1));
}

void TexFileModel::SetConvertCancelled(int row)
{
    if(row < 0 || row >= int(items_.size()))
        return;

    items_[size_t(row)].state = Cancelled;
    items_[size_t(row)].result_text = QStringLiteral("已取消(半成品已删除)");

    Q_EMIT dataChanged(index(row, 0), index(row, ColCount - 1));
}

std::vector<TexFileModel::JobDesc> TexFileModel::CollectReadyJobs() const
{
    std::vector<JobDesc> jobs;

    for(const Item &item : items_)
    {
        if(item.state != Ready)
            continue;

        JobDesc job;
        job.row    = int(&item - items_.data());
        job.path   = item.path;
        job.format = item.target_format;
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

void TexFileModel::BumpCounts()
{
    Q_EMIT CountsChanged(TotalCount(), ReadyCount());
}
