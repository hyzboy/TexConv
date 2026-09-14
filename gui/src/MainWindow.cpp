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
#include <QInputDialog>
#include <QMenu>
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

    // ---- 批次级设置行(全部转换选项均为逐文件,见右键菜单) ----
    auto options = new QHBoxLayout;

    auto mode_hint = new QLabel(QStringLiteral("全部选项逐文件配置:选中行右键批量设置(法线/单通道按文件名自动识别)"), this);

    auto outdir_label = new QLabel(QStringLiteral("输出:"), this);
    outdir_edit_ = new QLineEdit(this);
    outdir_edit_->setPlaceholderText(QStringLiteral("(留空 = 与源图同目录)"));
    outdir_btn_ = new QPushButton(QStringLiteral("浏览..."), this);

    options->addWidget(mode_hint);
    options->addSpacing(12);
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
    table_->horizontalHeader()->resizeSection(TexFileModel::ColFlags, 150);
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

    // 右键菜单:批量设置每文件配置(格式/法线/距离场/阈值)
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table_, &QWidget::customContextMenuRequested,
            this, &MainWindow::OnContextMenu);
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
        if(a == QStringLiteral("--mip"))          model_->SetFilesMipmaps(model_->AllRowIndexes(), true);
        else if(a == QStringLiteral("--gray"))    model_->SetFilesGrayscale(model_->AllRowIndexes(), true);
        else if(a == QStringLiteral("--discard")) model_->SetFilesDiscardAlpha(model_->AllRowIndexes(), true);
        else if(a == QStringLiteral("--normal"))  model_->SetFilesNormal(model_->AllRowIndexes(), true);
        else if(a == QStringLiteral("--single"))  model_->SetFilesSingleChannel(model_->AllRowIndexes(), true);
        else if(a == QStringLiteral("--df"))      model_->SetFilesDF(model_->AllRowIndexes(), true);
        else if(a.startsWith(QStringLiteral("--df-threshold:")))
            model_->SetFilesDFThreshold(model_->AllRowIndexes(), a.mid(14).toInt());
        else if(a.startsWith(QStringLiteral("--provider:")))
            model_->SetFilesProvider(model_->AllRowIndexes(), a.mid(11));
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
        job.row            = desc.row;
        job.path           = desc.path;
        job.format         = desc.format;
        job.gen_mipmaps    = desc.gen_mipmaps;
        job.force_grayscale = desc.force_grayscale;
        job.discard_alpha  = desc.discard_alpha;
        job.normal_map     = desc.normal_map;
        job.df_mode        = desc.df_mode;
        job.df_threshold   = desc.df_threshold;
        job.provider       = desc.provider;
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
    opts.output_dir       = outdir_edit_->text().trimmed();

    QMetaObject::invokeMethod(runner_, "RunConvert",
                              Qt::QueuedConnection,
                              Q_ARG(QList<ConvertItem>, jobs),
                              Q_ARG(ConvertOptions, opts));
}

void MainWindow::OnContextMenu(const QPoint &pos)
{
    if(busy_)
        return;

    const QModelIndexList selected = table_->selectionModel()->selectedRows();

    if(selected.isEmpty())
        return;

    // 参与"交集计算"的行:检测通过的行
    QModelIndexList ready_rows;
    bool any_normal = false, all_normal = false;
    bool any_df = false, all_df = false;
    QStringList allowed;                    // 选中行有效通道数下可选格式的交集

    {
        bool first = true;

        for(const QModelIndex &idx : selected)
        {
            const auto &item = model_->At(idx.row());

            if(item.state != TexFileModel::Ready)
                continue;

            ready_rows.append(idx);

            any_normal |= item.normal_map;
            any_df     |= item.df_mode;
            all_normal  = first ? item.normal_map : (all_normal && item.normal_map);
            all_df      = first ? item.df_mode     : (all_df && item.df_mode);

            const QStringList af = coreapi::AllowedFormats(model_->EffectiveChannelsAt(idx.row()));

            if(first)
            {
                allowed = af;
            }
            else
            {
                QSet<QString> keep(af.cbegin(), af.cend());
                QStringList both;

                for(const QString &f : allowed)
                    if(keep.contains(f))
                        both << f;

                allowed = both;
            }

            first = false;
        }
    }

    allowed.sort(Qt::CaseInsensitive);

    QMenu menu(this);

    // ---- 目标格式 ----
    QMenu *fmt_menu = menu.addMenu(QStringLiteral("设置目标格式"));

    if(ready_rows.isEmpty() || allowed.isEmpty())
        fmt_menu->setEnabled(false);

    for(const QString &fmt : allowed)
        fmt_menu->addAction(fmt, [this, ready_rows, fmt]
        {
            model_->SetFilesFormat(ready_rows, fmt);
        });

    // ---- 法线 / 距离场 ----
    bool any_single = false, all_single = false;
    bool first = true;

    for(const QModelIndex &idx : selected)
    {
        const auto &item = model_->At(idx.row());

        all_single  = first ? item.single_channel : (all_single && item.single_channel);
        any_single |= item.single_channel;

        first = false;
    }

    QAction *single_action = menu.addAction(QStringLiteral("单通道(转灰度+丢弃Alpha)"));
    single_action->setCheckable(true);
    single_action->setChecked(all_single);
    single_action->setToolTip(QStringLiteral("按单通道语义转换(自动识别:Roughness/Displacement/"
                                             "Metallic/Alpha/Opacity/Luminance/Height/Bump/AO 等)"));
    connect(single_action, &QAction::triggered, this, [this, ready_rows, all_single](bool)
    {
        model_->SetFilesSingleChannel(ready_rows, !all_single);
    });

    QAction *normal_action = menu.addAction(QStringLiteral("法线贴图(BC5)"));
    normal_action->setCheckable(true);
    normal_action->setChecked(all_normal);
    normal_action->setEnabled(!ready_rows.isEmpty());
    connect(normal_action, &QAction::triggered, this, [this, ready_rows, all_normal](bool)
    {
        model_->SetFilesNormal(ready_rows, !all_normal);
    });

    QAction *df_action = menu.addAction(QStringLiteral("距离场"));
    df_action->setCheckable(true);
    df_action->setChecked(all_df);
    df_action->setEnabled(!ready_rows.isEmpty());
    connect(df_action, &QAction::triggered, this, [this, ready_rows, all_df](bool)
    {
        model_->SetFilesDF(ready_rows, !all_df);
    });

    QAction *th_action = menu.addAction(QStringLiteral("距离场阈值..."));
    th_action->setEnabled(any_df);
    connect(th_action, &QAction::triggered, this, [this, selected]
    {
        int current = 128;

        for(const QModelIndex &idx : selected)
        {
            if(model_->At(idx.row()).df_mode)
            {
                current = model_->At(idx.row()).df_threshold;
                break;
            }
        }

        bool ok = false;
        const int v = QInputDialog::getInt(this,
                                           QStringLiteral("距离场阈值"),
                                           QStringLiteral("内外判定阈值(1-255):"),
                                           current, 1, 255, 1, &ok);

        if(ok)
            model_->SetFilesDFThreshold(selected, v);
    });

    menu.addSeparator();

    // ---- 通用转换选项(逐文件) ----
    bool any_mip = false, all_mip = false;
    bool any_gray = false, all_gray = false;
    bool any_discard = false, all_discard = false;
    first = true;

    for(const QModelIndex &idx : selected)
    {
        const auto &item = model_->At(idx.row());

        all_mip     = first ? item.gen_mipmaps     : (all_mip && item.gen_mipmaps);
        any_mip    |= item.gen_mipmaps;
        all_gray    = first ? item.force_grayscale : (all_gray && item.force_grayscale);
        any_gray   |= item.force_grayscale;
        all_discard = first ? item.discard_alpha   : (all_discard && item.discard_alpha);
        any_discard |= item.discard_alpha;

        first = false;
    }

    QAction *mip_action = menu.addAction(QStringLiteral("生成 Mipmap"));
    mip_action->setCheckable(true);
    mip_action->setChecked(all_mip);
    connect(mip_action, &QAction::triggered, this, [this, selected, all_mip](bool)
    {
        model_->SetFilesMipmaps(selected, !all_mip);
    });

    QAction *gray_action = menu.addAction(QStringLiteral("转灰度"));
    gray_action->setCheckable(true);
    gray_action->setChecked(all_gray);
    connect(gray_action, &QAction::triggered, this, [this, selected, all_gray](bool)
    {
        model_->SetFilesGrayscale(selected, !all_gray);
    });

    QAction *discard_action = menu.addAction(QStringLiteral("丢弃 Alpha"));
    discard_action->setCheckable(true);
    discard_action->setChecked(all_discard);
    connect(discard_action, &QAction::triggered, this, [this, selected, all_discard](bool)
    {
        model_->SetFilesDiscardAlpha(selected, !all_discard);
    });

    // ---- 压缩后端(逐文件) ----
    QMenu *provider_menu = menu.addMenu(QStringLiteral("压缩后端"));

    {
        // 当前选中行的后端是否一致(全一致则打勾对应项)
        QString uniform;
        bool same = true, first_p = true;

        for(const QModelIndex &idx : selected)
        {
            const QString &p = model_->At(idx.row()).provider;

            if(first_p) { uniform = p; first_p = false; }
            else if(p != uniform) { same = false; break; }
        }

        auto add_provider = [this, provider_menu, selected, uniform, same]
                            (const QString &display, const QString &value)
        {
            QAction *act = provider_menu->addAction(display);
            act->setCheckable(true);
            act->setChecked(same && uniform == value);

            connect(act, &QAction::triggered, this, [this, selected, value](bool)
            {
                model_->SetFilesProvider(selected, value);
            });
        };

        add_provider(QStringLiteral("默认(优先 AMD)"), QString());

        TexProviderInfo infos[8];
        const int n = TexCore_EnumProviders(infos, 8);

        for(int i = 0; i < n; i++)
            add_provider(QString::fromLatin1(infos[i].short_name),
                         QString::fromLatin1(infos[i].short_name));
    }

    menu.addSeparator();
    menu.addAction(QStringLiteral("移除选中"), this, &MainWindow::OnRemoveSelected);

    menu.exec(table_->viewport()->mapToGlobal(pos));
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
