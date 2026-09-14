#include "FormatDelegate.h"
#include "CoreApi.h"
#include "TexFileModel.h"

#include <QComboBox>

QWidget *FormatDelegate::createEditor(QWidget *parent,
                                      const QStyleOptionViewItem &option,
                                      const QModelIndex &index) const
{
    Q_UNUSED(option);

    const int channels = index.data(TexFileModel::ChannelsRole).toInt();

    if(channels < 1 || channels > 4)
        return nullptr;             // 未检测/非图片行不可编辑

    const QStringList formats = coreapi::AllowedFormats(channels);

    if(formats.isEmpty())
        return nullptr;

    auto combo = new QComboBox(parent);
    combo->addItems(formats);
    combo->setFrame(false);
    return combo;
}

void FormatDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    auto combo = qobject_cast<QComboBox *>(editor);

    if(!combo)
        return;

    const QString current = index.data(Qt::DisplayRole).toString();
    const int at = combo->findText(current);

    if(at >= 0)
        combo->setCurrentIndex(at);
}

void FormatDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                  const QModelIndex &index) const
{
    auto combo = qobject_cast<QComboBox *>(editor);

    if(!combo)
        return;

    model->setData(index, combo->currentText(), Qt::EditRole);
}
