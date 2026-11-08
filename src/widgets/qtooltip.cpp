// MIT License - see LICENSE
#include <QtWidgets/qtooltip.h>

#include "qtooltip_p.h"
#include "qwidget_p.h"

#include <QtCore/qpointer.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpainter.h>
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
        setFont(QGuiApplication::font());
        setPalette(QApplication::palette());
        const QSize size = textSize();
        QWidget *top = parentWidget();
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
        painter.setPen(palette().color(QPalette::ToolTipText));
        painter.drawText(rect().adjusted(kMarginX, kMarginY, -kMarginX, -kMarginY), Qt::AlignLeft | Qt::AlignTop,
                         m_text);
    }
    void timerEvent(QTimerEvent *) override { close(); }

private:
    static constexpr int kMarginX = 5;
    static constexpr int kMarginY = 3;

    QSize textSize() const {
        const QSize text = fontMetrics().size(0, m_text);
        return QSize(text.width() + 2 * kMarginX, text.height() + 2 * kMarginY);
    }
    void killTimers() {
        if (m_timer) {
            killTimer(m_timer);
            m_timer = 0;
        }
    }

    QString m_text;
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
    There are no \c setPalette(), \c setFont(), \c palette(), \c font() and \c showText() with a display time,
    no rich text, and no \c QToolTip::isVisible() for a tool tip of another window. A tool tip is always inside the
    window, so one that shows a long text is not wider than the window.
    \enddifferences

    \sa QWidget::toolTip(), QHelpEvent
*/

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
