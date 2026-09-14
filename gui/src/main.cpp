// TexConvQt 入口:QApplication + 转换内核生命周期(RAII 配对)。
// 命令行参数(可选):预填文件列表,便于自动化冒烟测试。

#include <QApplication>
#include <QMessageBox>
#include <QStringList>

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
}//namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("TexConvQt"));
    app.setOrganizationName(QStringLiteral("ULRE"));

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

    MainWindow window(QCoreApplication::arguments().mid(1));
    window.show();

    return app.exec();
}
