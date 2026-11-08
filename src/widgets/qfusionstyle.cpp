// MIT License - see LICENSE
#include "qfusionstyle_p.h"

#include <QtGui/qguiapplication.h>
#include <QtGui/qpainter.h>
#include <QtGui/qstylehints.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qwidget.h>

#include <algorithm>

namespace {

QPalette::ColorGroup groupOf(const QStyleOption *option) {
    return (option->state & QStyle::State_Enabled) ? QPalette::Active : QPalette::Disabled;
}

QColor outlineOf(const QStyleOption *option) {
    return option->palette.color(groupOf(option), QPalette::Window).darker(150);
}

QColor mix(const QColor &a, const QColor &b, int percentOfA) {
    auto m = [percentOfA](int x, int y) { return (x * percentOfA + y * (100 - percentOfA)) / 100; };
    return QColor(m(a.red(), b.red()), m(a.green(), b.green()), m(a.blue(), b.blue()));
}

// A box with a one pixel border whose corners are cut, which looks rounded without antialiasing.
void drawBox(QPainter *p, const QRect &r, const QColor &fill, const QColor &border, const QColor &behind) {
    p->fillRect(r.adjusted(1, 1, -1, -1), fill);
    p->setPen(border);
    p->drawLine(r.left() + 1, r.top(), r.right() - 1, r.top());
    p->drawLine(r.left() + 1, r.bottom(), r.right() - 1, r.bottom());
    p->drawLine(r.left(), r.top() + 1, r.left(), r.bottom() - 1);
    p->drawLine(r.right(), r.top() + 1, r.right(), r.bottom() - 1);
    p->setPen(mix(border, behind, 35));
    p->drawPoint(r.left(), r.top());
    p->drawPoint(r.right(), r.top());
    p->drawPoint(r.left(), r.bottom());
    p->drawPoint(r.right(), r.bottom());
}

} // namespace

QFusionStyle::~QFusionStyle() = default;

void QFusionStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                                 const QWidget *widget) const {
    (void)widget;
    const QPalette::ColorGroup group = groupOf(option);
    const QRect r = option->rect;
    painter->save();
    switch (element) {
    case PE_PanelButtonCommand: {
        const auto *button =
            option->type == QStyleOption::SO_Button ? static_cast<const QStyleOptionButton *>(option) : nullptr;
        const bool down = option->state & (State_Sunken | State_On);
        const bool flat = button && (button->features & QStyleOptionButton::Flat);
        const QColor window = option->palette.color(group, QPalette::Window);
        QColor fill = option->palette.color(group, QPalette::Button);
        if (down)
            fill = fill.darker(112);
        else if ((option->state & State_MouseOver) && (option->state & State_Enabled))
            fill = fill.lighter(106);
        QColor border = outlineOf(option);
        if (option->state & State_HasFocus)
            border = option->palette.color(group, QPalette::Highlight);
        if (flat && !down && !(option->state & State_MouseOver)) {
            if (option->state & State_HasFocus) {
                painter->setPen(border);
                painter->setBrush(Qt::NoBrush);
                painter->drawRect(r.adjusted(0, 0, -1, -1));
            }
        } else {
            drawBox(painter, r, fill, border, window);
            if (!down) {
                painter->setPen(option->palette.color(group, QPalette::Light));
                painter->drawLine(r.left() + 2, r.top() + 1, r.right() - 2, r.top() + 1);
            }
        }
        break;
    }
    case PE_FrameFocusRect:
        painter->setPen(option->palette.color(group, QPalette::Highlight));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(r.adjusted(0, 0, -1, -1));
        break;
    case PE_IndicatorCheckBox: {
        const bool down = option->state & State_Sunken;
        QColor fill = option->palette.color(group, QPalette::Base);
        if (down)
            fill = fill.darker(110);
        QColor border = outlineOf(option);
        if (option->state & State_HasFocus)
            border = option->palette.color(group, QPalette::Highlight);
        drawBox(painter, r, fill, border, option->palette.color(group, QPalette::Window));
        if (option->state & State_On) {
            painter->setPen(QPen(QBrush(option->palette.color(group, QPalette::Text)), 2));
            painter->drawLine(r.left() + 3, r.top() + r.height() / 2, r.left() + 5, r.top() + r.height() / 2 + 2);
            painter->drawLine(r.left() + 5, r.top() + r.height() / 2 + 2, r.right() - 3, r.top() + 3);
        }
        break;
    }
    case PE_PanelLineEdit:
        painter->fillRect(r, option->palette.color(group, QPalette::Base));
        break;
    case PE_FrameLineEdit:
    case PE_Frame: {
        QColor border = outlineOf(option);
        if ((option->state & State_HasFocus) && element == PE_FrameLineEdit)
            border = option->palette.color(group, QPalette::Highlight);
        painter->setPen(border);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(r.adjusted(0, 0, -1, -1));
        break;
    }
    }
    painter->restore();
}

void QFusionStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                               const QWidget *widget) const {
    const QPalette::ColorGroup group = groupOf(option);
    switch (element) {
    case CE_PushButton: {
        drawControl(CE_PushButtonBevel, option, painter, widget);
        QStyleOptionButton inner = *static_cast<const QStyleOptionButton *>(option);
        inner.rect = subElementRect(SE_PushButtonContents, option, widget);
        drawControl(CE_PushButtonLabel, &inner, painter, widget);
        break;
    }
    case CE_PushButtonBevel:
        drawPrimitive(PE_PanelButtonCommand, option, painter, widget);
        break;
    case CE_PushButtonLabel: {
        const auto *button = static_cast<const QStyleOptionButton *>(option);
        painter->save();
        painter->setPen(option->palette.color(group, QPalette::ButtonText));
        QRect textRect = button->rect;
        if (option->state & State_Sunken)
            textRect.translate(0, 1);
        painter->drawText(textRect, Qt::AlignCenter | Qt::TextSingleLine, button->text);
        painter->restore();
        break;
    }
    case CE_CheckBox: {
        QStyleOptionButton indicator = *static_cast<const QStyleOptionButton *>(option);
        indicator.rect = subElementRect(SE_CheckBoxIndicator, option, widget);
        drawPrimitive(PE_IndicatorCheckBox, &indicator, painter, widget);
        QStyleOptionButton label = *static_cast<const QStyleOptionButton *>(option);
        label.rect = subElementRect(SE_CheckBoxContents, option, widget);
        drawControl(CE_CheckBoxLabel, &label, painter, widget);
        break;
    }
    case CE_CheckBoxLabel: {
        const auto *button = static_cast<const QStyleOptionButton *>(option);
        painter->save();
        painter->setPen(option->palette.color(group, QPalette::WindowText));
        painter->drawText(button->rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, button->text);
        painter->restore();
        if (option->state & State_HasFocus) {
            QStyleOption focus;
            focus.state = option->state;
            focus.palette = option->palette;
            const int w = option->fontMetrics.horizontalAdvance(button->text);
            focus.rect =
                QRect(button->rect.left() - 2, button->rect.center().y() - option->fontMetrics.height() / 2 - 1, w + 4,
                      option->fontMetrics.height() + 2);
            painter->save();
            painter->setPen(QColor(option->palette.color(group, QPalette::Highlight)));
            painter->drawLine(focus.rect.left(), focus.rect.bottom(), focus.rect.right(), focus.rect.bottom());
            painter->restore();
        }
        break;
    }
    }
}

QRect QFusionStyle::subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const {
    const QRect r = option->rect;
    switch (element) {
    case SE_CheckBoxIndicator: {
        const int size = pixelMetric(PM_IndicatorWidth, option, widget);
        return QRect(r.left(), r.top() + (r.height() - size) / 2, size, size);
    }
    case SE_CheckBoxContents: {
        const int offset =
            pixelMetric(PM_IndicatorWidth, option, widget) + pixelMetric(PM_CheckBoxLabelSpacing, option, widget);
        return QRect(r.left() + offset, r.top(), std::max(0, r.width() - offset), r.height());
    }
    case SE_PushButtonContents: {
        const int m = pixelMetric(PM_ButtonMargin, option, widget);
        return r.adjusted(m, m / 2, -m, -m / 2);
    }
    }
    return r;
}

int QFusionStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const {
    (void)option;
    switch (metric) {
    case PM_ButtonMargin:
        return 6;
    case PM_DefaultFrameWidth:
        return 1;
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:
        return 14;
    case PM_CheckBoxLabelSpacing:
        return 6;
    case PM_LayoutLeftMargin:
    case PM_LayoutTopMargin:
    case PM_LayoutRightMargin:
    case PM_LayoutBottomMargin:
        // As in Qt, a window has a wider margin than a widget inside it.
        return widget && widget->isWindow() ? 11 : 9;
    case PM_LayoutHorizontalSpacing:
    case PM_LayoutVerticalSpacing:
        return 6;
    }
    return 0;
}

QSize QFusionStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contentsSize,
                                     const QWidget *widget) const {
    switch (type) {
    case CT_PushButton: {
        const int m = pixelMetric(PM_ButtonMargin, option, widget);
        const int frame = 2 * pixelMetric(PM_DefaultFrameWidth, option, widget);
        const auto *button = option && option->type == QStyleOption::SO_Button
                                 ? static_cast<const QStyleOptionButton *>(option)
                                 : nullptr;
        const bool hasText = button && !button->text.isEmpty();
        const int width = contentsSize.width() + m + frame + 6;
        return QSize(hasText ? std::max(80, width) : width, contentsSize.height() + m + frame);
    }
    case CT_CheckBox: {
        const int indicator = pixelMetric(PM_IndicatorWidth, option, widget);
        const int spacing = pixelMetric(PM_CheckBoxLabelSpacing, option, widget);
        // The label has 4 pixels of margin besides the spacing; the height is one more than the contents need.
        return QSize(indicator + (contentsSize.width() > 0 ? 4 + spacing + contentsSize.width() : 0),
                     std::max(indicator, contentsSize.height() + 4) + 1);
    }
    case CT_LineEdit: {
        const int frame = pixelMetric(PM_DefaultFrameWidth, option, widget);
        return QSize(contentsSize.width() + 2 * (frame + 3), contentsSize.height() + 2 * (frame + 2));
    }
    }
    return contentsSize;
}

QPalette QFusionStyle::standardPalette() const {
    const QStyleHints *hints = QGuiApplication::styleHints();
    QPalette palette;
    if (!hints || hints->colorScheme() != Qt::ColorScheme::Dark)
        return palette;
    // The neutral grays that Qt's Fusion style uses for its dark scheme.
    palette.setColor(QPalette::Window, QColor(0x2a, 0x2a, 0x2a));
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Base, QColor(0x1e, 0x1e, 0x1e));
    palette.setColor(QPalette::AlternateBase, QColor(0x2a, 0x2a, 0x2a));
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::Button, QColor(0x3b, 0x3b, 0x3b));
    palette.setColor(QPalette::ButtonText, Qt::white);
    // The shades around the button color are made from it, as QPalette(button, window) does.
    const QColor button(0x3b, 0x3b, 0x3b);
    palette.setColor(QPalette::Light, button.lighter(150));
    palette.setColor(QPalette::Midlight, button.lighter(115));
    palette.setColor(QPalette::Mid, button.darker(125));
    palette.setColor(QPalette::Dark, button.darker(150));
    palette.setColor(QPalette::Highlight, QColor(0x3d, 0x8e, 0xc9));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::ButtonText, QPalette::Text})
        palette.setColor(QPalette::Disabled, role, QColor(0x7f, 0x7f, 0x7f));
    return palette;
}
