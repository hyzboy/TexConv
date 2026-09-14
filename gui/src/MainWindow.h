#pragma once
// TexConvQt 主窗口:
//   左侧文件队列(拖拽添加) + 右上按钮列(检测/转换/移除/清空) + 底部日志与进度。

#include <QMainWindow>
#include <QThread>

#include "JobRunner.h"
#include "TexFileModel.h"

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTableView;
class QCheckBox;
class FormatDelegate;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:

    explicit MainWindow(const QStringList &initial_files,
                        QWidget *parent = nullptr);
    ~MainWindow() override;

protected:

    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private Q_SLOTS:

    void OnDetect();
    void OnConvert();
    void OnRemoveSelected();
    void OnClear();

    void OnCurrentRowChanged(const QModelIndex &current, const QModelIndex &previous);

    // JobRunner 回发(UI 线程)
    void OnProbeResult(int row, quint32 w, quint32 h,
                       int channels, int layout, int pixel_type, bool has_alpha,
                       const QString &default_format);
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

    QThread      worker_thread_;
    JobRunner   *runner_ = nullptr;

    int  batch_total_ = 0;
    int  batch_done_  = 0;
    bool busy_ = false;
};
