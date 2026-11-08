// MIT License - see LICENSE
#include <QtWidgets/qmainwindow.h>

#include "qwidget_p.h"

#include <QtWidgets/qdockwidget.h>
#include <QtWidgets/qlayout.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qmenubar.h>
#include <QtWidgets/qstatusbar.h>
#include <QtWidgets/qtoolbar.h>

#include <algorithm>

namespace {

QSize hintOf(const QWidget *w) {
    QSize s = w->sizeHint().expandedTo(w->minimumSizeHint());
    return QSize(std::max(0, s.width()), std::max(0, s.height()));
}

QSize minimumOf(const QWidget *w) {
    const QSize s = w->minimumSizeHint();
    return QSize(std::max(0, s.width()), std::max(0, s.height()));
}

bool isHorizontal(Qt::ToolBarArea area) { return area == Qt::TopToolBarArea || area == Qt::BottomToolBarArea; }

} // namespace

// Places the menu bar, the tool bars, the dock widgets, the status bar and the central widget of a main window.
class QMainWindowLayout : public QLayout {
public:
    explicit QMainWindowLayout(QMainWindow *window) : QLayout(window), m_window(window) {
        setContentsMargins(0, 0, 0, 0);
        setSpacing(0);
    }
    ~QMainWindowLayout() override {
        for (QLayoutItem *item : m_items)
            delete item;
    }

    void addManaged(QWidget *widget) {
        for (QLayoutItem *item : m_items)
            if (item->widget() == widget)
                return;
        m_items.push_back(new QWidgetItem(widget));
        invalidate();
    }

    void addItem(QLayoutItem *item) override { delete item; }
    QLayoutItem *itemAt(int index) const override {
        return index >= 0 && index < count() ? m_items[static_cast<std::size_t>(index)] : nullptr;
    }
    QLayoutItem *takeAt(int index) override {
        if (index < 0 || index >= count())
            return nullptr;
        QLayoutItem *item = m_items[static_cast<std::size_t>(index)];
        m_items.erase(m_items.begin() + index);
        return item;
    }
    int count() const override { return static_cast<int>(m_items.size()); }
    Qt::Orientations expandingDirections() const override { return Qt::Horizontal | Qt::Vertical; }

    QSize sizeHint() const override { return compute(false); }
    QSize minimumSize() const override { return compute(true); }
    void setGeometry(const QRect &rect) override {
        QLayout::setGeometry(rect);
        place(rect);
    }

private:
    using Line = std::vector<QToolBar *>;

    static bool shown(const QWidget *w) { return w && !w->isHidden() && !w->isWindow(); }

    // The lines of tool bars of `area`, in order.
    std::vector<Line> lines(Qt::ToolBarArea area) const {
        std::vector<Line> out;
        for (const auto &entry : m_window->m_toolBars) {
            auto *bar = static_cast<QToolBar *>(entry.bar.data());
            if (entry.area != area || !shown(bar))
                continue;
            if (out.empty() || (entry.lineBreak && !out.back().empty()))
                out.emplace_back();
            out.back().push_back(bar);
        }
        return out;
    }

    std::vector<QDockWidget *> docks(Qt::DockWidgetArea area) const {
        std::vector<QDockWidget *> out;
        for (const auto &entry : m_window->m_docks) {
            auto *dock = static_cast<QDockWidget *>(entry.dock.data());
            if (entry.area == area && shown(dock))
                out.push_back(dock);
        }
        return out;
    }

    // The thickness of each line, and the length of the longest one.
    static int thickness(const std::vector<Line> &all, bool horizontal, int *length = nullptr) {
        int total = 0;
        int longest = 0;
        for (const Line &line : all) {
            int thick = 0;
            int along = 0;
            for (const QToolBar *bar : line) {
                const QSize s = hintOf(bar);
                thick = std::max(thick, horizontal ? s.height() : s.width());
                along += horizontal ? s.width() : s.height();
            }
            total += thick;
            longest = std::max(longest, along);
        }
        if (length)
            *length = longest;
        return total;
    }

    QSize compute(bool minimum) const {
        m_window->prune();
        const auto sizeOf = [minimum](const QWidget *w) { return minimum ? minimumOf(w) : hintOf(w); };
        int topLength = 0;
        int bottomLength = 0;
        int leftLength = 0;
        int rightLength = 0;
        const int top = thickness(lines(Qt::TopToolBarArea), true, &topLength);
        const int bottom = thickness(lines(Qt::BottomToolBarArea), true, &bottomLength);
        const int left = thickness(lines(Qt::LeftToolBarArea), false, &leftLength);
        const int right = thickness(lines(Qt::RightToolBarArea), false, &rightLength);
        const auto dockRow = [&](Qt::DockWidgetArea area, bool horizontal, int *across) {
            int thick = 0;
            int sum = 0;
            for (const QDockWidget *dock : docks(area)) {
                const QSize s = sizeOf(dock);
                thick = std::max(thick, horizontal ? s.height() : s.width());
                sum += horizontal ? s.width() : s.height();
            }
            *across = sum;
            return thick;
        };
        int topDocksWidth = 0;
        int bottomDocksWidth = 0;
        int leftDocksHeight = 0;
        int rightDocksHeight = 0;
        const int topDocks = dockRow(Qt::TopDockWidgetArea, true, &topDocksWidth);
        const int bottomDocks = dockRow(Qt::BottomDockWidgetArea, true, &bottomDocksWidth);
        const int leftDocks = dockRow(Qt::LeftDockWidgetArea, false, &leftDocksHeight);
        const int rightDocks = dockRow(Qt::RightDockWidgetArea, false, &rightDocksHeight);
        const QSize central = shown(m_window->m_central) ? sizeOf(m_window->m_central) : QSize(0, 0);
        const QSize menu = shown(m_window->m_menu) ? hintOf(m_window->m_menu) : QSize(0, 0);
        const QSize status = shown(m_window->m_status) ? hintOf(m_window->m_status) : QSize(0, 0);

        int width = left + leftDocks + central.width() + rightDocks + right;
        width = std::max({width, topDocksWidth, bottomDocksWidth});
        if (!minimum)
            width = std::max({width, menu.width(), status.width(), topLength, bottomLength});
        int middle = std::max({central.height(), leftDocksHeight, rightDocksHeight});
        if (!minimum)
            middle = std::max({middle, leftLength, rightLength});
        const int height = menu.height() + top + topDocks + middle + bottomDocks + bottom + status.height();
        return QSize(width, height);
    }

    void place(const QRect &r) const {
        m_window->prune();
        int x0 = r.left();
        int x1 = r.right() + 1;
        int y0 = r.top();
        int y1 = r.bottom() + 1;
        if (QWidget *menu = m_window->m_menu; shown(menu)) {
            const int h = hintOf(menu).height();
            menu->setGeometry(x0, y0, x1 - x0, h);
            y0 += h;
        }
        if (QWidget *status = m_window->m_status; shown(status)) {
            const int h = hintOf(status).height();
            status->setGeometry(x0, y1 - h, x1 - x0, h);
            y1 -= h;
        }
        // Tool bars: lines across the top and the bottom, then columns at the left and the right.
        const auto placeLines = [&](Qt::ToolBarArea area) {
            const bool horizontal = isHorizontal(area);
            const std::vector<Line> all = lines(area);
            const int total = thickness(all, horizontal);
            int at = area == Qt::TopToolBarArea      ? y0
                     : area == Qt::BottomToolBarArea ? y1 - total
                     : area == Qt::LeftToolBarArea   ? x0
                                                     : x1 - total;
            for (const Line &line : all) {
                const int thick = thickness({line}, horizontal);
                int along = horizontal ? x0 : y0;
                for (QToolBar *bar : line) {
                    const QSize s = hintOf(bar);
                    if (horizontal) {
                        const int w = std::max(0, std::min(s.width(), x1 - along));
                        bar->setGeometry(along, at, w, thick);
                        along += w;
                    } else {
                        const int h = std::max(0, std::min(s.height(), y1 - along));
                        bar->setGeometry(at, along, thick, h);
                        along += h;
                    }
                }
                at += thick;
            }
            switch (area) {
            case Qt::TopToolBarArea:
                y0 += total;
                break;
            case Qt::BottomToolBarArea:
                y1 -= total;
                break;
            case Qt::LeftToolBarArea:
                x0 += total;
                break;
            default:
                x1 -= total;
                break;
            }
        };
        placeLines(Qt::TopToolBarArea);
        placeLines(Qt::BottomToolBarArea);
        placeLines(Qt::LeftToolBarArea);
        placeLines(Qt::RightToolBarArea);
        // Dock widgets: the top and bottom areas span the width, the left and right ones what is between them.
        const auto placeDocks = [&](Qt::DockWidgetArea area) {
            const std::vector<QDockWidget *> list = docks(area);
            if (list.empty())
                return;
            const bool horizontal = area == Qt::TopDockWidgetArea || area == Qt::BottomDockWidgetArea;
            int thick = 0;
            for (const QDockWidget *dock : list) {
                const QSize s = hintOf(dock);
                thick = std::max(thick, horizontal ? s.height() : s.width());
            }
            // A dock area takes at most half of the room that is left.
            thick = std::min(thick, horizontal ? (y1 - y0) / 2 : (x1 - x0) / 2);
            const int start = horizontal ? x0 : y0;
            const int room = horizontal ? x1 - x0 : y1 - y0;
            const int n = static_cast<int>(list.size());
            for (int i = 0; i < n; ++i) {
                const int a = start + room * i / n;
                const int b = start + room * (i + 1) / n;
                QDockWidget *dock = list[static_cast<std::size_t>(i)];
                switch (area) {
                case Qt::TopDockWidgetArea:
                    dock->setGeometry(a, y0, b - a, thick);
                    break;
                case Qt::BottomDockWidgetArea:
                    dock->setGeometry(a, y1 - thick, b - a, thick);
                    break;
                case Qt::LeftDockWidgetArea:
                    dock->setGeometry(x0, a, thick, b - a);
                    break;
                default:
                    dock->setGeometry(x1 - thick, a, thick, b - a);
                    break;
                }
            }
            switch (area) {
            case Qt::TopDockWidgetArea:
                y0 += thick;
                break;
            case Qt::BottomDockWidgetArea:
                y1 -= thick;
                break;
            case Qt::LeftDockWidgetArea:
                x0 += thick;
                break;
            default:
                x1 -= thick;
                break;
            }
        };
        placeDocks(Qt::TopDockWidgetArea);
        placeDocks(Qt::BottomDockWidgetArea);
        placeDocks(Qt::LeftDockWidgetArea);
        placeDocks(Qt::RightDockWidgetArea);
        if (QWidget *central = m_window->m_central; shown(central))
            central->setGeometry(x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0));
    }

    QMainWindow *m_window;
    std::vector<QLayoutItem *> m_items;
};

/*!
    \class QMainWindow
    \brief The QMainWindow class provides the main window of an application.

    A main window has a menu bar at the top, tool bars under it and at the sides, dock widgets around a central
    widget, and a status bar at the bottom. All are optional except the central widget, which fills the space that
    is left:

    \code
    QMainWindow window;
    window.setCentralWidget(new QPlainTextEdit);
    QMenu *file = window.menuBar()->addMenu("&File");
    file->addAction("&Quit", [] { qApp->quit(); });
    QToolBar *tools = window.addToolBar("Tools");
    tools->addAction("Bold");
    window.statusBar()->showMessage("Ready");
    window.show();
    \endcode

    menuBar() and statusBar() create the bars the first time they are called. Tool bars go in rows across the top
    and the bottom and in columns at the sides; addToolBarBreak() starts a new row. Dock widgets at the top and the
    bottom span the width of the window; the ones at the left and the right are between them, and several in one
    area share it.

    createPopupMenu() returns a menu with the toggle view actions of the dock widgets and tool bars.

    \differences
    The user cannot drag tool bars or dock widgets to other areas or resize the dock areas. Several dock widgets in
    one area share it equally, side by side or one above the other; there are no tabbed dock widgets, no
    \c splitDockWidget(), \c tabifyDockWidget(), \c setCorner(), \c saveState() or \c restoreState(), and no
    animation. The main window does not show createPopupMenu() on a right click.
    \enddifferences

    \sa QMenuBar, QToolBar, QDockWidget, QStatusBar
*/

/*!
    Constructs a main window with the given \a parent and window \a flags. A main window is always a window, even
    with a parent.
*/
QMainWindow::QMainWindow(QWidget *parent, Qt::WindowFlags flags)
    : QWidget(parent, flags | Qt::Window), m_layout(new QMainWindowLayout(this)) {}

/*!
    Destroys the main window and its bars and widgets.
*/
QMainWindow::~QMainWindow() = default;

/*!
    Returns the menu bar of the window, creating it the first time if the window has no menu widget.

    \sa setMenuBar(), menuWidget()
*/
QMenuBar *QMainWindow::menuBar() const {
    if (auto *bar = dynamic_cast<QMenuBar *>(m_menu.data()))
        return bar;
    auto *self = const_cast<QMainWindow *>(this);
    auto *bar = new QMenuBar(self);
    self->setMenuBar(bar);
    return bar;
}

/*!
    Makes \a menubar the menu bar of the window. The window takes it as a child and deletes the menu widget it had.

    \sa menuBar()
*/
void QMainWindow::setMenuBar(QMenuBar *menubar) { setMenuWidget(menubar); }

/*!
    \fn QWidget *QMainWindow::menuWidget() const

    Returns the widget at the top of the window, usually the menu bar, or \c nullptr.

    \sa setMenuWidget()
*/

/*!
    Makes \a menubar, which may be any widget, the widget at the top of the window. The window takes it as a child
    and deletes the one it had.

    \sa menuWidget(), setMenuBar()
*/
void QMainWindow::setMenuWidget(QWidget *menubar) {
    if (m_menu == menubar)
        return;
    if (m_menu) {
        unmanage(m_menu);
        delete m_menu.data();
    }
    m_menu = menubar;
    if (menubar)
        manage(menubar);
}

/*!
    Returns the status bar of the window, creating it the first time.

    \sa setStatusBar()
*/
QStatusBar *QMainWindow::statusBar() const {
    if (auto *bar = dynamic_cast<QStatusBar *>(m_status.data()))
        return bar;
    auto *self = const_cast<QMainWindow *>(this);
    auto *bar = new QStatusBar(self);
    self->setStatusBar(bar);
    return bar;
}

/*!
    Makes \a statusbar the status bar of the window, or removes the status bar with \c nullptr. The window takes it
    as a child and deletes the one it had.

    \sa statusBar()
*/
void QMainWindow::setStatusBar(QStatusBar *statusbar) {
    if (m_status == statusbar)
        return;
    if (m_status) {
        unmanage(m_status);
        delete m_status.data();
    }
    m_status = statusbar;
    if (statusbar)
        manage(statusbar);
}

/*!
    \fn QWidget *QMainWindow::centralWidget() const

    Returns the central widget of the window, or \c nullptr.

    \sa setCentralWidget()
*/

/*!
    Makes \a widget the central widget, which fills the space that the bars and dock widgets leave. The window takes
    it as a child and deletes the central widget it had.

    \sa centralWidget(), takeCentralWidget()
*/
void QMainWindow::setCentralWidget(QWidget *widget) {
    if (m_central == widget)
        return;
    if (m_central) {
        unmanage(m_central);
        delete m_central.data();
    }
    m_central = widget;
    if (widget)
        manage(widget);
}

/*!
    Removes the central widget from the window without deleting it and returns it; the widget no longer has a
    parent, and the caller owns it.

    \sa setCentralWidget()
*/
QWidget *QMainWindow::takeCentralWidget() {
    QWidget *widget = m_central;
    if (!widget)
        return nullptr;
    m_central = nullptr;
    unmanage(widget);
    widget->hide();
    widget->setParent(nullptr);
    return widget;
}

/*!
    Adds \a toolbar to the \a area of the window, after the tool bars there. A tool bar that is in the window already
    moves.

    \sa removeToolBar(), toolBarArea()
*/
void QMainWindow::addToolBar(Qt::ToolBarArea area, QToolBar *toolbar) {
    if (!toolbar)
        return;
    if (area != Qt::TopToolBarArea && area != Qt::BottomToolBarArea && area != Qt::LeftToolBarArea &&
        area != Qt::RightToolBarArea)
        area = Qt::TopToolBarArea;
    prune();
    m_toolBars.erase(std::remove_if(m_toolBars.begin(), m_toolBars.end(),
                                    [toolbar](const ToolBarEntry &e) { return e.bar == toolbar; }),
                     m_toolBars.end());
    m_toolBars.push_back({toolbar, area, m_pendingBreaks.testFlag(area)});
    m_pendingBreaks.setFlag(area, false);
    toolbar->setOrientation(isHorizontal(area) ? Qt::Horizontal : Qt::Vertical);
    toolbar->setToolButtonStyle(m_toolButtonStyle);
    manage(toolbar);
    relayout();
}

/*!
    \overload

    Adds \a toolbar at the top of the window.
*/
void QMainWindow::addToolBar(QToolBar *toolbar) { addToolBar(Qt::TopToolBarArea, toolbar); }

/*!
    \overload

    Creates a tool bar with the given window \a title, adds it at the top of the window and returns it.
*/
QToolBar *QMainWindow::addToolBar(const QString &title) {
    auto *toolbar = new QToolBar(title, this);
    addToolBar(Qt::TopToolBarArea, toolbar);
    return toolbar;
}

/*!
    Inserts \a toolbar before the tool bar \a before, in its area.

    \sa addToolBar()
*/
void QMainWindow::insertToolBar(QToolBar *before, QToolBar *toolbar) {
    const Qt::ToolBarArea area = toolBarArea(before);
    if (!toolbar || area == Qt::NoToolBarArea) {
        addToolBar(toolbar);
        return;
    }
    m_toolBars.erase(std::remove_if(m_toolBars.begin(), m_toolBars.end(),
                                    [toolbar](const ToolBarEntry &e) { return e.bar == toolbar; }),
                     m_toolBars.end());
    const auto at =
        std::find_if(m_toolBars.begin(), m_toolBars.end(), [before](const ToolBarEntry &e) { return e.bar == before; });
    m_toolBars.insert(at, {toolbar, area, false});
    toolbar->setOrientation(isHorizontal(area) ? Qt::Horizontal : Qt::Vertical);
    toolbar->setToolButtonStyle(m_toolButtonStyle);
    manage(toolbar);
    relayout();
}

/*!
    Makes the next tool bar added to \a area start a new line.

    \sa insertToolBarBreak(), toolBarBreak()
*/
void QMainWindow::addToolBarBreak(Qt::ToolBarArea area) { m_pendingBreaks |= area; }

/*!
    Makes the tool bar \a before start a new line of its area.

    \sa removeToolBarBreak()
*/
void QMainWindow::insertToolBarBreak(QToolBar *before) {
    for (ToolBarEntry &entry : m_toolBars)
        if (entry.bar == before)
            entry.lineBreak = true;
    relayout();
}

/*!
    Lets the tool bar \a before go on the line of the tool bar before it.

    \sa insertToolBarBreak()
*/
void QMainWindow::removeToolBarBreak(QToolBar *before) {
    for (ToolBarEntry &entry : m_toolBars)
        if (entry.bar == before)
            entry.lineBreak = false;
    relayout();
}

/*!
    Returns whether a new line of tool bars starts with \a toolbar.

    \sa insertToolBarBreak()
*/
bool QMainWindow::toolBarBreak(QToolBar *toolbar) const {
    for (const ToolBarEntry &entry : m_toolBars)
        if (toolbar && entry.bar == toolbar)
            return entry.lineBreak;
    return false;
}

/*!
    Removes \a toolbar from the window and hides it. The tool bar is not deleted.

    \sa addToolBar()
*/
void QMainWindow::removeToolBar(QToolBar *toolbar) {
    const auto it = std::find_if(m_toolBars.begin(), m_toolBars.end(),
                                 [toolbar](const ToolBarEntry &e) { return e.bar == toolbar; });
    if (it == m_toolBars.end())
        return;
    m_toolBars.erase(it);
    unmanage(toolbar);
    toolbar->hide();
    relayout();
}

/*!
    Returns the area that \a toolbar is in, or Qt::NoToolBarArea if it is not in the window.
*/
Qt::ToolBarArea QMainWindow::toolBarArea(const QToolBar *toolbar) const {
    for (const ToolBarEntry &entry : m_toolBars)
        if (toolbar && entry.bar == toolbar)
            return entry.area;
    return Qt::NoToolBarArea;
}

/*!
    Adds \a dockwidget to the \a area of the window, after the dock widgets there. A floating dock widget is docked,
    and one that is in the window already moves. Emits QDockWidget::dockLocationChanged().

    \sa removeDockWidget(), dockWidgetArea()
*/
void QMainWindow::addDockWidget(Qt::DockWidgetArea area, QDockWidget *dockwidget) {
    if (!dockwidget)
        return;
    if (area != Qt::LeftDockWidgetArea && area != Qt::RightDockWidgetArea && area != Qt::TopDockWidgetArea &&
        area != Qt::BottomDockWidgetArea)
        area = Qt::LeftDockWidgetArea;
    prune();
    m_docks.erase(std::remove_if(m_docks.begin(), m_docks.end(),
                                 [dockwidget](const DockEntry &e) { return e.dock == dockwidget; }),
                  m_docks.end());
    m_docks.push_back({dockwidget, area});
    if (dockwidget->parentWidget() != this)
        dockwidget->setParent(this);
    if (dockwidget->isFloating())
        dockwidget->setFloating(false);
    manage(dockwidget);
    relayout();
    emit dockwidget->dockLocationChanged(area);
}

/*!
    \overload

    Adds \a dockwidget to \a area; \a orientation is accepted for compatibility: the dock widgets of the left and
    right areas are always one above the other, and those of the top and bottom areas side by side.
*/
void QMainWindow::addDockWidget(Qt::DockWidgetArea area, QDockWidget *dockwidget, Qt::Orientation orientation) {
    (void)orientation;
    addDockWidget(area, dockwidget);
}

/*!
    Removes \a dockwidget from the window and hides it. The dock widget is not deleted.

    \sa addDockWidget()
*/
void QMainWindow::removeDockWidget(QDockWidget *dockwidget) {
    const auto it =
        std::find_if(m_docks.begin(), m_docks.end(), [dockwidget](const DockEntry &e) { return e.dock == dockwidget; });
    if (it == m_docks.end())
        return;
    m_docks.erase(it);
    unmanage(dockwidget);
    dockwidget->hide();
    relayout();
}

/*!
    Returns the area that \a dockwidget is in, or Qt::NoDockWidgetArea if it is not in the window.
*/
Qt::DockWidgetArea QMainWindow::dockWidgetArea(QDockWidget *dockwidget) const {
    for (const DockEntry &entry : m_docks)
        if (dockwidget && entry.dock == dockwidget)
            return entry.area;
    return Qt::NoDockWidgetArea;
}

/*!
    Shows \a dockwidget again in its area if it is in the window, and returns whether it is.
*/
bool QMainWindow::restoreDockWidget(QDockWidget *dockwidget) {
    if (dockWidgetArea(dockwidget) == Qt::NoDockWidgetArea)
        return false;
    dockwidget->show();
    relayout();
    return true;
}

/*!
    \fn Qt::ToolButtonStyle QMainWindow::toolButtonStyle() const

    Returns the style of the tool buttons of the tool bars of the window, Qt::ToolButtonIconOnly by default.

    \sa setToolButtonStyle()
*/

/*!
    Sets the style of the tool buttons of the tool bars of the window to \a toolButtonStyle.

    \sa toolButtonStyle()
*/
void QMainWindow::setToolButtonStyle(Qt::ToolButtonStyle toolButtonStyle) {
    if (m_toolButtonStyle == toolButtonStyle)
        return;
    m_toolButtonStyle = toolButtonStyle;
    prune();
    for (const ToolBarEntry &entry : m_toolBars)
        static_cast<QToolBar *>(entry.bar.data())->setToolButtonStyle(toolButtonStyle);
    emit toolButtonStyleChanged(toolButtonStyle);
}

/*!
    Returns a new menu with the toggle view actions of the dock widgets and, after a separator, of the tool bars of
    the window, or \c nullptr if it has neither. The caller owns the menu.

    \sa QToolBar::toggleViewAction(), QDockWidget::toggleViewAction()
*/
QMenu *QMainWindow::createPopupMenu() {
    prune();
    if (m_docks.empty() && m_toolBars.empty())
        return nullptr;
    auto *menu = new QMenu(this);
    for (const DockEntry &entry : m_docks)
        menu->addAction(static_cast<QDockWidget *>(entry.dock.data())->toggleViewAction());
    if (!m_docks.empty() && !m_toolBars.empty())
        menu->addSeparator();
    for (const ToolBarEntry &entry : m_toolBars)
        menu->addAction(static_cast<QToolBar *>(entry.bar.data())->toggleViewAction());
    return menu;
}

/*!
    \reimp
*/
QSize QMainWindow::sizeHint() const { return m_layout->sizeHint(); }

/*!
    \reimp
*/
QSize QMainWindow::minimumSizeHint() const { return m_layout->minimumSize(); }

void QMainWindow::manage(QWidget *widget) {
    if (widget->parentWidget() != this || widget->isWindow())
        widget->setParent(this);
    m_layout->addManaged(widget);
    if (!QtPrivate::WidgetAccess::data(widget).hiddenExplicitly)
        widget->show();
}

void QMainWindow::unmanage(QWidget *widget) { m_layout->removeWidget(widget); }

void QMainWindow::relayout() {
    m_layout->invalidate();
    if (isVisible())
        m_layout->activate();
}

// Forgets the tool bars and dock widgets that were deleted.
void QMainWindow::prune() const {
    m_toolBars.erase(std::remove_if(m_toolBars.begin(), m_toolBars.end(), [](const ToolBarEntry &e) { return !e.bar; }),
                     m_toolBars.end());
    m_docks.erase(std::remove_if(m_docks.begin(), m_docks.end(), [](const DockEntry &e) { return !e.dock; }),
                  m_docks.end());
}

#include "moc_qmainwindow.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
