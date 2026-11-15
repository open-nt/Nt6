// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtGui/qpixmap.h>
#include <QtWidgets/qlistview.h>

class QUndoGroup;
class QUndoModel;
class QUndoStack;

class QUndoView : public QListView {
    Q_OBJECT
    Q_PROPERTY(QString emptyLabel READ emptyLabel WRITE setEmptyLabel)
    Q_PROPERTY(QPixmap cleanIcon READ cleanIcon WRITE setCleanIcon)
public:
    explicit QUndoView(QWidget *parent = nullptr);
    explicit QUndoView(QUndoStack *stack, QWidget *parent = nullptr);
    explicit QUndoView(QUndoGroup *group, QWidget *parent = nullptr);
    ~QUndoView() override;

    QUndoStack *stack() const;
    QUndoGroup *group() const;

    void setEmptyLabel(const QString &label);
    QString emptyLabel() const;
    void setCleanIcon(const QPixmap &icon);
    QPixmap cleanIcon() const;

public slots:
    void setStack(QUndoStack *stack);
    void setGroup(QUndoGroup *group);

private:
    QUndoModel *m_model;
    QPointer<QUndoGroup> m_group;
    QMetaObject::Connection m_groupConnection;
};
