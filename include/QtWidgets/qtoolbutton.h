// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtGui/qaction.h>
#include <QtWidgets/qabstractbutton.h>

class QToolButton : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(bool autoRaise READ autoRaise WRITE setAutoRaise)
    Q_PROPERTY(Qt::ArrowType arrowType READ arrowType WRITE setArrowType)
    Q_PROPERTY(Qt::ToolButtonStyle toolButtonStyle READ toolButtonStyle WRITE setToolButtonStyle)
public:
    explicit QToolButton(QWidget *parent = nullptr);
    ~QToolButton() override;

    bool autoRaise() const { return m_autoRaise; }
    void setAutoRaise(bool enable);
    Qt::ArrowType arrowType() const { return m_arrow; }
    void setArrowType(Qt::ArrowType type);
    Qt::ToolButtonStyle toolButtonStyle() const { return m_style; }
    QAction *defaultAction() const { return m_defaultAction.data(); }

    QSize sizeHint() const override;

public slots:
    void setToolButtonStyle(Qt::ToolButtonStyle style);
    void setDefaultAction(QAction *action);

signals:
    /*!
        This signal is emitted when \a action, an action of the button such as its default action, is triggered.
    */
    void triggered(QAction *action) { emitSignal(&QToolButton::triggered, action); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void actionEvent(QActionEvent *event) override;

private:
    void syncAction();

    bool m_autoRaise = false;
    Qt::ArrowType m_arrow = Qt::NoArrow;
    Qt::ToolButtonStyle m_style = Qt::ToolButtonIconOnly;
    QPointer<QAction> m_defaultAction;
    QMetaObject::Connection m_triggered;
};
