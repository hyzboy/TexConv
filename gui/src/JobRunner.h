#pragma once
// 工作线程对象:检测(ProbeImage)与转换(RunJob)都在此线程串行执行,
// 结果经信号(自动排队连接)回 UI 线程;内核日志经 LogBridge 单例转发。

#include <QList>
#include <QObject>
#include <QString>
#include <atomic>

/// 内核日志桥:TexCore_Init 的回调可能来自任意线程,
/// 经信号(跨线程自动排队)送达 UI 线程的日志面板。
class LogBridge : public QObject
{
    Q_OBJECT

public:

    static LogBridge &Instance()
    {
        static LogBridge bridge;
        return bridge;
    }

    void Emit(const QString &line) { Q_EMIT LogLine(line); }

Q_SIGNALS:

    void LogLine(const QString &text);

private:

    LogBridge() = default;
};

/// 批量转换选项(仅剩批次级设置;全部转换选项均为逐文件配置,见 ConvertItem)
struct ConvertOptions
{
    QString output_dir;                 // 空 = 与源图同目录;非空 = 产物统一写到该目录
};

Q_DECLARE_METATYPE(ConvertOptions)

/// 转换任务描述(行号在批处理期间保持稳定:转换锁定禁止增删)
struct ConvertItem
{
    int     row = -1;
    QString path;               // 普通文件:源路径;Cube:公共前缀(输出 = 前缀.TexCube)
    QString format;             // 目标格式名(显式 target_format)
    bool    is_cube = false;
    QStringList faces;          // cube:6 张面图,顺序 +X,-X,+Y,-Y,+Z,-Z

    // ---- 逐文件转换选项 ----
    bool    gen_mipmaps = false;
    bool    force_grayscale = false;
    bool    discard_alpha = false;
    bool    normal_map = false;
    bool    single_channel = false;     // 单通道语义(等价 灰度+丢弃Alpha)
    int     ibl_mode = 0;               // IBL 模式: 0=无 1=同时生成 Irradiance+Prefiltered
    QString ibl_format;                 // IBL 产物格式:空=自动(BC6H 优先) BC6H/BC7/RGBA16F
    bool    df_mode = false;
    int     df_threshold = 128;
    QString provider;           // 空 = 默认(优先 Intel)
};

/// 检测任务描述
struct ProbeItem
{
    int     row = -1;
    QString path;
    bool    is_cube = false;
    QStringList faces;      // cube:6 张面图(检测取第 0 张)
};

Q_DECLARE_METATYPE(ConvertItem)
Q_DECLARE_METATYPE(ProbeItem)

class JobRunner : public QObject
{
    Q_OBJECT

public:

    explicit JobRunner(QObject *parent = nullptr);

    /// UI 线程调用:请求取消当前批处理
    void RequestCancel() { cancelled_.store(true); }

public Q_SLOTS:

    /// 批量检测:逐文件 TexCore_ProbeImage
    void RunProbe(QList<ProbeItem> jobs);

    /// 批量转换:逐文件 TexCore_RunJob(串行)
    void RunConvert(QList<ConvertItem> jobs, ConvertOptions opts);

Q_SIGNALS:

    void ProbeResult(int row, quint32 width, quint32 height,
                     int channels, int layout, int pixel_type, bool has_alpha);
    void ProbeFailed(int row, const QString &reason);

    void ConvertDone(int row, qint64 output_size);
    void ConvertFailed(int row, const QString &reason);
    void ConvertCancelled(int row);

    /// 当前文件内进度(mip 级),fraction ∈ (0,1]
    void FileProgress(float fraction);
    /// 批处理总进度(完成文件数)
    void BatchProgress(int done, int total);
    /// 一批结束;cancelled 表示中途中止
    void BatchFinished(int done, int failed, int cancelled, bool cancelled_by_user);

private:

    static int ProgressSink(void *user, float fraction);

    std::atomic<bool> cancelled_{false};
};
