#include "JobRunner.h"
#include "CoreApi.h"

#include "texconv/tex_core.h"
#include "texconv/tex_result.h"

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
            Q_EMIT ProbeResult(job.row,
                             info.width, info.height,
                             info.channels, info.layout, info.pixel_type,
                             info.has_alpha != 0,
                             coreapi::DefaultSlotFormat(info.channels));
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

void JobRunner::RunConvert(QList<ConvertItem> jobs)
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

        TexJobParams params{};
        params.input_path    = reinterpret_cast<const wchar_t *>(job.path.utf16());
        params.target_format = format.constData();      // 逐文件显式格式(所见即所得)
        // output_path = NULL:产物与源图同目录 .Tex2D(与 CLI/工作流一致)

        const int rc = TexCore_RunJob(&params, &JobRunner::ProgressSink, this);

        if(rc == TEX_OK)
        {
            Tex2DInfo out{};

            qint64 size = -1;

            if(TexCore_ReadInfo(
                   reinterpret_cast<const wchar_t *>(job.path.utf16()), &out) == TEX_OK)
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
