// MIT License - see LICENSE
#include "qfusionstyle_p.h"

#include <QtGui/qguiapplication.h>
#include <QtGui/qpainter.h>
#include <QtGui/qstylehints.h>
#include <QtWidgets/qgroupbox.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qwidget.h>

#include <algorithm>
#include <cmath>

namespace {

QPalette::ColorGroup groupOf(const QStyleOption *option) {
    return (option->state & QStyle::State_Enabled) ? QPalette::Active : QPalette::Disabled;
}

// Whether a color is dark, as the window of a dark color scheme is.
bool isDark(const QColor &color) { return color.red() + color.green() + color.blue() < 3 * 128; }

// The color of the outlines: darker than the window, or lighter if the window is dark.
QColor outlineOf(const QStyleOption *option) {
    const QColor window = option->palette.color(groupOf(option), QPalette::Window);
    return isDark(window) ? window.lighter(185) : window.darker(150);
}

// The color of the pages of a tab widget and of the current tab: a little lighter than the window.
QColor tabFrameColor(const QStyleOption *option) {
    const QColor window = option->palette.color(groupOf(option), QPalette::Window);
    return window.lighter(isDark(window) ? 130 : 103);
}

// The frame of a menu: darker than the outlines of the controls.
QColor menuFrameOf(const QStyleOption *option) {
    const QColor window = option->palette.color(groupOf(option), QPalette::Window);
    return isDark(window) ? window.lighter(185) : window.darker(160);
}

QColor mix(const QColor &a, const QColor &b, int percentOfA) {
    auto m = [percentOfA](int x, int y) { return (x * percentOfA + y * (100 - percentOfA)) / 100; };
    return QColor(m(a.red(), b.red()), m(a.green(), b.green()), m(a.blue(), b.blue()));
}

// Draws `text` with the character after the first '&' underlined; `flags` are the alignment flags.
void drawMnemonicText(QPainter *p, const QRect &rect, int flags, const QString &text) {
    QString plain;
    qsizetype mnemonic = -1;
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text.at(i) == '&' && i + 1 < text.size()) {
            ++i;
            if (text.at(i) != '&' && mnemonic < 0)
                mnemonic = plain.size();
        }
        plain += text.at(i);
    }
    p->drawText(rect, flags | Qt::TextSingleLine, plain);
    if (mnemonic < 0)
        return;
    const QFontMetrics fm(p->font());
    const int total = fm.horizontalAdvance(plain);
    int x = rect.left();
    if (flags & Qt::AlignRight)
        x = rect.right() + 1 - total;
    else if (flags & Qt::AlignHCenter)
        x = rect.left() + (rect.width() - total) / 2;
    const int before = fm.horizontalAdvance(plain.left(mnemonic));
    const int width = fm.horizontalAdvance(plain.at(mnemonic));
    const int baseline = rect.top() + (rect.height() - fm.height()) / 2 + fm.ascent();
    p->drawLine(x + before, baseline + 2, x + before + width - 1, baseline + 2);
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

// A triangle that is wide at the top (pointing down) or at the bottom (pointing up), 7 by 4 pixels around `center`.
void drawArrow(QPainter *p, const QPoint &center, bool down, const QColor &color) {
    p->setPen(color);
    for (int i = 0; i < 4; ++i) {
        const int half = down ? 3 - i : i;
        p->drawLine(center.x() - half, center.y() - 2 + i, center.x() + half, center.y() - 2 + i);
    }
}

// The rectangle of the title of a group box in `r`, the check box included.
QRect groupTitleRect(const QStyleOptionGroupBox *box) {
    const int indicator = box->checkable ? 14 + 4 : 0;
    const int width = indicator + box->fontMetrics.horizontalAdvance(box->text) + 6;
    return QRect(box->rect.left(), box->rect.top(), std::min(width, std::max(0, box->rect.width())),
                 box->fontMetrics.height());
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
    case PE_IndicatorRadioButton: {
        QColor fill = option->palette.color(group, QPalette::Base);
        if (option->state & State_Sunken)
            fill = fill.darker(110);
        QColor border = outlineOf(option);
        if (option->state & State_HasFocus)
            border = option->palette.color(group, QPalette::Highlight);
        painter->setPen(border);
        painter->setBrush(fill);
        painter->drawEllipse(r.adjusted(0, 0, -1, -1));
        if (option->state & State_On) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(option->palette.color(group, QPalette::Text));
            painter->drawEllipse(r.adjusted(4, 4, -4, -4));
        }
        break;
    }
    case PE_IndicatorArrowUp:
    case PE_IndicatorArrowDown:
        drawArrow(painter, r.center(), element == PE_IndicatorArrowDown,
                  option->palette.color(group, QPalette::ButtonText));
        break;
    case PE_IndicatorArrowLeft:
    case PE_IndicatorArrowRight: {
        // The same triangle turned: 4 pixels wide and 7 high.
        painter->setPen(option->palette.color(group, QPalette::ButtonText));
        const QPoint c = r.center();
        for (int i = 0; i < 4; ++i) {
            const int half = element == PE_IndicatorArrowLeft ? i : 3 - i;
            const int x = c.x() - 2 + i;
            painter->drawLine(x, c.y() - half, x, c.y() + half);
        }
        break;
    }
    case PE_PanelMenu:
        painter->fillRect(r, option->palette.color(group, QPalette::Base));
        painter->setPen(menuFrameOf(option));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(r.adjusted(0, 0, -1, -1));
        break;
    case PE_PanelStatusBar:
        painter->fillRect(r, option->palette.color(group, QPalette::Window));
        painter->setPen(outlineOf(option));
        painter->drawLine(r.left(), r.top(), r.right(), r.top());
        break;
    case PE_FrameGroupBox:
        painter->setPen(mix(outlineOf(option), option->palette.color(group, QPalette::Window), 45));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(r.adjusted(0, 0, -1, -1));
        break;
    case PE_PanelTipLabel:
        painter->fillRect(r, option->palette.color(group, QPalette::ToolTipBase));
        painter->setPen(option->palette.color(group, QPalette::ToolTipText).lighter(300));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(r.adjusted(0, 0, -1, -1));
        break;
    case PE_FrameTabWidget:
        painter->fillRect(r, tabFrameColor(option));
        painter->setPen(outlineOf(option));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(r.adjusted(0, 0, -1, -1));
        break;
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
    case CE_RadioButton: {
        QStyleOptionButton indicator = *static_cast<const QStyleOptionButton *>(option);
        indicator.rect = subElementRect(SE_RadioButtonIndicator, option, widget);
        drawPrimitive(PE_IndicatorRadioButton, &indicator, painter, widget);
        QStyleOptionButton label = *static_cast<const QStyleOptionButton *>(option);
        label.rect = subElementRect(SE_RadioButtonContents, option, widget);
        drawControl(CE_RadioButtonLabel, &label, painter, widget);
        break;
    }
    case CE_RadioButtonLabel:
        drawControl(CE_CheckBoxLabel, option, painter, widget);
        break;
    case CE_Slider: {
        const auto *slider = static_cast<const QStyleOptionSlider *>(option);
        const QRect groove = subElementRect(SE_SliderGroove, option, widget);
        const QRect handle = subElementRect(SE_SliderHandle, option, widget);
        const bool horizontal = slider->orientation == Qt::Horizontal;
        const QColor window = option->palette.color(group, QPalette::Window);
        const QColor highlight = option->palette.color(group, QPalette::Highlight);
        painter->save();
        drawBox(painter, groove, option->palette.color(group, QPalette::Base).darker(106), outlineOf(option), window);
        // The part from the end where the slider starts to the handle has the highlight color.
        QRect filled = groove.adjusted(1, 1, -1, -1);
        if (horizontal)
            filled.setRight(handle.center().x());
        else
            filled.setTop(handle.center().y());
        if (option->state & State_Enabled)
            painter->fillRect(filled, highlight);
        QColor fill = option->palette.color(group, QPalette::Button);
        if (option->state & State_Sunken)
            fill = fill.darker(112);
        else if (option->state & State_MouseOver)
            fill = fill.lighter(106);
        drawBox(painter, handle, fill, (option->state & State_HasFocus) ? highlight : outlineOf(option), window);
        if (!(option->state & State_Sunken)) {
            painter->setPen(option->palette.color(group, QPalette::Light));
            painter->drawLine(handle.left() + 2, handle.top() + 1, handle.right() - 2, handle.top() + 1);
        }
        painter->restore();
        break;
    }
    case CE_ProgressBar: {
        const auto *bar = static_cast<const QStyleOptionProgressBar *>(option);
        const QRect r = option->rect;
        const QColor window = option->palette.color(group, QPalette::Window);
        painter->save();
        drawBox(painter, r, option->palette.color(group, QPalette::Base), outlineOf(option), window);
        const QRect inner = subElementRect(SE_ProgressBarContents, option, widget);
        const qint64 span = static_cast<qint64>(bar->maximum) - bar->minimum;
        QRect filled;
        if (span > 0 && bar->progress > bar->minimum) {
            const qint64 done = std::min<qint64>(bar->progress, bar->maximum) - bar->minimum;
            filled = QRect(inner.left(), inner.top(), static_cast<int>(inner.width() * done / span), inner.height());
            painter->fillRect(filled, option->palette.color(group, QPalette::Highlight));
        }
        if (bar->textVisible && !bar->text.isEmpty()) {
            painter->setClipRect(inner);
            painter->setPen(option->palette.color(group, QPalette::Text));
            painter->drawText(inner, Qt::AlignCenter | Qt::TextSingleLine, bar->text);
            if (!filled.isEmpty()) {
                painter->setClipRect(filled);
                painter->setPen(option->palette.color(group, QPalette::HighlightedText));
                painter->drawText(inner, Qt::AlignCenter | Qt::TextSingleLine, bar->text);
            }
        }
        painter->restore();
        break;
    }
    case CE_Splitter: {
        const QColor window = option->palette.color(group, QPalette::Window);
        painter->save();
        painter->fillRect(option->rect, window);
        // Three dots in the middle of the handle, along it
        const bool horizontal = option->state & State_Horizontal;
        const QColor dot = mix(outlineOf(option), window, 70);
        const QPoint c = option->rect.center();
        for (int i = -1; i <= 1; ++i) {
            const QRect d =
                horizontal ? QRect(c.x() - 1, c.y() + i * 4 - 1, 2, 2) : QRect(c.x() + i * 4 - 1, c.y() - 1, 2, 2);
            painter->fillRect(d, dot);
        }
        painter->restore();
        break;
    }
    case CE_MenuItem: {
        const auto *item = static_cast<const QStyleOptionMenuItem *>(option);
        const QRect r = item->rect;
        const bool enabled = item->state & State_Enabled;
        const bool selected = enabled && (item->state & State_Selected);
        const QColor base = item->palette.color(group, QPalette::Base);
        painter->save();
        painter->setFont(item->font);
        if (item->menuItemType == QStyleOptionMenuItem::Separator) {
            painter->setPen(mix(menuFrameOf(item), base, 57));
            painter->drawLine(r.left() + 5, r.top(), r.right() - 5, r.top());
            painter->restore();
            break;
        }
        if (selected)
            painter->fillRect(r, item->palette.color(group, QPalette::Highlight));
        const QColor text = item->palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text);
        if (item->checkType != QStyleOptionMenuItem::NotCheckable) {
            QStyleOption box;
            box.state = (item->state & State_Enabled) | (item->checked ? State_On : State_Off);
            box.palette = item->palette;
            box.rect = QRect(r.left() + 8, r.top() + (r.height() - 14) / 2, 14, 14);
            drawPrimitive(PE_IndicatorCheckBox, &box, painter, widget);
        }
        painter->setPen(text);
        const qsizetype tab = item->text.indexOf('\t');
        const QString label = tab < 0 ? item->text : item->text.left(tab);
        drawMnemonicText(painter, QRect(r.left() + 28, r.top(), r.width() - 28, r.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, label);
        if (tab >= 0)
            painter->drawText(QRect(r.left(), r.top(), r.width() - 15, r.height()),
                              Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, item->text.mid(tab + 1));
        if (item->menuItemType == QStyleOptionMenuItem::SubMenu) {
            painter->setPen(mix(text, selected ? item->palette.color(group, QPalette::Highlight) : base, 63));
            const int cy = r.top() + r.height() / 2;
            for (int i = 0; i < 4; ++i)
                painter->drawLine(r.right() - 9 + i, cy - (3 - i), r.right() - 9 + i, cy + (3 - i));
        }
        painter->restore();
        break;
    }
    case CE_MenuBarEmptyArea: {
        const QColor window = option->palette.color(group, QPalette::Window);
        painter->fillRect(option->rect, window);
        painter->setPen(isDark(window) ? window.lighter(112) : window.darker(112));
        painter->drawLine(option->rect.left(), option->rect.bottom(), option->rect.right(), option->rect.bottom());
        break;
    }
    case CE_MenuBarItem: {
        const auto *item = static_cast<const QStyleOptionMenuItem *>(option);
        const bool selected = (item->state & State_Enabled) && (item->state & State_Selected);
        painter->save();
        painter->setFont(item->font);
        if (selected) {
            const QColor highlight = item->palette.color(group, QPalette::Highlight);
            painter->setPen(highlight.darker(125));
            painter->setBrush(highlight);
            painter->drawRect(item->rect.adjusted(0, 0, -1, -1));
        }
        painter->setPen(item->palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text));
        drawMnemonicText(painter, item->rect, Qt::AlignCenter, item->text);
        painter->restore();
        break;
    }
    case CE_Dial: {
        const auto *dial = static_cast<const QStyleOptionSlider *>(option);
        const QRect box = option->rect;
        const int size = std::min(box.width(), box.height());
        const QPoint center = box.center();
        const QColor window = option->palette.color(group, QPalette::Window);
        const QColor highlight = option->palette.color(group, QPalette::Highlight);
        painter->save();
        const qreal pi = 3.14159265358979323846;
        const qint64 range = static_cast<qint64>(dial->maximum) - dial->minimum;
        const qreal fraction =
            range > 0
                ? static_cast<qreal>(std::clamp(dial->sliderPosition, dial->minimum, dial->maximum) - dial->minimum) /
                      static_cast<qreal>(range)
                : 0;
        // A dial that does not wrap spans 270 degrees from the bottom left to the bottom right, one that wraps the full circle
        // from the bottom.
        const qreal span = dial->wrapping ? 360.0 : 270.0, start = dial->wrapping ? 270.0 : 225.0;
        auto point = [&](qreal degrees, qreal radius) {
            const qreal rad = degrees * pi / 180.0;
            return QPoint(center.x() + static_cast<int>(std::lround(radius * std::cos(rad))),
                          center.y() - static_cast<int>(std::lround(radius * std::sin(rad))));
        };
        const int outer = std::max(2, size / 2 - 3 - (dial->tickMarks ? 4 : 0));
        if (dial->tickMarks) {
            painter->setPen(mix(outlineOf(option), window, 80));
            const int notches = dial->wrapping ? 12 : 10;
            for (int i = 0; i <= notches; ++i) {
                if (dial->wrapping && i == notches)
                    break;
                const qreal a = start - span * i / notches;
                const int rim = size / 2 - 1;
                painter->drawLine(point(a, rim), point(a, rim - (i % 5 == 0 ? 4 : 2)));
            }
        }
        painter->setPen(outlineOf(option));
        painter->setBrush(option->palette.color(group, QPalette::Button));
        painter->drawEllipse(QRect(center.x() - outer, center.y() - outer, 2 * outer + 1, 2 * outer + 1));
        // The handle: a dot on the face, in the highlight color if the dial is enabled and has the focus
        const qreal angle = start - span * fraction;
        const int hr = std::max(2, outer / 6);
        const QPoint h = point(angle, outer * 0.72);
        painter->setPen((option->state & State_HasFocus) ? highlight : outlineOf(option));
        painter->setBrush(
            (option->state & State_Sunken)
                ? highlight.darker(115)
                : (option->state & State_Enabled ? highlight : option->palette.color(group, QPalette::Mid)));
        painter->drawEllipse(QRect(h.x() - hr, h.y() - hr, 2 * hr + 1, 2 * hr + 1));
        painter->restore();
        break;
    }
    case CE_ScrollBar: {
        const auto *bar = static_cast<const QStyleOptionSlider *>(option);
        const QRect r = option->rect;
        const bool horizontal = bar->orientation == Qt::Horizontal;
        const QColor window = option->palette.color(group, QPalette::Window);
        painter->save();
        painter->fillRect(r, isDark(window) ? window.lighter(112) : window.darker(104));
        painter->setPen(outlineOf(option));
        // The line between the bar and the contents it scrolls
        if (horizontal)
            painter->drawLine(r.left(), r.top(), r.right(), r.top());
        else
            painter->drawLine(r.left(), r.top(), r.left(), r.bottom());
        if (bar->maximum > bar->minimum) {
            const QRect handle = subElementRect(SE_ScrollBarSlider, option, widget);
            QColor fill = option->palette.color(group, QPalette::Button);
            if (option->state & State_Sunken)
                fill = fill.darker(112);
            else if (option->state & State_MouseOver)
                fill = fill.lighter(106);
            drawBox(painter, handle, fill, outlineOf(option), window);
            if (!(option->state & State_Sunken)) {
                painter->setPen(option->palette.color(group, QPalette::Light));
                if (horizontal)
                    painter->drawLine(handle.left() + 2, handle.top() + 1, handle.right() - 2, handle.top() + 1);
                else
                    painter->drawLine(handle.left() + 1, handle.top() + 2, handle.left() + 1, handle.bottom() - 2);
            }
        }
        painter->restore();
        break;
    }
    case CE_Header: {
        const auto *header = static_cast<const QStyleOptionHeader *>(option);
        const QRect r = option->rect;
        const bool horizontal = header->orientation == Qt::Horizontal;
        painter->save();
        QColor fill = option->palette.color(group, QPalette::Button);
        if (option->state & State_Sunken)
            fill = fill.darker(108);
        else if (option->state & State_MouseOver)
            fill = fill.lighter(104);
        painter->fillRect(r, fill);
        painter->setPen(outlineOf(option));
        if (horizontal) {
            painter->drawLine(r.left(), r.bottom(), r.right(), r.bottom());
            if (!header->lastSection)
                painter->drawLine(r.right(), r.top() + 3, r.right(), r.bottom() - 3);
        } else {
            painter->drawLine(r.right(), r.top(), r.right(), r.bottom());
            if (!header->lastSection)
                painter->drawLine(r.left() + 3, r.bottom(), r.right() - 3, r.bottom());
        }
        const int margin = pixelMetric(PM_HeaderMargin, option, widget);
        const QRect text = r.adjusted(margin, 0, -margin, -1);
        painter->setPen(option->palette.color(group, QPalette::ButtonText));
        painter->drawText(text, int(header->textAlignment) | Qt::AlignVCenter | Qt::TextSingleLine,
                          header->fontMetrics.elidedText(header->text, Qt::ElideRight, text.width()));
        painter->restore();
        break;
    }
    case CE_TabBarTab: {
        const auto *tab = static_cast<const QStyleOptionTab *>(option);
        const bool selected = option->state & State_Selected;
        const QColor window = option->palette.color(group, QPalette::Window);
        QRect r = option->rect;
        painter->save();
        if (selected) {
            const QColor border = outlineOf(option);
            painter->fillRect(r.adjusted(1, 1, -1, 0), tabFrameColor(option));
            painter->setPen(border);
            painter->drawLine(r.left() + 1, r.top(), r.right() - 1, r.top());
            painter->drawLine(r.left(), r.top() + 1, r.left(), r.bottom());
            painter->drawLine(r.right(), r.top() + 1, r.right(), r.bottom());
            painter->setPen(mix(border, window, 35));
            painter->drawPoint(r.left(), r.top());
            painter->drawPoint(r.right(), r.top());
            if (option->state & State_HasFocus)
                painter->fillRect(r.left() + 1, r.top() + 1, r.width() - 2, 2,
                                  option->palette.color(group, QPalette::Highlight));
        } else {
            r.setTop(r.top() + 3);
            QColor fill = window.darker(106);
            if ((option->state & State_MouseOver) && (option->state & State_Enabled))
                fill = window.darker(102);
            drawBox(painter, r, fill, outlineOf(option), window);
        }
        painter->setPen(option->palette.color(group, QPalette::WindowText));
        painter->drawText(r, Qt::AlignCenter | Qt::TextSingleLine, tab->text);
        painter->restore();
        break;
    }
    case CE_ComboBox: {
        const auto *combo = static_cast<const QStyleOptionComboBox *>(option);
        QStyleOption bevel = *option;
        if (option->state & State_On)
            bevel.state |= State_Sunken;
        drawPrimitive(PE_PanelButtonCommand, &bevel, painter, widget);
        const QRect arrow = subElementRect(SE_ComboBoxArrow, option, widget);
        QStyleOption arrowOption = *option;
        arrowOption.rect = arrow;
        drawPrimitive(PE_IndicatorArrowDown, &arrowOption, painter, widget);
        painter->save();
        painter->setClipRect(option->rect.adjusted(1, 1, -arrow.width(), -1));
        painter->setPen(option->palette.color(group, QPalette::ButtonText));
        painter->drawText(option->rect.adjusted(8, 0, -arrow.width(), 0),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, combo->currentText);
        painter->restore();
        break;
    }
    case CE_SpinBox: {
        const auto *spin = static_cast<const QStyleOptionSpinBox *>(option);
        QStyleOption frame = *option;
        drawPrimitive(PE_PanelLineEdit, &frame, painter, widget);
        drawPrimitive(PE_FrameLineEdit, &frame, painter, widget);
        const QColor window = option->palette.color(group, QPalette::Window);
        const struct {
            QRect rect;
            bool up;
            bool enabled;
            bool pressed;
        } buttons[] = {{subElementRect(SE_SpinBoxUp, option, widget), true, spin->upEnabled, spin->upPressed},
                       {subElementRect(SE_SpinBoxDown, option, widget), false, spin->downEnabled, spin->downPressed}};
        for (const auto &b : buttons) {
            QColor fill = option->palette.color(group, QPalette::Button);
            if (b.pressed)
                fill = fill.darker(112);
            painter->fillRect(b.rect, fill);
            painter->setPen(mix(outlineOf(option), window, 60));
            painter->drawLine(b.rect.left(), b.rect.top(), b.rect.left(), b.rect.bottom());
            QStyleOption arrow = *option;
            arrow.rect = b.rect;
            if (!b.enabled)
                arrow.state &= ~State(State_Enabled);
            drawPrimitive(b.up ? PE_IndicatorArrowUp : PE_IndicatorArrowDown, &arrow, painter, widget);
        }
        painter->setPen(mix(outlineOf(option), window, 60));
        painter->drawLine(buttons[1].rect.left(), buttons[1].rect.top(), buttons[1].rect.right(),
                          buttons[1].rect.top());
        break;
    }
    case CE_GroupBox: {
        const auto *box = static_cast<const QStyleOptionGroupBox *>(option);
        const QRect title = subElementRect(SE_GroupBoxTitle, option, widget);
        QStyleOption frame = *option;
        frame.rect = QRect(option->rect.left(), title.bottom() + 3, option->rect.width(),
                           option->rect.bottom() - title.bottom() - 2);
        drawPrimitive(PE_FrameGroupBox, &frame, painter, widget);
        painter->save();
        QRect text = title;
        if (box->checkable) {
            QStyleOptionButton indicator;
            indicator.state = option->state;
            indicator.palette = option->palette;
            indicator.rect = subElementRect(SE_GroupBoxIndicator, option, widget);
            drawPrimitive(PE_IndicatorCheckBox, &indicator, painter, widget);
            text.setLeft(title.left() + 14 + 4);
        }
        painter->setPen(option->palette.color(group, QPalette::WindowText));
        painter->drawText(text, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, box->text);
        painter->restore();
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
    case SE_RadioButtonIndicator: {
        const int size = pixelMetric(PM_ExclusiveIndicatorWidth, option, widget);
        return QRect(r.left(), r.top() + (r.height() - size) / 2, size, size);
    }
    case SE_RadioButtonContents: {
        const int offset = pixelMetric(PM_ExclusiveIndicatorWidth, option, widget) +
                           pixelMetric(PM_CheckBoxLabelSpacing, option, widget);
        return QRect(r.left() + offset, r.top(), std::max(0, r.width() - offset), r.height());
    }
    case SE_LineEditContents: {
        const int frame = option->type == QStyleOption::SO_Frame
                              ? static_cast<const QStyleOptionFrame *>(option)->lineWidth
                              : pixelMetric(PM_DefaultFrameWidth, option, widget);
        return r.adjusted(frame + 3, frame + 2, -(frame + 3), -(frame + 2));
    }
    case SE_SliderGroove: {
        const auto *slider = static_cast<const QStyleOptionSlider *>(option);
        const int thickness = 5;
        if (slider->orientation == Qt::Horizontal)
            return QRect(r.left(), r.center().y() - thickness / 2, r.width(), thickness);
        return QRect(r.center().x() - thickness / 2, r.top(), thickness, r.height());
    }
    case SE_SliderHandle: {
        const auto *slider = static_cast<const QStyleOptionSlider *>(option);
        const int length = pixelMetric(PM_SliderLength, option, widget);
        const int thickness = pixelMetric(PM_SliderThickness, option, widget);
        const qint64 range = static_cast<qint64>(slider->maximum) - slider->minimum;
        const qint64 done = std::clamp<qint64>(static_cast<qint64>(slider->sliderPosition) - slider->minimum, 0, range);
        if (slider->orientation == Qt::Horizontal) {
            const int span = std::max(0, r.width() - length);
            const int x = range > 0 ? static_cast<int>(done * span / range) : 0;
            return QRect(r.left() + x, r.center().y() - thickness / 2, length, thickness);
        }
        // The smallest value of a vertical slider is at the bottom.
        const int span = std::max(0, r.height() - length);
        const int y = range > 0 ? static_cast<int>(done * span / range) : 0;
        return QRect(r.center().x() - thickness / 2, r.bottom() + 1 - length - y, thickness, length);
    }
    case SE_ScrollBarSlider: {
        const auto *bar = static_cast<const QStyleOptionSlider *>(option);
        const bool horizontal = bar->orientation == Qt::Horizontal;
        const qint64 range = static_cast<qint64>(bar->maximum) - bar->minimum;
        // The groove is the bar without the line that separates it from the contents.
        const QRect groove = horizontal ? r.adjusted(0, 1, 0, 0) : r.adjusted(1, 0, 0, 0);
        const int length = horizontal ? groove.width() : groove.height();
        if (range <= 0)
            return groove;
        const qint64 total = range + std::max(1, bar->pageStep);
        const int handle = std::clamp(static_cast<int>(length * std::max(1, bar->pageStep) / total),
                                      std::min(length, pixelMetric(PM_ScrollBarSliderMin, option, widget)), length);
        const qint64 done = std::clamp<qint64>(static_cast<qint64>(bar->sliderPosition) - bar->minimum, 0, range);
        const int offset = static_cast<int>((length - handle) * done / range);
        if (horizontal)
            return QRect(groove.left() + offset, groove.top(), handle, groove.height()).adjusted(0, 1, 0, -1);
        return QRect(groove.left(), groove.top() + offset, groove.width(), handle).adjusted(1, 0, -1, 0);
    }
    case SE_ProgressBarContents:
        return r.adjusted(1, 1, -1, -1);
    case SE_ComboBoxArrow: {
        const int width = pixelMetric(PM_ComboBoxArrowWidth, option, widget);
        return QRect(r.right() - width + 1, r.top(), width, r.height());
    }
    case SE_SpinBoxEditField:
        return r.adjusted(1, 1, -(pixelMetric(PM_SpinBoxButtonWidth, option, widget) + 1), -1);
    case SE_SpinBoxUp: {
        const int width = pixelMetric(PM_SpinBoxButtonWidth, option, widget);
        return QRect(r.right() - width, r.top() + 1, width, (r.height() - 2) / 2);
    }
    case SE_SpinBoxDown: {
        const int width = pixelMetric(PM_SpinBoxButtonWidth, option, widget);
        const int upHeight = (r.height() - 2) / 2;
        return QRect(r.right() - width, r.top() + 1 + upHeight, width, r.height() - 2 - upHeight);
    }
    case SE_GroupBoxTitle:
        return groupTitleRect(static_cast<const QStyleOptionGroupBox *>(option));
    case SE_GroupBoxIndicator: {
        const QRect title = groupTitleRect(static_cast<const QStyleOptionGroupBox *>(option));
        return QRect(title.left(), title.top() + (title.height() - 14) / 2, 14, 14);
    }
    case SE_GroupBoxContents: {
        const QRect title = groupTitleRect(static_cast<const QStyleOptionGroupBox *>(option));
        return r.adjusted(1, title.height() + 3, -1, -1);
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
    case PM_LayoutTopMargin:
        // The title of a group box is above the layout inside it.
        if (dynamic_cast<const QGroupBox *>(widget))
            return widget->fontMetrics().height() + 14;
        return widget && widget->isWindow() ? 11 : 9;
    case PM_LayoutLeftMargin:
    case PM_LayoutRightMargin:
    case PM_LayoutBottomMargin:
        // As in Qt, a window has a wider margin than a widget inside it, and a group box has some more for its frame.
        if (dynamic_cast<const QGroupBox *>(widget))
            return 12;
        return widget && widget->isWindow() ? 11 : 9;
    case PM_LayoutHorizontalSpacing:
    case PM_LayoutVerticalSpacing:
        return 6;
    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight:
        return 14;
    case PM_SliderLength:
    case PM_SpinBoxButtonWidth:
        return 16;
    case PM_SliderThickness:
        return 15;
    case PM_ComboBoxArrowWidth:
        return 20;
    case PM_ScrollBarExtent:
        return 14;
    case PM_ScrollBarSliderMin:
        return 26;
    case PM_HeaderMargin:
        return 4;
    case PM_HeaderDefaultSectionSizeHorizontal:
        return 100;
    case PM_HeaderDefaultSectionSizeVertical:
        return 30;
    case PM_MenuBarItemSpacing:
        return 6;
    case PM_SplitterWidth:
        return 4;
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
        return QSize(contentsSize.width() + 2 * (frame + 3), contentsSize.height() + 2 * (frame + 3));
    }
    case CT_RadioButton: {
        const int indicator = pixelMetric(PM_ExclusiveIndicatorWidth, option, widget);
        const int spacing = pixelMetric(PM_CheckBoxLabelSpacing, option, widget);
        return QSize(indicator + (contentsSize.width() > 0 ? 4 + spacing + contentsSize.width() : 0),
                     std::max(indicator, contentsSize.height() + 4) + 1);
    }
    case CT_ProgressBar:
        return QSize(contentsSize.width() + 8, contentsSize.height() + 8);
    case CT_TabBarTab:
        return QSize(contentsSize.width() + 20, contentsSize.height() + 10);
    case CT_ComboBox: {
        const int m = pixelMetric(PM_ButtonMargin, option, widget);
        return QSize(contentsSize.width() + 8 + pixelMetric(PM_ComboBoxArrowWidth, option, widget),
                     contentsSize.height() + m + 2 * pixelMetric(PM_DefaultFrameWidth, option, widget));
    }
    case CT_ToolButton:
        return QSize(contentsSize.width() + 14, contentsSize.height() + 7);
    case CT_MenuItem: {
        const auto *item = static_cast<const QStyleOptionMenuItem *>(option);
        if (item->menuItemType == QStyleOptionMenuItem::Separator)
            return QSize(120, 2);
        int width = contentsSize.width() + 51;
        if (item->text.contains('\t'))
            width += item->tabWidth;
        if (item->menuItemType == QStyleOptionMenuItem::SubMenu)
            width += 12;
        return QSize(std::max(width, 120), contentsSize.height() + 8);
    }
    case CT_MenuBarItem:
        return QSize(contentsSize.width() + 9, contentsSize.height() + 5);
    case CT_SpinBox: {
        const int frame = pixelMetric(PM_DefaultFrameWidth, option, widget);
        return QSize(contentsSize.width() + 2 * (frame + 3) + pixelMetric(PM_SpinBoxButtonWidth, option, widget) + 3,
                     contentsSize.height() + 2 * (frame + 2) + 3);
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
    palette.setColor(QPalette::PlaceholderText, QColor(0x8a, 0x8a, 0x8a));
    for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::ButtonText, QPalette::Text})
        palette.setColor(QPalette::Disabled, role, QColor(0x7f, 0x7f, 0x7f));
    return palette;
}
