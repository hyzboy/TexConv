#pragma once
// 目标格式列的编辑委托:选中行在"目标格式"单元格弹出 QComboBox,
// 选项为该行源通道数经 TexFormat_IsAllowed 过滤后的全部合法格式。

#include <QStyledItemDelegate>

class FormatDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:

    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;
};
