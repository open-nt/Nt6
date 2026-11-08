// MIT License - see LICENSE
#include <QtWidgets/qmdisubwindow.h>

#include <QtCore/qcoreapplication.h>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qmdiarea.h>
#include <QtWidgets/qtoolbutton.h>

#include <algorithm>

namespace {

constexpr int kBorder = 4;
constexpr int kMinimizedWidth = 160;

} // namespace

/*!
    \class QMdiSubWindow
    \brief The QMdiSubWindow class is a window inside a QMdiArea.

    A sub-window has a title bar with the window title and buttons that minimize, maximize and close it, and a widget
    under it. QMdiArea::addSubWindow() makes one for a widget:

    \code
    QMdiSubWindow *window = area->addSubWindow(new QPlainTextEdit);
    window->setWindowTitle("Notes");
    window->show();
    \endcode

    The user moves the sub-window by its title bar and resizes it by its edges; double-clicking the title bar maximizes
    it or makes it normal again. A maximized sub-window fills the area and follows its size; a minimized one is only
    its title bar. Closing a sub-window sends a close event to its widget, and deletes the sub-window if the widget
    accepts it. The window title follows that of the widget.

    \differences
    The buttons show "-", "+", "=" and "x" rather than icons, and there is no system menu, no shading
    (\c showShaded()) and no keyboard moving and resizing. The sub-window options are recorded only; moving and resizing
    are never shown with a rubber band. showMaximized(), showMinimized() and showNormal() are functions of the
    sub-window, since QWidget has no window states in Nt.
    \enddifferences

    \sa QMdiArea
*/

/*!
    \enum QMdiSubWindow::SubWindowOption

    Options of a sub-window; recorded only. SubWindowOptions is a typedef for \c{QFlags<SubWindowOption>}.

    \value AllowOutsideAreaHorizontally The sub-window may be moved out of the area at the sides.
    \value AllowOutsideAreaVertically The sub-window may be moved out of the area at the top and the bottom.
    \value RubberBandResize Resizing shows a rubber band.
    \value RubberBandMove Moving shows a rubber band.
*/

/*!
    Constructs an empty sub-window with the given \a parent and window \a flags; Qt::FramelessWindowHint leaves out the
    frame and the title bar.
*/
QMdiSubWindow::QMdiSubWindow(QWidget *parent, Qt::WindowFlags flags) : QWidget(parent) {
    m_frameless = flags.testFlag(Qt::FramelessWindowHint);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    const auto makeButton = [this](const char *text, const char *tip) {
        auto *button = new QToolButton(this);
        button->setText(QString::fromLatin1(text));
        button->setToolTip(QString::fromLatin1(tip));
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        return button;
    };
    m_minimize = makeButton("-", "Minimize");
    m_maximize = makeButton("+", "Maximize");
    m_close = makeButton("x", "Close");
    QObject::connect(m_minimize, &QToolButton::clicked, this, [this] {
        if (isMinimized())
            showNormal();
        else
            showMinimized();
    });
    QObject::connect(m_maximize, &QToolButton::clicked, this, [this] {
        if (isMaximized())
            showNormal();
        else
            showMaximized();
    });
    QObject::connect(m_close, &QToolButton::clicked, this, [this] {
        QCloseEvent close;
        QCoreApplication::sendEvent(this, &close);
    });
    resize(sizeHint());
    updateButtons();
}

/*!
    Destroys the sub-window and its widget; the area forgets it.
*/
QMdiSubWindow::~QMdiSubWindow() {
    if (QMdiArea *area = mdiArea())
        area->forget(this);
}

/*!
    Makes \a widget the widget of the sub-window, under the title bar. The sub-window takes it as a child; the widget
    it had is deleted. The window title follows that of the widget.

    \sa widget()
*/
void QMdiSubWindow::setWidget(QWidget *widget) {
    if (widget == m_widget)
        return;
    if (m_widget) {
        QObject::disconnect(m_widget, nullptr, this, nullptr);
        delete m_widget.data();
    }
    m_widget = widget;
    if (widget) {
        widget->setParent(this);
        widget->show();
        if (!widget->windowTitle().isEmpty())
            setWindowTitle(widget->windowTitle());
        QObject::connect(widget, &QWidget::windowTitleChanged, this, &QWidget::setWindowTitle);
        resize(sizeHint());
    }
    layoutParts();
}

/*!
    \fn QWidget *QMdiSubWindow::widget() const

    Returns the widget of the sub-window, or \c nullptr.
*/

/*!
    Returns the area that the sub-window is in, or \c nullptr.
*/
QMdiArea *QMdiSubWindow::mdiArea() const {
    for (QWidget *w = parentWidget(); w; w = w->parentWidget())
        if (auto *area = dynamic_cast<QMdiArea *>(w))
            return area;
    return nullptr;
}

/*!
    Sets \a option if \a on is \c true, and clears it otherwise.
*/
void QMdiSubWindow::setOption(SubWindowOption option, bool on) { m_options.setFlag(option, on); }

/*!
    \fn bool QMdiSubWindow::testOption(SubWindowOption option) const

    Returns whether \a option is set.
*/

/*!
    \fn Qt::WindowStates QMdiSubWindow::windowState() const

    Returns the state of the sub-window: whether it is maximized, minimized or active.
*/

/*!
    \fn bool QMdiSubWindow::isMaximized() const

    Returns whether the sub-window is maximized.
*/

/*!
    \fn bool QMdiSubWindow::isMinimized() const

    Returns whether the sub-window is minimized.
*/

/*!
    \reimp
*/
QSize QMdiSubWindow::sizeHint() const {
    const QSize content = m_widget ? m_widget->sizeHint().expandedTo(m_widget->minimumSizeHint()) : QSize(200, 120);
    return QSize(std::max(content.width(), kMinimizedWidth) + 2 * border(),
                 std::max(0, content.height()) + titleHeight() + 2 * border());
}

/*!
    \reimp
*/
QSize QMdiSubWindow::minimumSizeHint() const {
    return QSize(3 * titleHeight() + 2 * border() + 40, titleHeight() + 2 * border());
}

/*!
    Maximizes the sub-window: it fills its area, and follows the size of the area.

    \sa showNormal()
*/
void QMdiSubWindow::showMaximized() {
    if (!isMaximized() && !isMinimized())
        m_normalGeometry = geometry();
    Qt::WindowStates state = m_state;
    state.setFlag(Qt::WindowMinimized, false);
    state.setFlag(Qt::WindowMaximized, true);
    setState(state);
    if (QWidget *p = parentWidget())
        setGeometry(p->rect());
    if (m_widget)
        m_widget->show();
    show();
    raise();
}

/*!
    Minimizes the sub-window to its title bar.

    \sa showNormal()
*/
void QMdiSubWindow::showMinimized() {
    if (!isMaximized() && !isMinimized())
        m_normalGeometry = geometry();
    Qt::WindowStates state = m_state;
    state.setFlag(Qt::WindowMaximized, false);
    state.setFlag(Qt::WindowMinimized, true);
    setState(state);
    if (m_widget)
        m_widget->hide();
    const QPoint at = m_normalGeometry.isValid() ? m_normalGeometry.topLeft() : pos();
    setGeometry(QRect(at, QSize(kMinimizedWidth, titleHeight() + 2 * border())));
    show();
}

/*!
    Makes the sub-window normal again, with the geometry it had before it was maximized or minimized.
*/
void QMdiSubWindow::showNormal() {
    Qt::WindowStates state = m_state;
    const bool wasChanged = state & (Qt::WindowMaximized | Qt::WindowMinimized);
    state.setFlag(Qt::WindowMaximized, false);
    state.setFlag(Qt::WindowMinimized, false);
    setState(state);
    if (m_widget)
        m_widget->show();
    if (wasChanged && m_normalGeometry.isValid())
        setGeometry(m_normalGeometry);
    show();
}

void QMdiSubWindow::setState(Qt::WindowStates state) {
    if (state == m_state)
        return;
    const Qt::WindowStates old = m_state;
    m_state = state;
    updateButtons();
    layoutParts();
    update();
    emit windowStateChanged(old, state);
}

void QMdiSubWindow::setActive(bool active) {
    if (m_active == active)
        return;
    m_active = active;
    Qt::WindowStates state = m_state;
    state.setFlag(Qt::WindowActive, active);
    setState(state);
    update();
}

void QMdiSubWindow::updateButtons() {
    m_maximize->setText(isMaximized() ? QStringLiteral("=") : QStringLiteral("+"));
    m_maximize->setToolTip(isMaximized() ? QStringLiteral("Restore") : QStringLiteral("Maximize"));
    m_minimize->setText(isMinimized() ? QStringLiteral("=") : QStringLiteral("-"));
    for (QToolButton *button : {m_minimize, m_maximize, m_close})
        button->setVisible(!m_frameless);
}

int QMdiSubWindow::titleHeight() const { return m_frameless ? 0 : fontMetrics().height() + 6; }

int QMdiSubWindow::border() const { return m_frameless || isMaximized() ? 0 : kBorder; }

QRect QMdiSubWindow::titleRect() const { return QRect(border(), border(), width() - 2 * border(), titleHeight()); }

int QMdiSubWindow::edgeAt(const QPoint &pos) const {
    if (m_frameless || isMaximized() || isMinimized())
        return NoEdge;
    int edges = NoEdge;
    if (pos.x() < kBorder)
        edges |= Left;
    else if (pos.x() >= width() - kBorder)
        edges |= Right;
    if (pos.y() < kBorder)
        edges |= Top;
    else if (pos.y() >= height() - kBorder)
        edges |= Bottom;
    return edges;
}

void QMdiSubWindow::layoutParts() {
    if (!m_close)
        return;
    const QRect title = titleRect();
    const int side = title.height();
    int right = title.right() + 1;
    for (QToolButton *button : {m_close, m_maximize, m_minimize}) {
        right -= side;
        button->setGeometry(right, title.top(), side, side);
    }
    if (m_widget && !isMinimized())
        m_widget->setGeometry(QRect(border(), title.bottom() + 1, std::max(0, width() - 2 * border()),
                                    std::max(0, height() - title.height() - 2 * border())));
}

/*!
    \reimp
*/
void QMdiSubWindow::paintEvent(QPaintEvent *event) {
    (void)event;
    if (m_frameless)
        return;
    QPainter painter(this);
    const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    painter.fillRect(rect(), palette().color(group, QPalette::Window));
    painter.setPen(palette().color(group, QPalette::Mid));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
    const QRect title = titleRect();
    painter.fillRect(title, palette().color(group, m_active ? QPalette::Highlight : QPalette::Button));
    painter.setPen(palette().color(group, m_active ? QPalette::HighlightedText : QPalette::ButtonText));
    painter.drawText(title.adjusted(6, 0, -3 * title.height() - 4, 0),
                     Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, windowTitle());
}

/*!
    \reimp
*/
void QMdiSubWindow::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    layoutParts();
}

/*!
    \reimp

    A press activates the sub-window, and starts moving it by the title bar or resizing it by an edge.
*/
void QMdiSubWindow::mousePressEvent(QMouseEvent *event) {
    if (QMdiArea *area = mdiArea())
        area->setActiveSubWindow(this);
    if (event->button() != Qt::LeftButton || !parentWidget()) {
        event->accept();
        return;
    }
    m_pressPos = parentWidget()->mapFromGlobal(event->globalPosition().toPoint());
    m_pressGeometry = geometry();
    m_dragEdges = edgeAt(event->pos());
    m_moving = m_dragEdges == NoEdge && titleRect().contains(event->pos()) && !isMaximized();
    event->accept();
}

/*!
    \reimp
*/
void QMdiSubWindow::mouseMoveEvent(QMouseEvent *event) {
    if (!(event->buttons() & Qt::LeftButton) || !parentWidget() || (!m_moving && m_dragEdges == NoEdge)) {
        event->accept();
        return;
    }
    const QPoint delta = parentWidget()->mapFromGlobal(event->globalPosition().toPoint()) - m_pressPos;
    if (m_moving) {
        move(m_pressGeometry.topLeft() + delta);
        event->accept();
        return;
    }
    QRect r = m_pressGeometry;
    const QSize minimum = minimumSizeHint();
    if (m_dragEdges & Left)
        r.setLeft(std::min(r.left() + delta.x(), r.right() + 1 - minimum.width()));
    if (m_dragEdges & Right)
        r.setRight(std::max(r.right() + delta.x(), r.left() + minimum.width() - 1));
    if (m_dragEdges & Top)
        r.setTop(std::min(r.top() + delta.y(), r.bottom() + 1 - minimum.height()));
    if (m_dragEdges & Bottom)
        r.setBottom(std::max(r.bottom() + delta.y(), r.top() + minimum.height() - 1));
    setGeometry(r);
    event->accept();
}

/*!
    \reimp
*/
void QMdiSubWindow::mouseReleaseEvent(QMouseEvent *event) {
    m_moving = false;
    m_dragEdges = NoEdge;
    event->accept();
}

/*!
    \reimp

    Double-clicking the title bar maximizes the sub-window, or makes it normal again.
*/
void QMdiSubWindow::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && titleRect().contains(event->pos())) {
        if (isMaximized() || isMinimized())
            showNormal();
        else
            showMaximized();
    }
    event->accept();
}

/*!
    \reimp

    The close event goes to the widget first; if the widget accepts it, the sub-window is hidden, leaves its area and is
    deleted later.
*/
void QMdiSubWindow::closeEvent(QCloseEvent *event) {
    if (m_widget) {
        QCloseEvent close;
        QCoreApplication::sendEvent(m_widget, &close);
        if (!close.isAccepted()) {
            event->ignore();
            return;
        }
    }
    event->accept();
    hide();
    if (QMdiArea *area = mdiArea())
        area->forget(this);
    deleteLater();
}

/*!
    \reimp
*/
void QMdiSubWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::WindowTitleChange) {
        update();
        if (QMdiArea *area = mdiArea())
            area->syncTabs();
    }
    if (event->type() == QEvent::FontChange)
        layoutParts();
    QWidget::changeEvent(event);
}

/*!
    \reimp

    Getting the focus activates the sub-window.
*/
void QMdiSubWindow::focusInEvent(QFocusEvent *event) {
    QWidget::focusInEvent(event);
    if (QMdiArea *area = mdiArea())
        area->setActiveSubWindow(this);
}

#include "moc_qmdisubwindow.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
