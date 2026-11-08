// MIT License - see LICENSE
#include <QtWidgets/qmdiarea.h>

#include <QtCore/qcoreapplication.h>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qtabbar.h>

#include <algorithm>
#include <cmath>

/*!
    \class QMdiArea
    \brief The QMdiArea class provides an area in which sub-windows are shown, for an application with many documents.

    An MDI area holds QMdiSubWindow windows: it lays them out, keeps one of them active, and can tile or cascade them.
    It is usually the central widget of a QMainWindow:

    \code
    auto *area = new QMdiArea;
    window.setCentralWidget(area);
    QMdiSubWindow *notes = area->addSubWindow(new QPlainTextEdit);
    notes->setWindowTitle("Notes");
    notes->show();
    connect(area, &QMdiArea::subWindowActivated, [](QMdiSubWindow *w) { updateMenus(w); });
    \endcode

    In TabbedView mode the area shows a tab bar with a tab for each sub-window, and the active sub-window fills the area
    without its frame.

    \differences
    The scroll bars are always off: sub-windows that are moved out of the area are cut off. There is no
    \c setDocumentMode(), \c setTabShape() or \c setTabPosition() (the tabs are at the top), and the background is
    filled with a color, not tiled with an image.
    \enddifferences

    \sa QMdiSubWindow
*/

/*!
    \enum QMdiArea::AreaOption

    Options of the area. AreaOptions is a typedef for \c{QFlags<AreaOption>}.

    \value DontMaximizeSubWindowOnActivation Activating a sub-window does not maximize it when the active one was
           maximized.
*/

/*!
    \enum QMdiArea::WindowOrder

    The orders of subWindowList(), and of activateNextSubWindow().

    \value CreationOrder The order the sub-windows were added in.
    \value StackingOrder From the bottom to the top.
    \value ActivationHistoryOrder From the least recently to the most recently active.
*/

/*!
    \enum QMdiArea::ViewMode

    How the sub-windows are shown.

    \value SubWindowView As windows with frames, the default.
    \value TabbedView As tabs: the active one fills the area.
*/

/*!
    Constructs an empty MDI area with the given \a parent.
*/
QMdiArea::QMdiArea(QWidget *parent) : QAbstractScrollArea(parent) {
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

/*!
    Destroys the area and its sub-windows.
*/
QMdiArea::~QMdiArea() {
    m_windows.clear();
    m_history.clear();
}

/*!
    Adds \a widget to the area in a new sub-window with the window \a flags, and returns the sub-window. A
    QMdiSubWindow is added as it is. The sub-window is shown with the area; if the area is visible already, call
    show() on it. The first sub-window becomes active.

    \sa removeSubWindow()
*/
QMdiSubWindow *QMdiArea::addSubWindow(QWidget *widget, Qt::WindowFlags flags) {
    if (!widget)
        return nullptr;
    auto *window = dynamic_cast<QMdiSubWindow *>(widget);
    if (!window) {
        window = new QMdiSubWindow(viewport(), flags);
        window->setWidget(widget);
    } else if (window->parentWidget() != viewport()) {
        window->setParent(viewport());
    }
    if (std::find(m_windows.begin(), m_windows.end(), window) == m_windows.end())
        m_windows.emplace_back(window);
    // Each new sub-window a step further down and to the right.
    const int step = window->titleHeight() + 4;
    const int n = static_cast<int>(m_windows.size()) - 1;
    window->move((n % 8) * step, (n % 8) * step);
    if (m_viewMode == TabbedView) {
        window->m_frameless = true;
        window->updateButtons();
        syncTabs();
    }
    if (!m_active)
        setActiveSubWindow(window);
    return window;
}

/*!
    Removes \a widget from the area without deleting it. For a QMdiSubWindow, the sub-window leaves the area and no
    longer has a parent; for the widget of a sub-window, the widget leaves its sub-window, which stays.

    \sa addSubWindow()
*/
void QMdiArea::removeSubWindow(QWidget *widget) {
    if (auto *window = dynamic_cast<QMdiSubWindow *>(widget)) {
        forget(window);
        window->hide();
        window->setParent(nullptr);
        return;
    }
    for (const QPointer<QMdiSubWindow> &window : m_windows) {
        if (window && window->widget() == widget) {
            QObject::disconnect(widget, nullptr, window, nullptr);
            window->m_widget = nullptr;
            widget->hide();
            widget->setParent(nullptr);
            return;
        }
    }
}

/*!
    Returns the sub-windows in the given \a order.
*/
QList<QMdiSubWindow *> QMdiArea::subWindowList(WindowOrder order) const {
    QList<QMdiSubWindow *> list;
    switch (order) {
    case CreationOrder:
        for (const QPointer<QMdiSubWindow> &w : m_windows)
            if (w)
                list.append(w.data());
        break;
    case StackingOrder:
        for (QObject *child : viewport()->children())
            if (auto *w = dynamic_cast<QMdiSubWindow *>(child))
                if (std::find(m_windows.begin(), m_windows.end(), w) != m_windows.end())
                    list.append(w);
        break;
    case ActivationHistoryOrder:
        for (const QPointer<QMdiSubWindow> &w : m_windows)
            if (w && std::find(m_history.begin(), m_history.end(), w) == m_history.end())
                list.append(w.data());
        for (const QPointer<QMdiSubWindow> &w : m_history)
            if (w)
                list.append(w.data());
        break;
    }
    return list;
}

/*!
    Returns the sub-window that is current: the active one, or the one that was active last while the window of the
    area was not active.
*/
QMdiSubWindow *QMdiArea::currentSubWindow() const { return m_active.data(); }

/*!
    Returns the active sub-window, or \c nullptr.
*/
QMdiSubWindow *QMdiArea::activeSubWindow() const { return m_active.data(); }

/*!
    \fn QBrush QMdiArea::background() const

    Returns the brush that fills the area behind the sub-windows; by default the dark color of the palette.
*/

/*!
    Sets the brush that fills the area behind the sub-windows to \a background.
*/
void QMdiArea::setBackground(const QBrush &background) {
    m_background = background;
    viewport()->update();
}

/*!
    \fn QMdiArea::WindowOrder QMdiArea::activationOrder() const

    Returns the order of activateNextSubWindow(), CreationOrder by default.
*/

/*!
    \fn void QMdiArea::setActivationOrder(WindowOrder order)

    Sets the order of activateNextSubWindow() to \a order.
*/

/*!
    Sets \a option if \a on is \c true, and clears it otherwise.
*/
void QMdiArea::setOption(AreaOption option, bool on) { m_options.setFlag(option, on); }

/*!
    \fn bool QMdiArea::testOption(AreaOption option) const

    Returns whether \a option is set.
*/

/*!
    \fn QMdiArea::ViewMode QMdiArea::viewMode() const

    Returns how the sub-windows are shown, SubWindowView by default.
*/

/*!
    Sets how the sub-windows are shown to \a mode.
*/
void QMdiArea::setViewMode(ViewMode mode) {
    if (m_viewMode == mode)
        return;
    m_viewMode = mode;
    for (const QPointer<QMdiSubWindow> &w : m_windows) {
        if (!w)
            continue;
        w->m_frameless = mode == TabbedView;
        w->updateButtons();
        w->layoutParts();
    }
    if (mode == TabbedView) {
        tabBar();
        m_tabs->show();
        syncTabs();
    } else {
        if (m_tabs)
            m_tabs->hide();
        setViewportMargins(0, 0, 0, 0);
        for (const QPointer<QMdiSubWindow> &w : m_windows)
            if (w && w->isMaximized())
                w->showNormal();
        for (const QPointer<QMdiSubWindow> &w : m_windows)
            if (w)
                w->show();
    }
    layoutTabbed();
}

/*!
    Returns whether the tabs of the tabbed view have close buttons. The default is \c false.
*/
bool QMdiArea::tabsClosable() const { return m_tabs && m_tabs->tabsClosable(); }

/*!
    Sets whether the tabs of the tabbed view have close buttons to \a closable.
*/
void QMdiArea::setTabsClosable(bool closable) {
    tabBar()->setTabsClosable(closable);
    layoutTabbed();
}

/*!
    Returns whether the tabs of the tabbed view can be dragged into another order. The default is \c false.
*/
bool QMdiArea::tabsMovable() const { return m_tabs && m_tabs->isMovable(); }

/*!
    Sets whether the tabs of the tabbed view can be dragged into another order to \a movable.
*/
void QMdiArea::setTabsMovable(bool movable) { tabBar()->setMovable(movable); }

/*!
    \reimp
*/
QSize QMdiArea::sizeHint() const { return QSize(640, 480); }

/*!
    \reimp
*/
QSize QMdiArea::minimumSizeHint() const { return QSize(120, 80); }

/*!
    Makes \a window the active sub-window: it is raised, gets the focus, and the one that was active stops being so.
    Emits subWindowActivated() if it changes. With \c nullptr no sub-window is active.

    \sa activeSubWindow()
*/
void QMdiArea::setActiveSubWindow(QMdiSubWindow *window) {
    if (window && std::find(m_windows.begin(), m_windows.end(), window) == m_windows.end())
        return;
    activate(window);
}

void QMdiArea::activate(QMdiSubWindow *window) {
    if (window == m_active)
        return;
    QMdiSubWindow *old = m_active;
    const bool maximize = old && old->isMaximized() && !testOption(DontMaximizeSubWindowOnActivation);
    if (window)
        emit window->aboutToActivate();
    if (old)
        old->setActive(false);
    m_active = window;
    if (window) {
        m_history.erase(std::remove(m_history.begin(), m_history.end(), window), m_history.end());
        m_history.emplace_back(window);
        window->setActive(true);
        if (maximize && m_viewMode == SubWindowView)
            window->showMaximized();
        window->raise();
        if (QWidget *w = window->widget(); w && (w->focusPolicy() & Qt::ClickFocus) && !w->hasFocus())
            w->setFocus();
    }
    if (m_viewMode == TabbedView)
        layoutTabbed();
    syncTabs();
    emit subWindowActivated(window);
}

/*!
    Lays the visible sub-windows out in a grid that fills the area.

    \sa cascadeSubWindows()
*/
void QMdiArea::tileSubWindows() {
    const QList<QMdiSubWindow *> windows = visibleWindows();
    if (windows.isEmpty() || m_viewMode == TabbedView)
        return;
    const int n = static_cast<int>(windows.size());
    const int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n))));
    const int rows = (n + columns - 1) / columns;
    const QRect area = viewport()->rect();
    for (int i = 0; i < n; ++i) {
        QMdiSubWindow *w = windows.at(i);
        if (w->isMaximized() || w->isMinimized())
            w->showNormal();
        const int row = i / columns;
        const int column = i % columns;
        // The last row shares the width among the windows it has.
        const int inRow = row == rows - 1 ? n - row * columns : columns;
        const int x0 = area.width() * column / inRow;
        const int x1 = area.width() * (column + 1) / inRow;
        const int y0 = area.height() * row / rows;
        const int y1 = area.height() * (row + 1) / rows;
        w->setGeometry(x0, y0, x1 - x0, y1 - y0);
    }
}

/*!
    Lays the visible sub-windows out one behind the other, each a step further down and to the right.

    \sa tileSubWindows()
*/
void QMdiArea::cascadeSubWindows() {
    const QList<QMdiSubWindow *> windows = visibleWindows();
    if (windows.isEmpty() || m_viewMode == TabbedView)
        return;
    const QRect area = viewport()->rect();
    const int step = windows.first()->titleHeight() + 4;
    const int n = static_cast<int>(windows.size());
    const QSize size(std::max(area.width() - step * (n - 1), area.width() * 2 / 3),
                     std::max(area.height() - step * (n - 1), area.height() * 2 / 3));
    for (int i = 0; i < n; ++i) {
        QMdiSubWindow *w = windows.at(i);
        if (w->isMaximized() || w->isMinimized())
            w->showNormal();
        w->setGeometry(QRect(QPoint(i * step, i * step), size));
        w->raise();
    }
    if (m_active)
        m_active->raise();
}

/*!
    Closes the active sub-window.
*/
void QMdiArea::closeActiveSubWindow() {
    if (QMdiSubWindow *window = m_active) {
        QCloseEvent close;
        QCoreApplication::sendEvent(window, &close);
    }
}

/*!
    Closes all the sub-windows. A sub-window whose widget ignores the close event stays.
*/
void QMdiArea::closeAllSubWindows() {
    for (QMdiSubWindow *window : subWindowList()) {
        QCloseEvent close;
        QCoreApplication::sendEvent(window, &close);
    }
}

/*!
    Activates the visible sub-window after the active one in the activation order, going round at the end.
*/
void QMdiArea::activateNextSubWindow() {
    QList<QMdiSubWindow *> list = subWindowList(m_order);
    list.erase(std::remove_if(list.begin(), list.end(), [](QMdiSubWindow *w) { return w->isHidden(); }), list.end());
    if (list.isEmpty())
        return;
    const qsizetype at = list.indexOf(m_active.data());
    setActiveSubWindow(list.at(static_cast<int>((at + 1) % list.size())));
}

/*!
    Activates the visible sub-window before the active one in the activation order, going round at the start.
*/
void QMdiArea::activatePreviousSubWindow() {
    QList<QMdiSubWindow *> list = subWindowList(m_order);
    list.erase(std::remove_if(list.begin(), list.end(), [](QMdiSubWindow *w) { return w->isHidden(); }), list.end());
    if (list.isEmpty())
        return;
    const qsizetype at = list.indexOf(m_active.data());
    setActiveSubWindow(list.at(static_cast<int>((at <= 0 ? list.size() : at) - 1)));
}

/*!
    \reimp

    The viewport is filled with the background, and maximized sub-windows follow its size.
*/
bool QMdiArea::viewportEvent(QEvent *event) {
    if (event->type() == QEvent::Paint) {
        QPainter painter(viewport());
        const QBrush brush = m_background.style() != Qt::NoBrush
                                 ? m_background
                                 : QBrush(palette().color(QPalette::Active, QPalette::Dark));
        painter.fillRect(viewport()->rect(), brush);
        return true;
    }
    if (event->type() == QEvent::Resize) {
        for (const QPointer<QMdiSubWindow> &w : m_windows)
            if (w && w->isMaximized())
                w->setGeometry(viewport()->rect());
        layoutTabbed();
    }
    return QAbstractScrollArea::viewportEvent(event);
}

/*!
    \reimp
*/
void QMdiArea::resizeEvent(QResizeEvent *event) {
    QAbstractScrollArea::resizeEvent(event);
    layoutTabbed();
}

QTabBar *QMdiArea::tabBar() {
    if (m_tabs)
        return m_tabs;
    m_tabs = new QTabBar(this);
    QObject::connect(m_tabs, &QTabBar::currentChanged, this, [this](int index) {
        if (m_syncing)
            return;
        const QList<QMdiSubWindow *> windows = subWindowList();
        if (index >= 0 && index < windows.size())
            setActiveSubWindow(windows.at(index));
    });
    QObject::connect(m_tabs, &QTabBar::tabCloseRequested, this, [this](int index) {
        const QList<QMdiSubWindow *> windows = subWindowList();
        if (index >= 0 && index < windows.size()) {
            QCloseEvent close;
            QCoreApplication::sendEvent(windows.at(index), &close);
        }
    });
    QObject::connect(m_tabs, &QTabBar::tabMoved, this, [this](int from, int to) {
        if (from < 0 || to < 0 || from >= static_cast<int>(m_windows.size()) ||
            to >= static_cast<int>(m_windows.size()))
            return;
        const QPointer<QMdiSubWindow> moved = m_windows[static_cast<std::size_t>(from)];
        m_windows.erase(m_windows.begin() + from);
        m_windows.insert(m_windows.begin() + to, moved);
    });
    m_tabs->hide();
    return m_tabs;
}

void QMdiArea::forget(QMdiSubWindow *window) {
    const bool wasActive = window == m_active;
    m_windows.erase(std::remove(m_windows.begin(), m_windows.end(), window), m_windows.end());
    m_history.erase(std::remove(m_history.begin(), m_history.end(), window), m_history.end());
    const auto gone = [](const QPointer<QMdiSubWindow> &w) { return w.isNull(); };
    m_windows.erase(std::remove_if(m_windows.begin(), m_windows.end(), gone), m_windows.end());
    m_history.erase(std::remove_if(m_history.begin(), m_history.end(), gone), m_history.end());
    if (wasActive) {
        m_active = nullptr;
        QMdiSubWindow *next = nullptr;
        for (auto it = m_history.rbegin(); it != m_history.rend() && !next; ++it)
            if (*it && !(*it)->isHidden())
                next = *it;
        activate(next);
        if (!next)
            emit subWindowActivated(nullptr);
    }
    syncTabs();
    layoutTabbed();
}

// In the tabbed view the tab bar is above the viewport and only the active sub-window is shown, filling it.
void QMdiArea::layoutTabbed() {
    if (m_viewMode != TabbedView || !m_tabs)
        return;
    const int bar = m_tabs->sizeHint().height();
    setViewportMargins(0, bar, 0, 0);
    m_tabs->setGeometry(0, 0, width(), bar);
    for (const QPointer<QMdiSubWindow> &w : m_windows) {
        if (!w)
            continue;
        if (w == m_active) {
            w->setGeometry(viewport()->rect());
            w->show();
            w->raise();
        } else {
            w->hide();
        }
    }
}

void QMdiArea::syncTabs() {
    if (!m_tabs)
        return;
    m_syncing = true;
    const QList<QMdiSubWindow *> windows = subWindowList();
    while (m_tabs->count() > windows.size())
        m_tabs->removeTab(m_tabs->count() - 1);
    for (int i = 0; i < windows.size(); ++i) {
        QString title = windows.at(i)->windowTitle();
        if (title.isEmpty())
            title = QStringLiteral("(Untitled)");
        if (i < m_tabs->count())
            m_tabs->setTabText(i, title);
        else
            m_tabs->addTab(title);
    }
    m_tabs->setCurrentIndex(static_cast<int>(windows.indexOf(m_active.data())));
    m_syncing = false;
}

QList<QMdiSubWindow *> QMdiArea::visibleWindows() const {
    QList<QMdiSubWindow *> list;
    for (const QPointer<QMdiSubWindow> &w : m_windows)
        if (w && !w->isHidden())
            list.append(w.data());
    return list;
}

#include "moc_qmdiarea.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
