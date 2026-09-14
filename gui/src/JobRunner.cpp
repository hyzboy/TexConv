#include "JobRunner.h"
#include "CoreApi.h"

#include "texconv/tex_core.h"
#include "texconv/tex_result.h"

#include <QDir>
#include <QFileInfo>

#include <atomic>

namespace
{
    void CoreLogSink(void *user, int level, const char *utf8_line)
    {
        Q_UNUSED(level);

        if(!utf8_line)
            return;

        LogBridge::Instance().Emit(QString::fromUtf8(utf8_line));
    }
}//namespace

JobRunner::JobRunner(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<ConvertItem>("ConvertItem");
    qRegisterMetaType<ProbeItem>("ProbeItem");
    qRegisterMetaType<QList<ConvertItem>>("QList<ConvertItem>");
    qRegisterMetaType<QList<ProbeItem>>("QList<ProbeItem>");
    qRegisterMetaType<ConvertOptions>("ConvertOptions");
}

int JobRunner::ProgressSink(void *user, float fraction)
{
    auto self = static_cast<JobRunner *>(user);

    Q_EMIT self->FileProgress(fraction);

    return self->cancelled_.load() ? 1 : 0;
}

void JobRunner::RunProbe(QList<ProbeItem> jobs)
{
    cancelled_.store(false);

    const int total = jobs.size();
    int done = 0, failed = 0;

    for(const ProbeItem &job : jobs)
    {
        if(cancelled_.load())
            break;

        TexImageInfo info{};

        const int rc = TexCore_ProbeImage(
            reinterpret_cast<const wchar_t *>(job.path.utf16()), &info);

        if(rc == TEX_OK)
        {
            // 默认目标格式由模型按当前选项推导(法线/DF 会改变默认)
            Q_EMIT ProbeResult(job.row,
                               info.width, info.height,
                               info.channels, info.layout, info.pixel_type,
                               info.has_alpha != 0);
        }
        else
        {
            ++failed;
            Q_EMIT ProbeFailed(job.row, coreapi::ErrorText(rc));
        }

        ++done;
        Q_EMIT BatchProgress(done, total);
    }

    Q_EMIT BatchFinished(done - failed, failed, 0, cancelled_.load());
}

void JobRunner::RunConvert(QList<ConvertItem> jobs, ConvertOptions opts)
{
    cancelled_.store(false);

    const int total = jobs.size();
    int done = 0, failed = 0, cancelled = 0;

    for(const ConvertItem &job : jobs)
    {
        if(cancelled_.load())
        {
            // 未开跑的行统一标记取消
            Q_EMIT ConvertCancelled(job.row);
            ++cancelled;
            continue;
        }

        const QByteArray format = job.format.toLatin1();
        const QByteArray provider = job.provider.toLatin1();    // 空 = 内核默认(优先 AMD)

        // 输出目录:空 = 与源图同目录(内核 output_path=NULL 语义);
        // 非空 = 显式路径,内核自动补 .Tex2D 后缀(这里直接拼全,所见即所得)
        QString output_path;

        if(!opts.output_dir.isEmpty())
            output_path = QDir(opts.output_dir)
                              .filePath(QFileInfo(job.path).completeBaseName() + ".Tex2D");

        TexJobParams params{};
        params.input_path      = reinterpret_cast<const wchar_t *>(job.path.utf16());
        params.output_path     = output_path.isEmpty()
                               ? nullptr
                               : reinterpret_cast<const wchar_t *>(output_path.utf16());
        params.target_format   = format.constData();        // 逐文件显式格式(所见即所得)

        // ---- 全部转换选项均为逐文件 ----
        params.gen_mipmaps     = job.gen_mipmaps ? 1 : 0;
        params.force_grayscale = (job.force_grayscale || job.single_channel) ? 1 : 0;
        params.discard_alpha   = (job.discard_alpha || job.single_channel) ? 1 : 0;
        params.normal_map      = job.normal_map ? 1 : 0;
        params.df_mode         = job.df_mode ? 1 : 0;
        params.df_threshold    = job.df_threshold;

        if(!provider.isEmpty())
            params.provider = provider.constData();

        const int rc = TexCore_RunJob(&params, &JobRunner::ProgressSink, this);

        if(rc == TEX_OK)
        {
            const QString out_for_read = output_path.isEmpty()
                                       ? job.path
                                       : output_path;   // 两者内核都能读回

            Tex2DInfo out{};

            qint64 size = -1;

            if(TexCore_ReadInfo(
                   reinterpret_cast<const wchar_t *>(out_for_read.utf16()), &out) == TEX_OK)
                size = out.file_size;

            Q_EMIT ConvertDone(job.row, size);
            ++done;
        }
        else if(rc == TEX_ERR_CANCELLED)
        {
            Q_EMIT ConvertCancelled(job.row);
            ++cancelled;
        }
        else
        {
            Q_EMIT ConvertFailed(job.row, coreapi::ErrorText(rc));
            ++failed;
        }

        Q_EMIT BatchProgress(done + failed + cancelled, total);
    }

    Q_EMIT BatchFinished(done, failed, cancelled, cancelled_.load());
}
