// MIT License - see LICENSE
#include <QtWidgets/qtooltip.h>
#include <QtWidgets/qmaterialstyle.h>

#include <optional>

#include "qtooltip_p.h"
#include "qwidget_p.h"

#include <QtCore/qpointer.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpainter.h>
#include <QtGui/qtextdocument.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>

namespace QtPrivate {

int &toolTipDelay() {
    static int delay = 700;
    return delay;
}

} // namespace QtPrivate

namespace {

// The label that shows a tool tip. It is a child of the window of the widget that the tool tip is for, above its other
// children, and the mouse does not see it.
// The font and the palette that setFont() and setPalette() set, if they were set.
std::optional<QFont> &tipFont() {
    static std::optional<QFont> font;
    return font;
}
std::optional<QPalette> &tipPalette() {
    static std::optional<QPalette> palette;
    return palette;
}

// What a rich tooltip of the Material style shows: a subhead, which may be empty, over supporting text.
struct RichTip {
    bool rich = false;
    QString title;
    QString body;
};

// The text of `html` without its markup: its paragraphs and line breaks as line breaks.
QString plainOf(const QString &html) {
    QTextDocument document;
    document.setHtml(html);
    return document.toPlainText().trimmed();
}

// The name of the tag at `at` in `html`, in lower case, if it is an opening tag; the end of the tag goes in `end`.
QString openingTag(const QString &html, qsizetype at, qsizetype *end) {
    if (at >= html.size() || html.at(at) != QLatin1Char('<'))
        return QString();
    qsizetype i = at + 1;
    while (i < html.size() && html.at(i).isLetterOrNumber())
        ++i;
    const qsizetype close = html.indexOf(QLatin1Char('>'), i);
    if (i == at + 1 || close < 0)
        return QString();
    *end = close + 1;
    return html.mid(at + 1, i - at - 1).toLower();
}

// A tooltip is a rich tooltip when its text is rich text, or when the widget it is for asks for one with the property
// materialToolTip = "rich". Rich text that begins with a bold run or a heading has that as its subhead; with the
// property, plain text has its first line as the subhead.
RichTip richTipOf(const QString &text, const QWidget *owner) {
    RichTip tip;
    const bool asked = owner && owner->property("materialToolTip").toString() == QLatin1String("rich");
    if (!Qt::mightBeRichText(text)) {
        if (asked) {
            const qsizetype brk = text.indexOf(QLatin1Char('\n'));
            tip.rich = true;
            tip.title = brk < 0 ? text : text.left(brk);
            tip.body = brk < 0 ? QString() : text.mid(brk + 1);
        }
        return tip;
    }
    tip.rich = true;
    // Past the tags that only wrap the text, to the first that holds some.
    qsizetype at = 0;
    qsizetype end = 0;
    for (;;) {
        while (at < text.size() && text.at(at).isSpace())
            ++at;
        const QString tag = openingTag(text, at, &end);
        if (tag != QLatin1String("qt") && tag != QLatin1String("html") && tag != QLatin1String("body") &&
            tag != QLatin1String("p") && tag != QLatin1String("div"))
            break;
        at = end;
    }
    const QString tag = openingTag(text, at, &end);
    const bool heading = tag.size() == 2 && tag.at(0) == QLatin1Char('h') && tag.at(1) >= QLatin1Char('1') &&
                         tag.at(1) <= QLatin1Char('6');
    if (tag == QLatin1String("b") || tag == QLatin1String("strong") || heading) {
        const qsizetype close = text.indexOf(QLatin1String("</") + tag, end, Qt::CaseInsensitive);
        const qsizetype after = close < 0 ? -1 : text.indexOf(QLatin1Char('>'), close);
        if (after >= 0) {
            tip.title = plainOf(text.mid(end, close - end));
            tip.body = plainOf(text.mid(after + 1));
            return tip;
        }
    }
    tip.body = plainOf(text);
    if (asked) {
        const qsizetype brk = tip.body.indexOf(QLatin1Char('\n'));
        tip.title = brk < 0 ? tip.body : tip.body.left(brk);
        tip.body = brk < 0 ? QString() : tip.body.mid(brk + 1);
    }
    return tip;
}

class TipLabel : public QWidget {
public:
    explicit TipLabel(QWidget *parent) : QWidget(parent) {
        QtPrivate::WidgetAccess::data(this).mouseTransparent = true;
        hide();
    }

    QString text() const { return m_text; }
    QRect rectOfOwner() const { return m_rect; }
    QWidget *owner() const { return m_owner; }

    void open(const QString &text, const QPoint &topLeft, QWidget *owner, const QRect &rect) {
        m_text = text;
        m_owner = owner;
        m_rect = rect;
        m_tip = isMaterial() ? richTipOf(text, owner) : RichTip();
        // The style draws the container of a rich tooltip, and asks the label which it is.
        setProperty("_nt_materialRichToolTip", m_tip.rich);
        QFont shown = QToolTip::font();
        // Body small for a plain tooltip, body medium for a rich one, unless the program chose a font for tool tips.
        if (isMaterial() && !tipFont())
            shown.setPixelSize(m_tip.rich ? 14 : 12);
        setFont(shown);
        setPalette(QToolTip::palette());
        QWidget *top = parentWidget();
        const QSize size = m_tip.rich ? richSize(top->width()) : textSize();
        // Keep the tool tip inside the window.
        QPoint at = topLeft;
        at.setX(std::max(0, std::min(at.x(), top->width() - size.width())));
        if (at.y() + size.height() > top->height())
            at.setY(std::max(0, topLeft.y() - size.height() - 20));
        setGeometry(QRect(at, size));
        killTimers();
        // Long texts stay longer: ten seconds and some more for every character.
        m_timer = startTimer(10000 + 40 * static_cast<int>(text.size()));
        raise();
        show();
        update();
    }

    void close() {
        killTimers();
        m_owner = nullptr;
        if (!isHidden())
            hide();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setFont(font());
        QStyleOption option;
        option.initFrom(this);
        style()->drawPrimitive(QStyle::PE_PanelTipLabel, &option, &painter, this);
        if (m_tip.rich) {
            paintRich(painter);
            return;
        }
        painter.setPen(palette().color(QPalette::ToolTipText));
        painter.drawText(rect().adjusted(marginX(), marginY(), -marginX(), -marginY()),
                         Qt::AlignLeft | (isMaterial() ? Qt::AlignVCenter : Qt::AlignTop), m_text);
    }
    void timerEvent(QTimerEvent *) override { close(); }

private:
    static constexpr int kMarginX = 5;
    static constexpr int kMarginY = 3;
    bool isMaterial() const { return qobject_cast<const QMaterialStyle *>(style()) != nullptr; }
    // A plain tooltip of the Material style has 8 at its sides and 4 above and below, and is at least 24 high.
    int marginX() const { return isMaterial() ? 8 : kMarginX; }
    int marginY() const { return isMaterial() ? 4 : kMarginY; }

    // A rich tooltip: 16 at its sides, 12 above and below, at most 320 wide, and 2 below it for its shadow. Its
    // subhead is title small, 4 above the supporting text, and both are in OnSurfaceVariant.
    static constexpr int kRichMarginX = 16;
    static constexpr int kRichMarginY = 12;
    static constexpr int kRichTitleSpace = 4;
    static constexpr int kRichWidest = 320;
    static constexpr int kRichShadow = 2;
    static constexpr int kTextFlags = Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap;
    QFont titleFont() const {
        QFont title = font();
        title.setWeight(QFont::Medium);
        return title;
    }
    // The room of the subhead and of the supporting text, each wrapped to the width that the tooltip may have.
    QRect titleRect() const {
        if (m_tip.title.isEmpty())
            return QRect();
        return QFontMetrics(titleFont()).boundingRect(QRect(0, 0, m_textWidth, 100000), kTextFlags, m_tip.title);
    }
    QRect bodyRect() const {
        if (m_tip.body.isEmpty())
            return QRect();
        return fontMetrics().boundingRect(QRect(0, 0, m_textWidth, 100000), kTextFlags, m_tip.body);
    }
    QSize richSize(int windowWidth) {
        m_textWidth = std::max(1, std::min(kRichWidest, windowWidth) - 2 * kRichMarginX);
        const QRect title = titleRect();
        const QRect body = bodyRect();
        const int space = title.isEmpty() || body.isEmpty() ? 0 : kRichTitleSpace;
        const int width = std::min(m_textWidth, std::max(title.width(), body.width()));
        return QSize(width + 2 * kRichMarginX, title.height() + space + body.height() + 2 * kRichMarginY + kRichShadow);
    }
    void paintRich(QPainter &painter) const {
        const auto *material = qobject_cast<const QMaterialStyle *>(style());
        const QColor ink =
            material ? material->color(QMaterialStyle::Role::OnSurfaceVariant) : palette().color(QPalette::ToolTipText);
        const int width = this->width() - 2 * kRichMarginX;
        int y = kRichMarginY;
        painter.setPen(ink);
        if (!m_tip.title.isEmpty()) {
            painter.setFont(titleFont());
            const int height = titleRect().height();
            painter.drawText(QRect(kRichMarginX, y, width, height), kTextFlags, m_tip.title);
            y += height + kRichTitleSpace;
        }
        if (!m_tip.body.isEmpty()) {
            painter.setFont(font());
            painter.drawText(QRect(kRichMarginX, y, width, bodyRect().height()), kTextFlags, m_tip.body);
        }
    }

    QSize textSize() const {
        const QSize text = fontMetrics().size(0, m_text);
        const int height = text.height() + 2 * marginY();
        return QSize(text.width() + 2 * marginX(), isMaterial() ? std::max(24, height) : height);
    }
    void killTimers() {
        if (m_timer) {
            killTimer(m_timer);
            m_timer = 0;
        }
    }

    QString m_text;
    RichTip m_tip;
    int m_textWidth = 0;
    QPointer<QWidget> m_owner;
    QRect m_rect;
    int m_timer = 0;
};

QPointer<TipLabel> g_tip;

} // namespace

namespace QtPrivate {

bool toolTipFollowsMouse(QWidget *under, const QPoint &pos) {
    if (!g_tip || g_tip->isHidden())
        return false;
    // The tool tip stays while the mouse is on the widget it is for, and within the rectangle that it was given.
    const QRect rect = g_tip->rectOfOwner();
    if (g_tip->owner() != under || (!rect.isNull() && !rect.contains(pos))) {
        g_tip->close();
        return false;
    }
    return true;
}

} // namespace QtPrivate

/*!
    \class QToolTip
    \brief The QToolTip class shows a text in a small label next to the mouse.

    A tool tip explains a widget. Most of the time QWidget::setToolTip() is all that is needed: the window shows
    the tool tip of the widget under the mouse when the mouse has rested there for a moment, and takes it away when the
    mouse moves to another widget, a mouse button or a key is pressed, or the mouse leaves the window.

    \code
    button->setToolTip("Saves the document");
    \endcode

    A tool tip next to its button, below and to the right of the mouse.

    \image widgets/qtooltip.png QToolTip in the light color scheme

    To show a tool tip that depends on where the mouse is, as QTableWidget does for the cells, reimplement
    QWidget::event() for the QEvent::ToolTip event and call showText() from it. The tool tip is a label inside
    the window of the widget, not a window of its own, so it never reaches outside the window. It is drawn by the style
    with the \c ToolTipBase and \c ToolTipText colors of the palette.

    \differences
    There is no \c showText() with a display time,
    no rich text (with QMaterialStyle, rich text makes a rich tooltip: a bold run or a heading at its start is the
    subhead, and the rest of the markup is left out), and no \c QToolTip::isVisible() for a tool tip of another window. A tool tip is always inside the
    window, so one that shows a long text is not wider than the window.
    \enddifferences

    \sa QWidget::toolTip(), QHelpEvent
*/

/*!
    \since 6.0

    Returns the font of tool tips: the one setFont() set, or the font of the application.
*/
QFont QToolTip::font() {
    const std::optional<QFont> &font = tipFont();
    return font.has_value() ? *font : QGuiApplication::font();
}

/*!
    \since 6.0

    Sets the font of tool tips to \a font.
*/
void QToolTip::setFont(const QFont &font) { tipFont() = font; }

/*!
    \since 6.0

    Returns the palette of tool tips: the one setPalette() set, or the palette of the application.
*/
QPalette QToolTip::palette() {
    const std::optional<QPalette> &palette = tipPalette();
    return palette.has_value() ? *palette : QApplication::palette();
}

/*!
    \since 6.0

    Sets the palette of tool tips to \a palette; its QPalette::ToolTipBase and QPalette::ToolTipText colors are the
    ones used.
*/
void QToolTip::setPalette(const QPalette &palette) { tipPalette() = palette; }

/*!
    Shows \a text in a tool tip, with its top left corner a little below and to the right of \a pos, which is on the
    screen. \a w is the widget that the tool tip is for: its window holds the tool tip. If \a w is \c nullptr, the active
    window does, or else the first window. If \a rect is not empty, the tool tip goes away when the mouse leaves it; it
    is in the coordinates of \a w. An empty \a text hides the tool tip. A tool tip that is shown replaces the one before.

    \sa hideText()
*/
void QToolTip::showText(const QPoint &pos, const QString &text, QWidget *w, const QRect &rect) {
    if (text.isEmpty()) {
        hideText();
        return;
    }
    QWidget *top = w ? w->window() : QApplication::activeWindow();
    if (!top) {
        const QList<QWidget *> windows = QApplication::topLevelWidgets();
        for (QWidget *candidate : windows) {
            if (candidate->isVisible()) {
                top = candidate;
                break;
            }
        }
    }
    if (!top)
        return;
    if (!g_tip || g_tip->parentWidget() != top) {
        delete g_tip.data();
        g_tip = new TipLabel(top);
    }
    g_tip->open(text, top->mapFromGlobal(pos) + QPoint(2, 16), w, rect);
}

/*!
    Hides the tool tip, if one is shown.

    \sa showText()
*/
void QToolTip::hideText() {
    if (g_tip)
        g_tip->close();
}

/*!
    Returns whether a tool tip is shown.

    \sa text()
*/
bool QToolTip::isVisible() { return g_tip && !g_tip->isHidden(); }

/*!
    Returns the text of the tool tip that is shown, or an empty string if there is none.

    \sa isVisible()
*/
QString QToolTip::text() { return isVisible() ? g_tip->text() : QString(); }
