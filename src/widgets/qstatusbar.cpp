// MIT License - see LICENSE
#include <QtWidgets/qstatusbar.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qlayoutitem.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>

namespace {
constexpr int kSideMargin = 6;
constexpr int kTopMargin = 3;
constexpr int kBottomMargin = 2;
constexpr int kSpacing = 4;
} // namespace

/*!
    \class QStatusBar
    \brief The QStatusBar class is a bar for messages and small widgets at the bottom of a window.

    A status bar shows a message, which goes away after a time or when it is cleared, and has widgets: the ones that
    addWidget() adds are on the left and hidden while a message is shown, and the ones that addPermanentWidget() adds are on
    the right and always there:

    \code
    QStatusBar *bar = new QStatusBar(window);
    bar->addPermanentWidget(new QLabel("Line 1, column 1"));
    bar->showMessage("File saved", 3000);
    \endcode

    A status bar with a message, and one with a widget on each side.

    \image widgets/qstatusbar.png QStatusBar in the light color scheme

    \differences
    There is no \c QMainWindow to have a status bar, so put it at the bottom of a layout. The size grip is not drawn:
    \c setSizeGripEnabled() is only a property. There is no \c reformat() and no \c hideOrShow(), and a message cannot be
    set by a \c QWidget::statusTip.
    \enddifferences

    \sa QLabel, QProgressBar
*/

/*!
    Constructs a status bar with no message and the given \a parent. It is as high as its widgets need, and as wide as
    its parent.
*/
QStatusBar::QStatusBar(QWidget *parent) : QWidget(parent) { setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed); }

/*!
    Destroys the status bar.
*/
QStatusBar::~QStatusBar() { *m_alive = false; }

void QStatusBar::insertEntry(int index, QWidget *widget, int stretch, bool permanent) {
    if (!widget)
        return;
    removeWidget(widget);
    // The normal widgets come before the permanent ones, in the order they were added; `index` counts within the kind.
    std::vector<int> positions;
    for (int i = 0; i < static_cast<int>(m_entries.size()); ++i)
        if (m_entries[static_cast<std::size_t>(i)].permanent == permanent)
            positions.push_back(i);
    int at;
    if (index < 0 || index >= static_cast<int>(positions.size()))
        at = permanent ? static_cast<int>(m_entries.size())
                       : static_cast<int>(positions.empty() ? 0 : positions.back() + 1);
    else
        at = positions[static_cast<std::size_t>(index)];
    if (!permanent && positions.empty()) {
        // The first normal widget goes before all the permanent ones.
        at = 0;
    }
    widget->setParent(this);
    m_entries.insert(m_entries.begin() + at, Entry{widget, std::max(0, stretch), permanent, false});
    if (!permanent && !m_message.isEmpty()) {
        // A widget that comes during a message is meant to be seen when the message is gone.
        m_entries[static_cast<std::size_t>(at)].hiddenByUs = true;
        widget->hide();
    } else {
        widget->show();
    }
    QObject::connect(widget, &QObject::destroyed, this, [this, alive = m_alive](QObject *w) {
        if (*alive)
            entryDestroyed(w);
    });
    updateGeometry();
    layoutEntries();
}

void QStatusBar::entryDestroyed(QObject *widget) {
    for (std::size_t i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].widget == widget) {
            m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(i));
            break;
        }
    updateGeometry();
    layoutEntries();
}

/*!
    Adds \a widget on the left, after the ones that are there, with the \a stretch factor that says how much of the extra
    room it takes. It is hidden while a message is shown. The status bar makes it a child.

    \sa insertWidget(), addPermanentWidget()
*/
void QStatusBar::addWidget(QWidget *widget, int stretch) { insertEntry(-1, widget, stretch, false); }

/*!
    Inserts \a widget among the widgets on the left, at \a index, with the given \a stretch factor.

    \sa addWidget()
*/
void QStatusBar::insertWidget(int index, QWidget *widget, int stretch) { insertEntry(index, widget, stretch, false); }

/*!
    Adds \a widget on the right, after the ones that are there, with the \a stretch factor. It is not hidden by a message.

    \sa insertPermanentWidget(), addWidget()
*/
void QStatusBar::addPermanentWidget(QWidget *widget, int stretch) { insertEntry(-1, widget, stretch, true); }

/*!
    Inserts \a widget among the widgets on the right, at \a index, with the given \a stretch factor.

    \sa addPermanentWidget()
*/
void QStatusBar::insertPermanentWidget(int index, QWidget *widget, int stretch) {
    insertEntry(index, widget, stretch, true);
}

/*!
    Removes \a widget from the status bar. It is not deleted, and is hidden; it stays a child.

    \sa addWidget()
*/
void QStatusBar::removeWidget(QWidget *widget) {
    for (std::size_t i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].widget == widget) {
            m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(i));
            widget->hide();
            QObject::disconnect(widget, &QObject::destroyed, this, nullptr);
            updateGeometry();
            layoutEntries();
            return;
        }
}

/*!
    \fn QString QStatusBar::currentMessage() const

    Returns the message that is shown, or an empty text if there is none.

    \sa showMessage()
*/

/*!
    \fn bool QStatusBar::isSizeGripEnabled() const

    Returns the size grip property, \c false by default. It has no effect.
*/

/*!
    \fn void QStatusBar::setSizeGripEnabled(bool enabled)

    Sets the size grip property to \a enabled. It has no effect.
*/

/*!
    Shows \a text in the status bar in place of the widgets on the left. If \a timeout is more than 0, the message goes
    away after that many milliseconds; otherwise it stays until clearMessage() or another message. Emits messageChanged().

    \sa clearMessage()
*/
void QStatusBar::showMessage(const QString &text, int timeout) {
    if (m_timer) {
        killTimer(m_timer);
        m_timer = 0;
    }
    if (text.isEmpty()) {
        clearMessage();
        return;
    }
    const bool first = m_message.isEmpty();
    m_message = text;
    if (first) {
        for (Entry &e : m_entries) {
            if (e.permanent)
                continue;
            e.hiddenByUs = !e.widget->isHidden();
            e.widget->hide();
        }
    }
    if (timeout > 0)
        m_timer = startTimer(timeout);
    layoutEntries();
    update();
    emit messageChanged(m_message);
}

/*!
    Removes the message, and shows the widgets on the left again. Emits messageChanged() if there was a message.

    \sa showMessage()
*/
void QStatusBar::clearMessage() {
    if (m_timer) {
        killTimer(m_timer);
        m_timer = 0;
    }
    if (m_message.isEmpty())
        return;
    m_message.clear();
    for (Entry &e : m_entries) {
        if (!e.permanent && e.hiddenByUs)
            e.widget->show();
        e.hiddenByUs = false;
    }
    layoutEntries();
    update();
    emit messageChanged(QString());
}

/*!
    \reimp
*/
void QStatusBar::timerEvent(QTimerEvent *event) {
    if (event->timerId() == m_timer)
        clearMessage();
    else
        QWidget::timerEvent(event);
}

/*!
    \reimp
*/
QSize QStatusBar::sizeHint() const {
    int width = 2 * kSideMargin, height = fontMetrics().height();
    int shown = 0;
    for (const Entry &e : m_entries) {
        if (e.widget->isHidden() && !e.hiddenByUs)
            continue;
        const QSize hint = QWidgetItem(e.widget).sizeHint();
        width += hint.width() + (shown++ ? kSpacing : 0);
        height = std::max(height, hint.height());
    }
    return QSize(width, height + kTopMargin + kBottomMargin);
}

/*!
    \reimp
*/
QSize QStatusBar::minimumSizeHint() const { return QSize(0, sizeHint().height()); }

void QStatusBar::layoutEntries() {
    const int top = kTopMargin, height = std::max(0, this->height() - kTopMargin - kBottomMargin);
    int right = width() - kSideMargin;
    // The permanent widgets from the right edge
    for (int i = static_cast<int>(m_entries.size()) - 1; i >= 0; --i) {
        const Entry &e = m_entries[static_cast<std::size_t>(i)];
        if (!e.permanent || e.widget->isHidden())
            continue;
        const int w = QWidgetItem(e.widget).sizeHint().width();
        e.widget->setGeometry(right - w, top, w, height);
        right -= w + kSpacing;
    }
    if (!m_message.isEmpty())
        return;
    // The others from the left, sharing out what is left by their stretch
    int left = kSideMargin, used = 0, stretchSum = 0, count = 0;
    for (const Entry &e : m_entries) {
        if (e.permanent || e.widget->isHidden())
            continue;
        used += QWidgetItem(e.widget).sizeHint().width();
        stretchSum += e.stretch;
        ++count;
    }
    const int extra = std::max(0, right - kSideMargin - (used + std::max(0, count - 1) * kSpacing));
    for (const Entry &e : m_entries) {
        if (e.permanent || e.widget->isHidden())
            continue;
        int w = QWidgetItem(e.widget).sizeHint().width();
        if (stretchSum > 0)
            w += extra * e.stretch / stretchSum;
        e.widget->setGeometry(left, top, w, height);
        left += w + kSpacing;
    }
}

/*!
    \reimp
*/
void QStatusBar::resizeEvent(QResizeEvent *event) {
    (void)event;
    layoutEntries();
}

/*!
    \reimp
*/
void QStatusBar::changeEvent(QEvent *event) {
    updateGeometry();
    layoutEntries();
    update();
    QWidget::changeEvent(event);
}

/*!
    \reimp
*/
void QStatusBar::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    QStyleOption option;
    option.initFrom(this);
    style()->drawPrimitive(QStyle::PE_PanelStatusBar, &option, &painter, this);
    if (m_message.isEmpty())
        return;
    int right = width() - kSideMargin;
    for (const Entry &e : m_entries)
        if (e.permanent && !e.widget->isHidden())
            right = std::min(right, e.widget->x() - kSpacing);
    painter.setFont(font());
    painter.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::WindowText));
    const QRect text(kSideMargin, kTopMargin, std::max(0, right - kSideMargin),
                     std::max(0, height() - kTopMargin - kBottomMargin));
    painter.drawText(text, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                     fontMetrics().elidedText(m_message, Qt::ElideRight, text.width()));
}

#include "moc_qstatusbar.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
