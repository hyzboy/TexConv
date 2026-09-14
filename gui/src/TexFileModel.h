#pragma once
// 文件队列表格模型。
// 行生命周期:Pending →(检测)→ Ready / NotImage →(转换)→ Converting → Done / Failed / Cancelled
// 转换进行中模型进入只读锁定(禁止增删),行号保持稳定,工作线程以行号回发结果。
//
// 全部转换选项均为"每文件"配置(非全局):
//   * 生成 Mipmap / 转灰度 / 丢弃 Alpha / 法线 / 距离场(阈值) / 压缩后端;
//   * 法线在添加文件时按文件名自动识别(含 normal/nmap,或以 _n/-n/ n 结尾),可手动改;
//   * 右键菜单可对选中行批量设置;
//   * "标记"列汇总显示一行已启用的全部选项;
//   * 目标格式的可选项与默认值按"计预处理后的有效通道数"推导(与内核一致)。

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
        ColFlags,       // 标记:该行已启用的全部选项汇总
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
        EffectiveChannelsRole,              // int:计预处理后的有效通道数(委托过滤用)
        NormalLockedRole,                   // bool:该行法线模式,目标格式锁定 BC5
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
        QString  target_format;             // 当前选择的目标格式(检测后按选项推导)
        QString  result_text;               // 转换结果/失败原因
        qint64   output_size = -1;
        QString  probe_fail_text;           // 检测失败原因

        // ---- 每文件转换选项 ----
        bool     gen_mipmaps = false;       // 生成 mipmap
        bool     force_grayscale = false;   // 转灰度
        bool     discard_alpha = false;     // 丢弃 alpha
        bool     normal_map = false;        // 法线贴图:目标格式强制 BC5
        bool     df_mode = false;           // 距离场:对灰度(1ch)或 Alpha 生成后按 1 通道继续
        int      df_threshold = 128;        // 距离场内外判定阈值
        QString  provider;                  // 压缩后端 short_name;空 = 默认(优先 AMD)
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
    /// 批量添加文件(去重,同路径只留一条;按文件名自动识别法线贴图);返回实际新增数
    int AddPaths(const QStringList &paths);

    /// 移除指定行(转换锁定期间调用无效)
    bool RemoveRows(const QModelIndexList &selected);

    void Clear();

    /// 检测前置位:对应行状态 → Probing
    void MarkProbing(int row);

    /// 回填检测结果(默认目标格式按该行选项推导)
    void SetProbeResult(int row, quint32 w, quint32 h,
                        int channels, int layout, int pixel_type, bool has_alpha);

    void SetProbeFailed(int row, const QString &reason);

    /// 转换前置位
    void MarkConverting(int row);

    void SetConvertDone(int row, qint64 output_size);
    void SetConvertFailed(int row, const QString &reason);
    void SetConvertCancelled(int row);

    // --- 每文件配置(作用于选中行;转换锁定期间调用无效) ---
    void SetFilesMipmaps(const QModelIndexList &rows, bool on);
    void SetFilesGrayscale(const QModelIndexList &rows, bool on);
    void SetFilesDiscardAlpha(const QModelIndexList &rows, bool on);
    void SetFilesNormal(const QModelIndexList &rows, bool on);
    void SetFilesDF(const QModelIndexList &rows, bool on);
    void SetFilesDFThreshold(const QModelIndexList &rows, int threshold);
    void SetFilesProvider(const QModelIndexList &rows, const QString &provider);
    void SetFilesFormat(const QModelIndexList &rows, const QString &format);

    /// 只读锁定(转换期间禁止增删与配置变更)
    void SetLocked(bool locked) { locked_ = locked; }
    bool IsLocked() const { return locked_; }

    // --- 工作线程取快照 ---
    struct JobDesc
    {
        int      row;
        QString  path;
        QString  format;
        bool     gen_mipmaps;
        bool     force_grayscale;
        bool     discard_alpha;
        bool     normal_map;
        bool     df_mode;
        int      df_threshold;
        QString  provider;
    };

    /// 取所有"检测通过"行的任务快照
    std::vector<JobDesc> CollectReadyJobs() const;

    /// 取指定行通道数(委托/其它 UI 用)
    int ChannelsAt(int row) const { return At(row).channels; }

    /// 取指定行有效通道数(计预处理;右键交集过滤用)
    int EffectiveChannelsAt(int row) const { return EffectiveChannels(At(row)); }

    const Item &At(int row) const { return items_[size_t(row)]; }

    int TotalCount() const { return int(items_.size()); }
    int ReadyCount() const;

    /// 全部行的行号列表(右键"应用到全部"用)
    QModelIndexList AllRowIndexes() const;

Q_SIGNALS:

    void CountsChanged(int total, int ready);

private:

    /// 按该行当前选项推导默认目标格式
    QString DeriveTarget(const Item &item) const;

    /// 计预处理(灰度/丢弃Alpha/距离场)后的有效通道数(对齐内核 ConvertImage 顺序)
    int EffectiveChannels(const Item &item) const;

    void RefreshItem(int row);
    void BumpCounts();

    std::vector<Item>   items_;
    QSet<QString>       path_set_;      // 去重(小写化路径)
    bool                locked_ = false;
};
