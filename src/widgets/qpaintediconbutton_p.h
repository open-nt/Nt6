// MIT License - see LICENSE
#pragma once

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qtoolbutton.h>

#include <functional>
#include <utility>

namespace QtPrivate {

// An icon button inside one of Material's pickers, such as the one that switches a date picker to its input mode: a
// standard icon button, which the style draws with its state layer, focus ring and ripple, and over it the icon that
// its widget paints. Space, Return and Enter press it.
class PaintedIconButton : public QToolButton {
public:
    PaintedIconButton(QWidget *parent, std::function<void(QPainter *)> icon)
        : QToolButton(parent), m_icon(std::move(icon)) {
        setProperty("materialIconButton", QStringLiteral("standard"));
        setAutoRaise(true);
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        QToolButton::paintEvent(event);
        QPainter painter(this);
        m_icon(&painter);
    }
    void keyPressEvent(QKeyEvent *event) override {
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && !event->isAutoRepeat()) {
            click();
            event->accept();
            return;
        }
        QToolButton::keyPressEvent(event);
    }

private:
    std::function<void(QPainter *)> m_icon;
};

} // namespace QtPrivate
