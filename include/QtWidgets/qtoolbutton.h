// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qabstractbutton.h>

class QToolButton : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(bool autoRaise READ autoRaise WRITE setAutoRaise)
    Q_PROPERTY(Qt::ArrowType arrowType READ arrowType WRITE setArrowType)
public:
    explicit QToolButton(QWidget *parent = nullptr);
    ~QToolButton() override;

    bool autoRaise() const { return m_autoRaise; }
    void setAutoRaise(bool enable);
    Qt::ArrowType arrowType() const { return m_arrow; }
    void setArrowType(Qt::ArrowType type);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_autoRaise = false;
    Qt::ArrowType m_arrow = Qt::NoArrow;
};
