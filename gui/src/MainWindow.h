#pragma once
// TexConvQt 主窗口:
//   左侧文件队列(拖拽添加) + 右上按钮列(检测/转换/移除/清空) + 底部日志与进度。

#include <QMainWindow>
#include <QThread>
#include <QTimer>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;

#include "JobRunner.h"
#include "TexFileModel.h"

class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTableView;
class FormatDelegate;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:

    explicit MainWindow(const QStringList &initial_files,
                        QWidget *parent = nullptr);
    ~MainWindow() override;

    /// 无头自动化:启动后自动 检测→转换→退出(退出码 0=全部成功)。
    /// 供脚本/CI 冒烟测试使用。
    void SetAutoRun(bool on)
    {
        auto_run_ = on;

        if(auto_run_)
            QTimer::singleShot(0, this, [this] { OnDetect(); });    // 启动第一次检测
    }

    /// 解析 --auto 模式的选项开关并应用到对应控件:
    ///   --mip --gray --discard --normal --df --df-threshold:N
    ///   --provider:NAME --outdir:DIR
    void ApplyCommandLineOptions(const QStringList &args);

protected:

    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private Q_SLOTS:

    void OnDetect();
    void OnConvert();
    void OnRemoveSelected();
    void OnClear();
    void OnContextMenu(const QPoint &pos);

    void OnCurrentRowChanged(const QModelIndex &current, const QModelIndex &previous);

    // JobRunner 回发(UI 线程)
    void OnProbeResult(int row, quint32 w, quint32 h,
                       int channels, int layout, int pixel_type, bool has_alpha);
    void OnProbeFailed(int row, const QString &reason);
    void OnConvertDone(int row, qint64 output_size);
    void OnConvertFailed(int row, const QString &reason);
    void OnConvertCancelled(int row);
    void OnFileProgress(float fraction);
    void OnBatchProgress(int done, int total);
    void OnBatchFinished(int done, int failed, int cancelled, bool cancelled_by_user);
    void OnLogLine(const QString &text);

private:

    void BuildUi();
    void CreateRunner();
    void UpdateButtons();
    void AddDroppedUrls(const QList<QUrl> &urls);
    void CollectPathsRecursive(const QString &dir, bool recursive, QStringList &out);

    TexFileModel   *model_   = nullptr;
    FormatDelegate *format_delegate_ = nullptr;
    QTableView     *table_   = nullptr;
    QPlainTextEdit *log_     = nullptr;
    QProgressBar   *progress_ = nullptr;
    QLabel         *progress_label_ = nullptr;
    QLabel         *status_label_ = nullptr;
    QCheckBox      *recursive_check_ = nullptr;

    QPushButton *detect_btn_  = nullptr;
    QPushButton *convert_btn_ = nullptr;
    QPushButton *cancel_btn_  = nullptr;
    QPushButton *remove_btn_  = nullptr;
    QPushButton *clear_btn_   = nullptr;

    // 转换选项行
    QCheckBox  *mip_check_    = nullptr;
    QCheckBox  *gray_check_   = nullptr;
    QCheckBox  *discard_check_ = nullptr;
    QComboBox  *provider_combo_ = nullptr;
    QLineEdit  *outdir_edit_  = nullptr;
    QPushButton *outdir_btn_  = nullptr;

    QThread      worker_thread_;
    JobRunner   *runner_ = nullptr;

    int  batch_total_ = 0;
    int  batch_done_  = 0;
    bool busy_ = false;
    bool auto_run_ = false;     // --auto:检测批结束自动转转换批;转换批结束自动退出
    int  auto_phase_ = 0;       // 1=检测批 2=转换批
};
