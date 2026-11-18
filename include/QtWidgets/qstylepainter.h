// MIT License - see LICENSE
#pragma once

#include <QtGui/qpainter.h>
#include <QtGui/qpalette.h>
#include <QtWidgets/qstyle.h>

class QStyleOption;
class QWidget;

class QStylePainter : public QPainter {
public:
    QStylePainter();
    explicit QStylePainter(QWidget *widget);
    QStylePainter(QPaintDevice *device, QWidget *widget);

    bool begin(QWidget *widget);
    bool begin(QPaintDevice *device, QWidget *widget);

    void drawPrimitive(QStyle::PrimitiveElement element, const QStyleOption &option);
    void drawControl(QStyle::ControlElement element, const QStyleOption &option);
    void drawComplexControl(QStyle::ComplexControl cc, const QStyleOptionComplex &option);
    void drawItemText(const QRect &rectangle, int flags, const QPalette &palette, bool enabled, const QString &text,
                      QPalette::ColorRole textRole = QPalette::NoRole);
    void drawItemPixmap(const QRect &rectangle, int flags, const QPixmap &pixmap);
    QStyle *style() const { return m_style; }

private:
    QWidget *m_widget = nullptr;
    QStyle *m_style = nullptr;
};
