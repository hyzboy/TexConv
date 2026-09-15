// TexConvQt 入口:QApplication + 转换内核生命周期(RAII 配对)。
// 命令行参数(可选):预填文件列表,便于自动化冒烟测试。

#include <QApplication>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStringList>
#include <QPalette>
#include <QStyleFactory>

#include <windows.h>

#include "JobRunner.h"
#include "MainWindow.h"

#include "texconv/tex_core.h"

namespace
{
    void CoreLogSink(void *user, int level, const char *utf8_line)
    {
        Q_UNUSED(user);
        Q_UNUSED(level);
        LogBridge::Instance().Emit(QString::fromUtf8(utf8_line));
    }

    /// Fusion 风格 + 深色调色板
    void ApplyDarkTheme(QApplication &app)
    {
        app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

        QPalette pal;
        pal.setColor(QPalette::Window,          QColor(45, 45, 48));
        pal.setColor(QPalette::WindowText,      QColor(208, 208, 208));
        pal.setColor(QPalette::Base,            QColor(30, 30, 32));
        pal.setColor(QPalette::AlternateBase,   QColor(38, 38, 40));
        pal.setColor(QPalette::ToolTipBase,     QColor(45, 45, 48));
        pal.setColor(QPalette::ToolTipText,     QColor(208, 208, 208));
        pal.setColor(QPalette::Text,            QColor(208, 208, 208));
        pal.setColor(QPalette::Disabled, QPalette::Text, QColor(100, 100, 100));
        pal.setColor(QPalette::Dark,            QColor(30, 30, 32));
        pal.setColor(QPalette::Shadow,          QColor(20, 20, 20));
        pal.setColor(QPalette::Button,          QColor(53, 53, 56));
        pal.setColor(QPalette::ButtonText,      QColor(208, 208, 208));
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(100, 100, 100));
        pal.setColor(QPalette::BrightText,      QColor(255, 80, 80));
        pal.setColor(QPalette::Link,            QColor(42, 130, 218));
        pal.setColor(QPalette::Highlight,       QColor(42, 130, 218));
        pal.setColor(QPalette::HighlightedText, Qt::white);
        pal.setColor(QPalette::Disabled, QPalette::Highlight, QColor(60, 60, 65));
        pal.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(100, 100, 100));

        // Placeholder 文字
        pal.setColor(QPalette::PlaceholderText, QColor(110, 110, 110));

        app.setPalette(pal);
    }
}//namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("TexConvQt"));
    app.setOrganizationName(QStringLiteral("ULRE"));
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    ApplyDarkTheme(app);

    // 初始化转换内核(加载 texenc*.dll 插件、图像库;日志桥接到 UI)
    const TexCoreCallbacks core_cb{ &CoreLogSink, nullptr };

    const int rc = TexCore_Init(&core_cb);

    if(rc != TEX_OK)
    {
        QMessageBox::critical(nullptr,
                              QStringLiteral("TexConvQt"),
                              QStringLiteral("转换内核初始化失败(%1)。\n"
                                             "请确认 TexConvCore.dll / TexImage.dll / texenc*.dll "
                                             "与本程序同目录。").arg(rc));
        return 1;
    }

    struct CoreGuard
    {
        ~CoreGuard() { TexCore_Shutdown(); }
    } core_guard;

    const QStringList args = QCoreApplication::arguments().mid(1);

    MainWindow window(args.filter(QRegularExpression(QStringLiteral("^--"))).isEmpty()
                         ? args
                         : args.filter(QRegularExpression(QStringLiteral("^(?!--)"))));
    window.SetAutoRun(args.contains(QStringLiteral("--auto")));
    window.ApplyCommandLineOptions(args);
    window.show();

    return app.exec();
}
