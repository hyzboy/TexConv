#include "MainWindow.h"
#include "CoreApi.h"
#include "FormatDelegate.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QSpinBox>
#include <QDropEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QSplitter>
#include <QPushButton>
#include <QShortcut>
#include <QTableView>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

MainWindow::MainWindow(const QStringList &initial_files, QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("TexConv —— 纹理转换工具"));
    resize(1150, 720);
    setAcceptDrops(true);

    BuildUi();
    CreateRunner();

    model_->AddPaths(initial_files);        // 命令行预填(便于自动化测试)

    UpdateButtons();
}

MainWindow::~MainWindow()
{
    worker_thread_.quit();
    worker_thread_.wait();
}

void MainWindow::BuildUi()
{
    auto central = new QWidget(this);
    setCentralWidget(central);

    auto root_layout = new QVBoxLayout(central);
    root_layout->setContentsMargins(8, 8, 8, 8);
    root_layout->setSpacing(6);

    // ---- 工具栏行 ----
    auto toolbar = new QHBoxLayout;
    detect_btn_ = new QPushButton(QStringLiteral("检测"), this);
    detect_btn_->setToolTip(QStringLiteral("扫描列表中的文件是否为图片,获取宽高/通道/像素类型,并填入默认目标格式"));
    convert_btn_ = new QPushButton(QStringLiteral("转换"), this);
    convert_btn_->setToolTip(QStringLiteral("按每行的目标格式批量转换,产物与源图同目录(.Tex2D)"));
    cancel_btn_ = new QPushButton(QStringLiteral("取消"), this);
    cancel_btn_->setVisible(false);
    remove_btn_ = new QPushButton(QStringLiteral("移除选中"), this);
    clear_btn_  = new QPushButton(QStringLiteral("清空"), this);
    recursive_check_ = new QCheckBox(QStringLiteral("包含子目录"), this);

    toolbar->addWidget(detect_btn_);
    toolbar->addWidget(convert_btn_);
    toolbar->addWidget(cancel_btn_);
    toolbar->addWidget(remove_btn_);
    toolbar->addWidget(clear_btn_);
    toolbar->addSpacing(16);
    toolbar->addWidget(recursive_check_);
    toolbar->addStretch(1);
    status_label_ = new QLabel(this);
    toolbar->addWidget(status_label_);

    root_layout->addLayout(toolbar);

    // ---- 转换选项行 ----
    auto options = new QHBoxLayout;

    mip_check_    = new QCheckBox(QStringLiteral("Mipmap"), this);
    mip_check_->setToolTip(QStringLiteral("生成 mipmap 链(压缩格式 4×4 下限,少 2 级)"));
    gray_check_   = new QCheckBox(QStringLiteral("灰度"), this);
    gray_check_->setToolTip(QStringLiteral("强制转灰度(/gray)"));
    discard_check_ = new QCheckBox(QStringLiteral("丢弃Alpha"), this);
    discard_check_->setToolTip(QStringLiteral("丢弃 alpha 通道(/discard_alpha)"));
    normal_check_ = new QCheckBox(QStringLiteral("法线(BC5)"), this);
    normal_check_->setToolTip(QStringLiteral("法线贴图模式:所有目标格式强制 BC5(/normal)"));
    df_check_     = new QCheckBox(QStringLiteral("距离场"), this);
    df_check_->setToolTip(QStringLiteral("距离场模式:单通道对灰度、RGBA 对 Alpha 生成,\n"
                                         "生成后按 1 通道继续(默认格式变为 R8)(/DF)"));

    df_threshold_spin_ = new QSpinBox(this);
    df_threshold_spin_->setRange(1, 255);
    df_threshold_spin_->setValue(128);
    df_threshold_spin_->setPrefix(QStringLiteral("阈值:"));
    df_threshold_spin_->setToolTip(QStringLiteral("距离场内外判定阈值(默认 128)"));
    df_threshold_spin_->setEnabled(false);

    auto provider_label = new QLabel(QStringLiteral("后端:"), this);
    provider_combo_ = new QComboBox(this);
    provider_combo_->addItem(QStringLiteral("默认"), QString());        // data = short_name,空=默认

    {
        TexProviderInfo infos[8];
        const int n = TexCore_EnumProviders(infos, 8);

        for(int i = 0; i < n; i++)
            provider_combo_->addItem(QString::fromLatin1(infos[i].short_name),
                                     QString::fromLatin1(infos[i].short_name));
    }

    provider_combo_->setToolTip(QStringLiteral("块压缩编码后端(来自 texenc*.dll 插件)"));

    auto outdir_label = new QLabel(QStringLiteral("输出:"), this);
    outdir_edit_ = new QLineEdit(this);
    outdir_edit_->setPlaceholderText(QStringLiteral("(留空 = 与源图同目录)"));
    outdir_btn_ = new QPushButton(QStringLiteral("浏览..."), this);

    options->addWidget(mip_check_);
    options->addWidget(gray_check_);
    options->addWidget(discard_check_);
    options->addSpacing(8);
    options->addWidget(normal_check_);
    options->addWidget(df_check_);
    options->addWidget(df_threshold_spin_);
    options->addSpacing(8);
    options->addWidget(provider_label);
    options->addWidget(provider_combo_);
    options->addSpacing(8);
    options->addWidget(outdir_label);
    options->addWidget(outdir_edit_, 1);
    options->addWidget(outdir_btn_);

    root_layout->addLayout(options);

    // ---- 表格 + 日志(上下分割) ----
    auto splitter = new QSplitter(Qt::Vertical, this);

    table_ = new QTableView(splitter);
    model_ = new TexFileModel(this);
    format_delegate_ = new FormatDelegate(table_);
    table_->setModel(model_);
    table_->setItemDelegateForColumn(TexFileModel::ColTarget, format_delegate_);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->setShowGrid(true);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->resizeSection(TexFileModel::ColFile, 260);
    table_->horizontalHeader()->resizeSection(TexFileModel::ColState, 70);
    table_->horizontalHeader()->resizeSection(TexFileModel::ColSize, 90);
    table_->horizontalHeader()->resizeSection(TexFileModel::ColChannels, 50);
    table_->horizontalHeader()->resizeSection(TexFileModel::ColPixelType, 80);
    table_->horizontalHeader()->resizeSection(TexFileModel::ColTarget, 110);
    table_->verticalHeader()->setDefaultSectionSize(24);
    table_->setDragDropMode(QAbstractItemView::NoDragDrop);

    log_ = new QPlainTextEdit(splitter);
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(2000);
    log_->setPlainText(QStringLiteral("拖拽文件或文件夹到窗口,先「检测」再「转换」。"));

    splitter->setStretchFactor(0, 5);
    splitter->setStretchFactor(1, 1);

    root_layout->addWidget(splitter, 1);

    // ---- 进度行 ----
    auto progress_layout = new QHBoxLayout;
    progress_ = new QProgressBar(this);
    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setVisible(false);
    progress_label_ = new QLabel(this);
    progress_label_->setVisible(false);

    progress_layout->addWidget(new QLabel(QStringLiteral("进度:"), this));
    progress_layout->addWidget(progress_, 1);
    progress_layout->addWidget(progress_label_);

    root_layout->addLayout(progress_layout);

    // ---- 信号 ----
    connect(detect_btn_,  &QPushButton::clicked, this, &MainWindow::OnDetect);
    connect(convert_btn_, &QPushButton::clicked, this, &MainWindow::OnConvert);
    connect(cancel_btn_,  &QPushButton::clicked, this, [this] { if(runner_) runner_->RequestCancel(); });
    connect(remove_btn_,  &QPushButton::clicked, this, &MainWindow::OnRemoveSelected);
    connect(clear_btn_,   &QPushButton::clicked, this, &MainWindow::OnClear);

    connect(model_, &TexFileModel::CountsChanged, this, [this](int total, int ready)
    {
        status_label_->setText(QStringLiteral("共 %1 个文件,%2 个可转换").arg(total).arg(ready));
        UpdateButtons();
    });

    connect(table_->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &MainWindow::OnCurrentRowChanged);

    auto del_shortcut = new QShortcut(QKeySequence::Delete, table_);
    del_shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(del_shortcut, &QShortcut::activated, this, &MainWindow::OnRemoveSelected);

    // 法线/DF 切换 → 模型重推默认格式(格式列显示随之刷新)
    connect(normal_check_, &QCheckBox::toggled, this, [this](bool)
    {
        model_->SetOptionFlags(normal_check_->isChecked(), df_check_->isChecked());
        OnCurrentRowChanged(table_->currentIndex(), QModelIndex());
    });
    connect(df_check_, &QCheckBox::toggled, this, [this](bool checked)
    {
        df_threshold_spin_->setEnabled(checked);
        model_->SetOptionFlags(normal_check_->isChecked(), checked);
        OnCurrentRowChanged(table_->currentIndex(), QModelIndex());
    });
    connect(outdir_btn_, &QPushButton::clicked, this, [this]
    {
        const QString dir = QFileDialog::getExistingDirectory(
            this, QStringLiteral("选择输出目录"), outdir_edit_->text());

        if(!dir.isEmpty())
            outdir_edit_->setText(dir);
    });

    // 内核日志(可能来自工作线程)→ 日志面板
    connect(&LogBridge::Instance(), &LogBridge::LogLine,
            this, &MainWindow::OnLogLine, Qt::QueuedConnection);
}

void MainWindow::ApplyCommandLineOptions(const QStringList &args)
{
    for(const QString &a : args)
    {
        if(a == QStringLiteral("--mip"))          mip_check_->setChecked(true);
        else if(a == QStringLiteral("--gray"))    gray_check_->setChecked(true);
        else if(a == QStringLiteral("--discard")) discard_check_->setChecked(true);
        else if(a == QStringLiteral("--normal"))  normal_check_->setChecked(true);
        else if(a == QStringLiteral("--df"))      df_check_->setChecked(true);
        else if(a.startsWith(QStringLiteral("--df-threshold:")))
            df_threshold_spin_->setValue(a.mid(14).toInt());
        else if(a.startsWith(QStringLiteral("--provider:")))
        {
            const QString name = a.mid(11);
            const int at = provider_combo_->findData(name);

            if(at >= 0)
                provider_combo_->setCurrentIndex(at);
        }
        else if(a.startsWith(QStringLiteral("--outdir:")))
            outdir_edit_->setText(a.mid(9));
    }
}

void MainWindow::OnLogLine(const QString &text)
{
    log_->appendPlainText(text);
}

void MainWindow::CreateRunner()
{
    runner_ = new JobRunner;                // 无 parent:moveToThread 要求
    runner_->moveToThread(&worker_thread_);

    connect(&worker_thread_, &QThread::finished, runner_, &QObject::deleteLater);

    connect(runner_, &JobRunner::ProbeResult,  this, &MainWindow::OnProbeResult);
    connect(runner_, &JobRunner::ProbeFailed,  this, &MainWindow::OnProbeFailed);
    connect(runner_, &JobRunner::ConvertDone,  this, &MainWindow::OnConvertDone);
    connect(runner_, &JobRunner::ConvertFailed,this, &MainWindow::OnConvertFailed);
    connect(runner_, &JobRunner::ConvertCancelled, this, &MainWindow::OnConvertCancelled);
    connect(runner_, &JobRunner::FileProgress, this, &MainWindow::OnFileProgress);
    connect(runner_, &JobRunner::BatchProgress,this, &MainWindow::OnBatchProgress);
    connect(runner_, &JobRunner::BatchFinished,this, &MainWindow::OnBatchFinished);

    worker_thread_.start();
}

void MainWindow::UpdateButtons()
{
    const bool has_rows = model_->TotalCount() > 0;
    const bool has_ready = model_->ReadyCount() > 0;

    detect_btn_->setEnabled(!busy_ && has_rows);
    convert_btn_->setEnabled(!busy_ && has_ready);
    remove_btn_->setEnabled(!busy_ && has_rows);
    clear_btn_->setEnabled(!busy_ && has_rows);
    recursive_check_->setEnabled(!busy_);
    cancel_btn_->setVisible(busy_);
    table_->setEnabled(true);               // 表格保持可看,编辑被模型锁定

    mip_check_->setEnabled(!busy_);
    gray_check_->setEnabled(!busy_);
    discard_check_->setEnabled(!busy_);
    normal_check_->setEnabled(!busy_);
    df_check_->setEnabled(!busy_);
    df_threshold_spin_->setEnabled(!busy_ && df_check_->isChecked());
    provider_combo_->setEnabled(!busy_);
    outdir_edit_->setEnabled(!busy_);
    outdir_btn_->setEnabled(!busy_);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if(busy_)
        return;

    AddDroppedUrls(event->mimeData()->urls());
    event->acceptProposedAction();
}

void MainWindow::AddDroppedUrls(const QList<QUrl> &urls)
{
    QStringList files;

    for(const QUrl &url : urls)
    {
        const QString local = url.toLocalFile();

        if(local.isEmpty())
            continue;

        const QFileInfo fi(local);

        if(fi.isDir())
            CollectPathsRecursive(local, recursive_check_->isChecked(), files);
        else if(fi.isFile())
            files << local;
    }

    if(files.isEmpty())
        return;

    const int added = model_->AddPaths(files);

    LogBridge::Instance().Emit(QStringLiteral("已添加 %1 个文件(去重后)。").arg(added));
}

void MainWindow::CollectPathsRecursive(const QString &dir, bool recursive, QStringList &out)
{
    // 内核枚举会跳过 .Tex2D(与 CLI 目录模式一致);
    // 无扩展名白名单 —— 非图片由「检测」标记剔除
    TexCore_EnumDirectory(
        reinterpret_cast<const wchar_t *>(dir.utf16()),
        recursive ? 1 : 0,
        [](void *user, const wchar_t *path) -> int
        {
            // path 是临时对象,必须立刻拷贝
            static_cast<QStringList *>(user)->push_back(
                QString::fromStdWString(std::wstring(path)));
            return 0;
        },
        &out);
}

void MainWindow::OnCurrentRowChanged(const QModelIndex &current, const QModelIndex &previous)
{
    // 上一行关掉编辑器
    if(previous.isValid())
        table_->closePersistentEditor(model_->index(previous.row(), TexFileModel::ColTarget));

    // 选中行打开格式下拉(仅"已就绪"行;法线模式锁定 BC5 不弹)
    if(current.isValid()
     && model_->At(current.row()).state == TexFileModel::Ready
     && !model_->index(current.row(), TexFileModel::ColTarget)
             .data(TexFileModel::NormalLockedRole).toBool()
     && model_->ChannelsAt(current.row()) >= 1)
    {
        table_->openPersistentEditor(model_->index(current.row(), TexFileModel::ColTarget));
    }
}

void MainWindow::OnDetect()
{
    if(busy_)
        return;

    QList<ProbeItem> jobs;

    for(int row = 0; row < model_->TotalCount(); row++)
    {
        const auto &item = model_->At(row);

        // 待检测/已就绪/检测失败均可重新检测
        if(item.state == TexFileModel::Pending
         || item.state == TexFileModel::Ready
         || item.state == TexFileModel::NotImage)
        {
            model_->MarkProbing(row);

            ProbeItem job;
            job.row  = row;
            job.path = item.path;
            jobs.push_back(job);
        }
    }

    if(jobs.isEmpty())
        return;

    busy_ = true;
    batch_total_ = jobs.size();
    batch_done_ = 0;
    if(auto_run_)auto_phase_ = 1;

    progress_->setVisible(true);
    progress_label_->setVisible(true);
    progress_->setValue(0);

    UpdateButtons();

    QMetaObject::invokeMethod(runner_, "RunProbe",
                              Qt::QueuedConnection,
                              Q_ARG(QList<ProbeItem>, jobs));
}

void MainWindow::OnConvert()
{
    if(busy_)
        return;

    const std::vector<TexFileModel::JobDesc> ready = model_->CollectReadyJobs();

    if(ready.empty())
        return;

    QList<ConvertItem> jobs;

    for(const auto &desc : ready)
    {
        model_->MarkConverting(desc.row);

        ConvertItem job;
        job.row    = desc.row;
        job.path   = desc.path;
        job.format = desc.format;
        jobs.push_back(job);
    }

    busy_ = true;
    batch_total_ = jobs.size();
    batch_done_ = 0;
    if(auto_run_)auto_phase_ = 2;

    model_->SetLocked(true);                // 转换期间禁止增删,行号稳定
    table_->closePersistentEditor(table_->currentIndex().siblingAtColumn(TexFileModel::ColTarget));

    progress_->setVisible(true);
    progress_label_->setVisible(true);
    progress_->setValue(0);

    UpdateButtons();

    ConvertOptions opts;
    opts.gen_mipmaps      = mip_check_->isChecked();
    opts.force_grayscale  = gray_check_->isChecked();
    opts.discard_alpha    = discard_check_->isChecked();
    opts.normal_map       = normal_check_->isChecked();
    opts.df_mode          = df_check_->isChecked();
    opts.df_threshold     = df_threshold_spin_->value();
    opts.provider         = provider_combo_->currentData().toString();
    opts.output_dir       = outdir_edit_->text().trimmed();

    QMetaObject::invokeMethod(runner_, "RunConvert",
                              Qt::QueuedConnection,
                              Q_ARG(QList<ConvertItem>, jobs),
                              Q_ARG(ConvertOptions, opts));
}

void MainWindow::OnRemoveSelected()
{
    model_->RemoveRows(table_->selectionModel()->selectedRows());
    OnCurrentRowChanged(table_->currentIndex(), QModelIndex());
}

void MainWindow::OnClear()
{
    model_->Clear();
    UpdateButtons();
}

void MainWindow::OnProbeResult(int row, quint32 w, quint32 h,
                               int channels, int layout, int pixel_type, bool has_alpha)
{
    model_->SetProbeResult(row, w, h, channels, layout, pixel_type, has_alpha);

    // 若该行恰为当前选中行,刷新其格式编辑器
    OnCurrentRowChanged(table_->currentIndex(), QModelIndex());
}

void MainWindow::OnProbeFailed(int row, const QString &reason)
{
    model_->SetProbeFailed(row, reason);
}

void MainWindow::OnConvertDone(int row, qint64 output_size)
{
    model_->SetConvertDone(row, output_size);
}

void MainWindow::OnConvertFailed(int row, const QString &reason)
{
    model_->SetConvertFailed(row, reason);
}

void MainWindow::OnConvertCancelled(int row)
{
    model_->SetConvertCancelled(row);
}

void MainWindow::OnFileProgress(float fraction)
{
    // 总进度 = (已完成文件数 + 当前文件 fraction) / 总数
    const float overall = batch_total_
                        ? float(batch_done_ + fraction) / float(batch_total_)
                        : 0.f;

    progress_->setValue(int(overall * 100 + 0.5f));
}

void MainWindow::OnBatchProgress(int done, int total)
{
    batch_done_ = done;
    batch_total_ = total;

    progress_->setValue(total ? int(done * 100 / total) : 0);
}

void MainWindow::OnBatchFinished(int done, int failed, int cancelled, bool cancelled_by_user)
{
    busy_ = false;
    model_->SetLocked(false);

    progress_->setVisible(false);
    progress_label_->setVisible(false);
    progress_->setValue(0);

    UpdateButtons();
    OnCurrentRowChanged(table_->currentIndex(), QModelIndex());

    const QString summary = cancelled_by_user
        ? QStringLiteral("批处理已取消:完成 %1,失败 %2,取消 %3").arg(done).arg(failed).arg(cancelled)
        : QStringLiteral("批处理完成:成功 %1,失败 %2,取消 %3").arg(done).arg(failed).arg(cancelled);

    status_label_->setText(summary);

    // --auto 流程:检测批结束 → 自动转换;转换批结束 → 自动退出
    if(auto_run_)
    {
        if(auto_phase_ == 1)
        {
            auto_phase_ = 0;
            QTimer::singleShot(0, this, [this] { OnConvert(); });
        }
        else if(auto_phase_ == 2)
        {
            auto_phase_ = 0;
            QCoreApplication::exit((failed == 0 && cancelled == 0) ? 0 : 1);
        }
    }
}
