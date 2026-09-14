#pragma once
// 文件队列表格模型。
// 行生命周期:Pending →(检测)→ Ready / NotImage →(转换)→ Converting → Done / Failed / Cancelled
// 转换进行中模型进入只读锁定(禁止增删),行号保持稳定,工作线程以行号回发结果。

#include <QAbstractTableModel>
#include <QSet>
#include <QString>
#include <vector>

class TexFileModel : public QAbstractTableModel
{
    Q_OBJECT

public:

    enum FileState
    {
        Pending,        // 待检测
        Probing,        // 检测中
        Ready,          // 检测通过,可转换
        NotImage,       // 检测失败:非图片/损坏
        Converting,     // 转换中
        Done,           // 转换完成
        Failed,         // 转换失败
        Cancelled,      // 转换被取消
    };

    enum Column
    {
        ColFile = 0,    // 文件名(完整路径 tooltip)
        ColState,       // 状态
        ColSize,        // 宽x高
        ColChannels,    // 通道
        ColPixelType,   // 像素类型
        ColTarget,      // 目标格式(选中行变 ComboBox)
        ColResult,      // 转换结果
        ColCount
    };

    /// 供委托/工作线程读取的自定义角色
    enum ItemRole
    {
        ChannelsRole = Qt::UserRole + 1,    // int:源图通道数(未检测 = 0)
        StateRole,                          // int:FileState
        PathRole,                           // QString:完整路径
        EffectiveChannelsRole,              // int:计 DF 后的有效通道数(委托过滤用)
        NormalLockedRole,                   // bool:法线模式锁定目标格式为 BC5
    };

    struct Item
    {
        QString  path;
        FileState state = Pending;
        quint32  width = 0, height = 0;
        int      channels = 0;
        int      layout = -1;
        int      pixel_type = -1;
        bool     has_alpha = false;
        QString  target_format;             // 当前选择的目标格式(检测后填默认值)
        QString  result_text;               // 转换结果/失败原因
        qint64   output_size = -1;
        QString  probe_fail_text;           // 检测失败原因
    };

    explicit TexFileModel(QObject *parent = nullptr);

    // --- QAbstractTableModel ---
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    // --- 内容操作 ---
    /// 批量添加文件(去重,同路径只留一条);返回实际新增数
    int AddPaths(const QStringList &paths);

    /// 移除指定行(转换锁定期间调用无效)
    bool RemoveRows(const QModelIndexList &selected);

    void Clear();

    /// 检测前置位:对应行状态 → Probing
    void MarkProbing(int row);

    /// 回填检测结果(默认目标格式按当前选项由模型推导)
    void SetProbeResult(int row, quint32 w, quint32 h,
                        int channels, int layout, int pixel_type, bool has_alpha);

    void SetProbeFailed(int row, const QString &reason);

    /// 转换前置位
    void MarkConverting(int row);

    void SetConvertDone(int row, qint64 output_size);
    void SetConvertFailed(int row, const QString &reason);
    void SetConvertCancelled(int row);

    /// 只读锁定(转换期间禁止增删)
    void SetLocked(bool locked) { locked_ = locked; }
    bool IsLocked() const { return locked_; }

    /// 转换选项旗标(法线/DF):影响默认目标格式推导与格式列可编辑性。
    /// 全部"已就绪"行的目标格式按新选项重新推导为默认值。
    void SetOptionFlags(bool normal_map, bool df_mode);

    bool NormalMapOn() const { return normal_map_; }
    bool DfModeOn() const { return df_mode_; }

    // --- 工作线程取快照 ---
    struct JobDesc
    {
        int      row;
        QString  path;
        QString  format;
    };

    /// 取所有"检测通过"行的任务快照
    std::vector<JobDesc> CollectReadyJobs() const;

    /// 取指定行通道数(委托/其它 UI 用)
    int ChannelsAt(int row) const { return At(row).channels; }

    const Item &At(int row) const { return items_[size_t(row)]; }

    int TotalCount() const { return int(items_.size()); }
    int ReadyCount() const;

Q_SIGNALS:

    void CountsChanged(int total, int ready);

private:

    void BumpCounts();

    std::vector<Item>   items_;
    QSet<QString>       path_set_;      // 去重(小写化路径)
    bool                locked_ = false;
    bool                normal_map_ = false;
    bool                df_mode_ = false;

    int EffectiveChannels(const Item &item) const
    {
        return df_mode_ ? 1 : item.channels;    // DF 生成后图像为单通道
    }
};
