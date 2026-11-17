// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtGui/qpixmap.h>

#include <memory>

class QMimeData;

class QDrag : public QObject {
    Q_OBJECT
public:
    explicit QDrag(QObject *dragSource);
    ~QDrag() override;

    void setMimeData(QMimeData *data);
    QMimeData *mimeData() const;
    void setPixmap(const QPixmap &pixmap);
    QPixmap pixmap() const;
    void setHotSpot(const QPoint &hotspot);
    QPoint hotSpot() const;
    QObject *source() const;
    QObject *target() const;

    Qt::DropAction exec(Qt::DropActions supportedActions = Qt::MoveAction);
    Qt::DropAction exec(Qt::DropActions supportedActions, Qt::DropAction defaultDropAction);
    Qt::DropActions supportedActions() const;
    Qt::DropAction defaultAction() const;

    static void cancel();

signals:
    /*!
        This signal is emitted when the action the drop would take changes to \a action.
    */
    void actionChanged(Qt::DropAction action) { emitSignal(&QDrag::actionChanged, action); }
    /*!
        This signal is emitted when the drag moves over another target, \a newTarget, or over none.
    */
    void targetChanged(QObject *newTarget) { emitSignal(&QDrag::targetChanged, newTarget); }

private:
    Q_DISABLE_COPY(QDrag)
    struct Data;
    std::unique_ptr<Data> m_d;
};
