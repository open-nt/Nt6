// MIT License - see LICENSE
#include <QtWidgets/qmainwindow.h>
#include <QtWidgets/qmaterialstyle.h>
#include <QtWidgets/qstyle.h>

#include "qwidget_p.h"

#include <QtWidgets/qabstractbutton.h>
#include <QtWidgets/qdockwidget.h>
#include <QtWidgets/qlayout.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qmenubar.h>
#include <QtCore/qdatastream.h>
#include <QtWidgets/qstatusbar.h>
#include <QtWidgets/qtabbar.h>
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

    // The dock widgets that take a place in `area`: of the ones tabbed together, only the current one.
    std::vector<QDockWidget *> docks(Qt::DockWidgetArea area) const {
        std::vector<QDockWidget *> out;
        std::vector<int> groups;
        for (const auto &entry : m_window->m_docks) {
            auto *dock = static_cast<QDockWidget *>(entry.dock.data());
            if (entry.area != area || !shown(dock))
                continue;
            if (entry.group != 0) {
                if (std::find(groups.begin(), groups.end(), entry.group) != groups.end())
                    continue;
                const QMainWindow::DockGroup *group = m_window->groupOf(entry.group);
                if (group && group->current && group->current != dock && shown(group->current))
                    continue;
                groups.push_back(entry.group);
            }
            out.push_back(dock);
        }
        return out;
    }

    // Places `dock` at `rect`, with the tabs of its group under it and the other dock widgets of the group hidden.
    void placeDock(QDockWidget *dock, const QRect &rect) const {
        const QMainWindow::DockEntry *entry = m_window->entryOf(dock);
        const QMainWindow::DockGroup *group = entry ? m_window->groupOf(entry->group) : nullptr;
        if (!group || !group->bar) {
            dock->setGeometry(rect);
            return;
        }
        QTabBar *bar = group->bar;
        const int thick = std::min(bar->sizeHint().height(), rect.height() / 2);
        const bool above = bar->shape() == QTabBar::RoundedNorth || bar->shape() == QTabBar::TriangularNorth;
        bar->setGeometry(rect.left(), above ? rect.top() : rect.bottom() + 1 - thick, rect.width(), thick);
        bar->show();
        bar->raise();
        dock->setGeometry(above ? rect.adjusted(0, thick, 0, 0) : rect.adjusted(0, 0, 0, -thick));
        for (const auto &other : m_window->m_docks)
            if (other.group == entry->group && other.dock && other.dock != dock)
                other.dock->setGeometry(QRect(rect.topLeft(), QSize(0, 0)));
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
        const QSize status =
            shown(m_window->m_status) && !m_window->floatingStatus() ? hintOf(m_window->m_status) : QSize(0, 0);

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
        QStatusBar *snackbar = m_window->floatingStatus();
        if (QWidget *status = m_window->m_status; shown(status) && !snackbar) {
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
                // What a line leaves free goes to the bars whose size policy asks to expand along it, as in Qt.
                const auto expands = [horizontal](const QToolBar *bar) {
                    const QSizePolicy policy = bar->sizePolicy();
                    const auto grows = horizontal ? policy.horizontalPolicy() : policy.verticalPolicy();
                    return (static_cast<int>(grows) & static_cast<int>(QSizePolicy::ExpandFlag)) != 0;
                };
                int spare = horizontal ? x1 - x0 : y1 - y0;
                int expanding = 0;
                for (const QToolBar *bar : line) {
                    spare -= horizontal ? hintOf(bar).width() : hintOf(bar).height();
                    expanding += expands(bar) ? 1 : 0;
                }
                spare = std::max(0, spare);
                for (QToolBar *bar : line) {
                    QSize s = hintOf(bar);
                    if (expanding > 0 && expands(bar)) {
                        const int share = spare / expanding;
                        spare -= share;
                        --expanding;
                        if (horizontal)
                            s.rwidth() += share;
                        else
                            s.rheight() += share;
                    }
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
                QSize s = hintOf(dock);
                // A size that resizeDocks() set takes the place of the hint.
                for (const auto &entry : m_window->m_docks) {
                    if (entry.dock == dock) {
                        if (entry.width >= 0)
                            s.setWidth(entry.width);
                        if (entry.height >= 0)
                            s.setHeight(entry.height);
                    }
                }
                // The area is no thicker than the dock may be, so that the central widget gets the rest.
                s = s.boundedTo(dock->maximumSize()).expandedTo(dock->minimumSize());
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
                    placeDock(dock, QRect(a, y0, b - a, thick));
                    break;
                case Qt::BottomDockWidgetArea:
                    placeDock(dock, QRect(a, y1 - thick, b - a, thick));
                    break;
                case Qt::LeftDockWidgetArea:
                    placeDock(dock, QRect(x0, a, thick, b - a));
                    break;
                default:
                    placeDock(dock, QRect(x1 - thick, a, thick, b - a));
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
        if (snackbar) // above what is at the bottom
            m_window->placeSnackbar(snackbar, QRect(x0, r.top(), std::max(0, x1 - x0), std::max(0, y1 - r.top())));
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

    tabifyDockWidget() puts dock widgets in one place with tabs to choose between them, and saveState() and
    restoreState() keep where the tool bars and dock widgets are between runs of the application:

    \code
    settings.setValue("state", window.saveState());
    // next time, after the bars and dock widgets are made:
    window.restoreState(settings.value("state").toByteArray());
    \endcode

    A main window with a menu bar, a tool bar, a dock widget, a central text edit and a status bar.

    \image widgets/qmainwindow.png QMainWindow in the light color scheme

    \differences
    The user cannot drag tool bars or dock widgets to other areas or resize the dock areas. Several dock widgets in
    one area share it equally, side by side or one above the other, whatever the orientation of splitDockWidget().
    The corners (setCorner()) are kept but do not change the layout, and there is no animation. The tabs of tabbed
    dock widgets are below them, or above for QTabWidget::North. The state that saveState() writes is Nt's own and
    is not read by Qt. The main window does not show createPopupMenu() on a right click.
    \enddifferences

    \sa QMenuBar, QToolBar, QDockWidget, QStatusBar
*/

/*!
    \enum QMainWindow::DockOption
    \since 6.0

    Options for the dock widgets of a main window, combined in the DockOptions flags. Nt keeps them: dock widgets
    are not dragged or animated.

    \value AnimatedDocks Dock widgets move with an animation.
    \value AllowNestedDocks Dock areas may be split in both directions.
    \value AllowTabbedDocks Dock widgets may be dropped onto each other to be tabbed.
    \value ForceTabbedDocks Dock widgets in an area are always tabbed.
    \value VerticalTabs The tabs of tabbed dock widgets run down at the side.
    \value GroupedDragging A title bar drags all the dock widgets tabbed with it.
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
    if (m_iconSize.isValid())
        toolbar->setIconSize(m_iconSize);
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
    if (m_iconSize.isValid())
        toolbar->setIconSize(m_iconSize);
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
    leaveGroup(dockwidget);
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
    leaveGroup(dockwidget);
    m_docks.erase(std::find_if(m_docks.begin(), m_docks.end(),
                               [dockwidget](const DockEntry &e) { return e.dock == dockwidget; }));
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
    \since 6.0

    Returns the size of the icons of the tool bars of the window: what setIconSize() set, or the tool bar icon size
    of the style.
*/
QSize QMainWindow::iconSize() const {
    if (m_iconSize.isValid())
        return m_iconSize;
    const int extent = style()->pixelMetric(QStyle::PM_ToolBarIconSize, nullptr, this);
    return QSize(extent > 0 ? extent : 24, extent > 0 ? extent : 24);
}

/*!
    \since 6.0

    Sets the size of the icons of the tool bars of the window to \a iconSize, for those it has and those added later.
*/
void QMainWindow::setIconSize(const QSize &iconSize) {
    if (m_iconSize == iconSize)
        return;
    m_iconSize = iconSize;
    prune();
    for (const ToolBarEntry &entry : m_toolBars)
        static_cast<QToolBar *>(entry.bar.data())->setIconSize(iconSize);
    emit iconSizeChanged(this->iconSize());
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

QStatusBar *QMainWindow::floatingStatus() const {
    auto *status = qobject_cast<QStatusBar *>(m_status.data());
    if (!status || status->isHidden() || !qobject_cast<const QMaterialStyle *>(status->style()) ||
        !QMaterialStyle::isSnackbar(status))
        return nullptr;
    return status;
}

// Puts a snackbar over the bottom of `area`, above a navigation bar that is there and beside a navigation rail; with no
// message it has no size.
void QMainWindow::placeSnackbar(QStatusBar *status, const QRect &area) const {
    QRect room = area;
    constexpr int margin = 16;
    constexpr int widest = 600;
    int bottom = room.bottom() + 1;
    if (QWidget *central = m_central) {
        QList<QTabBar *> bars = central->findChildren<QTabBar *>();
        if (auto *self = qobject_cast<QTabBar *>(central))
            bars.append(self);
        for (const QTabBar *bar : std::as_const(bars)) {
            if (QMaterialStyle::isNavigationRail(bar) && !bar->isHidden()) {
                // The snackbar keeps to the pages, at whichever side of them the rail is.
                const int at = bar->mapTo(this, QPoint(0, 0)).x();
                if (at <= room.left() + 4)
                    room.setLeft(std::max(room.left(), at + bar->width()));
                else if (at + bar->width() >= room.left() + room.width() - 4)
                    room.setRight(std::min(room.right(), at - 1));
                continue;
            }
            if (!QMaterialStyle::isNavigationBar(bar) || bar->isHidden())
                continue;
            const int top = bar->mapTo(this, QPoint(0, 0)).y();
            if (top + bar->height() >= bottom - 4)
                bottom = top;
        }
    }
    const int width = std::max(0, std::min(widest, room.width() - 2 * margin));
    // Material puts a snackbar above a floating action button that is where it would go, so that both can be reached.
    const int left = room.left() + (room.width() - width) / 2;
    constexpr int reach = 96; // how far over the bottom a button counts as being at it
    int above = bottom;
    if (const QWidget *central = m_central) {
        const QList<QAbstractButton *> buttons = central->findChildren<QAbstractButton *>();
        for (const QAbstractButton *button : buttons) {
            if (!QMaterialStyle::isFloatingActionButton(button) || !button->isVisibleTo(this))
                continue;
            const QRect at(button->mapTo(this, QPoint(0, 0)), button->size());
            if (at.right() >= left && at.left() < left + width && at.bottom() >= bottom - reach && at.top() < above)
                above = at.top();
        }
    }
    bottom = above;
    const int height = status->currentMessage().isEmpty() ? 0 : QMaterialStyle::snackbarHeight(status, width) + 2;
    status->setGeometry(left, bottom - margin - height, width, height);
    status->raise();
}

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
    // A group of tabbed dock widgets that has fewer than two left is dissolved.
    for (auto it = m_groups.begin(); it != m_groups.end();) {
        const int id = it->id;
        const auto members = std::count_if(m_docks.begin(), m_docks.end(), [id](const DockEntry &e) { return e.group == id; });
        if (members >= 2) {
            ++it;
            continue;
        }
        for (DockEntry &e : m_docks)
            if (e.group == id)
                e.group = 0;
        if (it->bar)
            it->bar->deleteLater();
        if (it->bar)
            it->bar->hide();
        it = m_groups.erase(it);
    }
}

QMainWindow::DockEntry *QMainWindow::entryOf(const QWidget *dock) const {
    for (DockEntry &e : m_docks)
        if (dock && e.dock == dock)
            return &e;
    return nullptr;
}

QMainWindow::DockGroup *QMainWindow::groupOf(int id) const {
    for (DockGroup &g : m_groups)
        if (id != 0 && g.id == id)
            return &g;
    return nullptr;
}

// Takes `dock` out of the tabs it is in; a group left with one dock widget is dissolved.
void QMainWindow::leaveGroup(const QWidget *dock) const {
    DockEntry *entry = entryOf(dock);
    if (!entry || entry->group == 0)
        return;
    const int id = entry->group;
    entry->group = 0;
    if (DockGroup *group = groupOf(id); group && group->current == dock)
        group->current = nullptr;
    prune();
    if (groupOf(id))
        const_cast<QMainWindow *>(this)->syncGroup(id); // NOLINT(cppcoreguidelines-pro-type-const-cast): the tabs
}

// Makes the tabs of the group `id` those of its dock widgets, in their order, with the current one selected.
void QMainWindow::syncGroup(int id) {
    DockGroup *group = groupOf(id);
    if (!group)
        return;
    std::vector<QDockWidget *> members;
    for (const DockEntry &e : m_docks)
        if (e.group == id && e.dock)
            members.push_back(static_cast<QDockWidget *>(e.dock.data()));
    if (!group->current || std::find(members.begin(), members.end(), group->current.data()) == members.end())
        group->current = members.empty() ? nullptr : members.back();
    if (!group->bar) {
        auto *bar = new QTabBar(this);
        bar->setObjectName(QStringLiteral("qt_dockwidget_tabbar"));
        bar->setDrawBase(true);
        bar->setExpanding(false);
        group->bar = bar;
        QObject::connect(bar, &QTabBar::currentChanged, this, [this, id](int index) {
            DockGroup *g = groupOf(id);
            if (!g || index < 0)
                return;
            int n = 0;
            for (const DockEntry &e : m_docks) {
                if (e.group != id || !e.dock)
                    continue;
                if (n++ == index) {
                    if (g->current == e.dock)
                        return;
                    g->current = e.dock;
                    relayout();
                    emit tabifiedDockWidgetActivated(static_cast<QDockWidget *>(e.dock.data()));
                    return;
                }
            }
        });
    }
    QTabBar *bar = group->bar;
    const QSignalBlocker blocker(bar);
    while (bar->count() > 0)
        bar->removeTab(bar->count() - 1);
    int current = 0;
    for (std::size_t i = 0; i < members.size(); ++i) {
        bar->addTab(members[i]->windowTitle());
        if (members[i] == group->current)
            current = static_cast<int>(i);
    }
    bar->setCurrentIndex(current);
    bar->setDocumentMode(m_documentMode);
    const Qt::DockWidgetArea area = members.empty() ? Qt::LeftDockWidgetArea : dockWidgetArea(members.front());
    const QTabWidget::TabPosition position = tabPosition(area);
    bar->setShape(static_cast<QTabBar::Shape>((m_tabShape == QTabWidget::Triangular ? 4 : 0) +
                                              (position == QTabWidget::North ? 0 : 1)));
    bar->show();
}

/*!
    \since 6.0

    Puts \a second in the place of \a first, with tabs under them to choose which of them is shown, and makes
    \a second the one shown. \a second moves to the area of \a first.

    \sa tabifiedDockWidgets(), splitDockWidget()
*/
void QMainWindow::tabifyDockWidget(QDockWidget *first, QDockWidget *second) {
    if (!first || !second || first == second)
        return;
    const Qt::DockWidgetArea area = dockWidgetArea(first);
    if (area == Qt::NoDockWidgetArea)
        return;
    if (dockWidgetArea(second) == Qt::NoDockWidgetArea)
        addDockWidget(area, second);
    leaveGroup(second);
    DockEntry *firstEntry = entryOf(first);
    if (firstEntry->group == 0) {
        firstEntry->group = m_nextGroup++;
        m_groups.push_back({firstEntry->group, nullptr, nullptr});
    }
    const int id = firstEntry->group;
    // `second` goes after the last dock widget of the group.
    auto from = std::find_if(m_docks.begin(), m_docks.end(), [second](const DockEntry &e) { return e.dock == second; });
    DockEntry moved = *from;
    m_docks.erase(from);
    moved.area = area;
    moved.group = id;
    auto last = std::find_if(m_docks.rbegin(), m_docks.rend(), [id](const DockEntry &e) { return e.group == id; });
    m_docks.insert(last.base(), moved);
    groupOf(id)->current = second;
    syncGroup(id);
    relayout();
    emit second->dockLocationChanged(area);
}

/*!
    \since 6.0

    Returns the dock widgets that are tabbed with \a dockwidget, without it.

    \sa tabifyDockWidget()
*/
QList<QDockWidget *> QMainWindow::tabifiedDockWidgets(QDockWidget *dockwidget) const {
    QList<QDockWidget *> out;
    const DockEntry *entry = entryOf(dockwidget);
    if (!entry || entry->group == 0)
        return out;
    for (const DockEntry &e : m_docks)
        if (e.group == entry->group && e.dock && e.dock != dockwidget)
            out.append(static_cast<QDockWidget *>(e.dock.data()));
    return out;
}

/*!
    \since 6.0

    Moves \a dockwidget to the area of \a after, right after it, out of any tabs. The dock widgets of an area
    share it in one direction, so \a orientation is accepted for compatibility.

    \sa tabifyDockWidget()
*/
void QMainWindow::splitDockWidget(QDockWidget *after, QDockWidget *dockwidget, Qt::Orientation orientation) {
    (void)orientation;
    if (!after || !dockwidget || after == dockwidget)
        return;
    const Qt::DockWidgetArea area = dockWidgetArea(after);
    if (area == Qt::NoDockWidgetArea)
        return;
    if (dockWidgetArea(dockwidget) == Qt::NoDockWidgetArea)
        addDockWidget(area, dockwidget);
    leaveGroup(dockwidget);
    auto from =
        std::find_if(m_docks.begin(), m_docks.end(), [dockwidget](const DockEntry &e) { return e.dock == dockwidget; });
    DockEntry moved = *from;
    m_docks.erase(from);
    moved.area = area;
    auto at = std::find_if(m_docks.begin(), m_docks.end(), [after](const DockEntry &e) { return e.dock == after; });
    // After the whole group of `after`, if it is tabbed.
    const int group = at->group;
    ++at;
    while (group != 0 && at != m_docks.end() && at->group == group)
        ++at;
    m_docks.insert(at, moved);
    relayout();
    emit dockwidget->dockLocationChanged(area);
}

namespace {
constexpr quint32 StateMagic = 0x4E744D57; // "NtMW"
} // namespace

/*!
    \since 6.0

    Returns the state of the tool bars and dock widgets of the window: their areas, order, sizes, visibility and
    tabs, under \a version, for restoreState(). The tool bars and dock widgets are known by their object names,
    which must be set and unique.

    \sa restoreState(), QWidget::saveGeometry()
*/
QByteArray QMainWindow::saveState(int version) const {
    prune();
    QByteArray state;
    QDataStream out(&state, QIODeviceBase::WriteOnly);
    out << StateMagic << static_cast<qint32>(version);
    out << static_cast<quint32>(m_toolBars.size());
    for (const ToolBarEntry &e : m_toolBars)
        out << e.bar->objectName() << static_cast<qint32>(e.area) << e.lineBreak << e.bar->isHidden();
    out << static_cast<quint32>(m_docks.size());
    for (const DockEntry &e : m_docks) {
        const DockGroup *group = groupOf(e.group);
        const bool current = group && group->current == e.dock;
        out << e.dock->objectName() << static_cast<qint32>(e.area) << static_cast<qint32>(e.width)
            << static_cast<qint32>(e.height) << e.dock->isHidden() << static_cast<qint32>(e.group) << current;
    }
    return state;
}

/*!
    \since 6.0

    Restores the state of the tool bars and dock widgets that saveState() returned as \a state under \a version.
    Returns \c false, changing nothing, if \a state is not such a state or has another version. Tool bars and
    dock widgets that are not in \a state keep their place, after the others.
*/
bool QMainWindow::restoreState(const QByteArray &state, int version) {
    QDataStream in(state);
    quint32 magic = 0;
    qint32 savedVersion = 0;
    in >> magic >> savedVersion;
    if (in.status() != QDataStream::Ok || magic != StateMagic || savedVersion != version)
        return false;
    struct SavedBar {
        QString name;
        qint32 area = 0;
        bool lineBreak = false;
        bool hidden = false;
    };
    struct SavedDock {
        QString name;
        qint32 area = 0;
        qint32 width = -1;
        qint32 height = -1;
        bool hidden = false;
        qint32 group = 0;
        bool current = false;
    };
    quint32 bars = 0;
    in >> bars;
    std::vector<SavedBar> savedBars;
    for (quint32 i = 0; i < bars && in.status() == QDataStream::Ok; ++i) {
        SavedBar b;
        in >> b.name >> b.area >> b.lineBreak >> b.hidden;
        savedBars.push_back(b);
    }
    quint32 docks = 0;
    in >> docks;
    std::vector<SavedDock> savedDocks;
    for (quint32 i = 0; i < docks && in.status() == QDataStream::Ok; ++i) {
        SavedDock d;
        in >> d.name >> d.area >> d.width >> d.height >> d.hidden >> d.group >> d.current;
        savedDocks.push_back(d);
    }
    if (in.status() != QDataStream::Ok)
        return false;
    prune();
    // Tool bars: those in the state first, in its order.
    std::vector<ToolBarEntry> toolBars;
    for (const SavedBar &b : savedBars) {
        auto it = std::find_if(m_toolBars.begin(), m_toolBars.end(), [&](const ToolBarEntry &e) {
            return !b.name.isEmpty() && e.bar->objectName() == b.name;
        });
        if (it == m_toolBars.end())
            continue;
        ToolBarEntry e = *it;
        m_toolBars.erase(it);
        e.area = static_cast<Qt::ToolBarArea>(b.area);
        e.lineBreak = b.lineBreak;
        e.bar->setVisible(!b.hidden);
        toolBars.push_back(e);
    }
    toolBars.insert(toolBars.end(), m_toolBars.begin(), m_toolBars.end());
    m_toolBars = toolBars;
    // Dock widgets, with their tabs.
    for (const DockEntry &e : m_docks)
        leaveGroup(e.dock);
    std::vector<DockEntry> dockEntries;
    std::vector<std::pair<qint32, int>> groupIds; // saved id, new id
    for (const SavedDock &d : savedDocks) {
        auto it = std::find_if(m_docks.begin(), m_docks.end(), [&](const DockEntry &e) {
            return !d.name.isEmpty() && e.dock->objectName() == d.name;
        });
        if (it == m_docks.end())
            continue;
        DockEntry e = *it;
        m_docks.erase(it);
        e.area = static_cast<Qt::DockWidgetArea>(d.area);
        e.width = d.width;
        e.height = d.height;
        e.group = 0;
        if (d.group != 0) {
            auto known = std::find_if(groupIds.begin(), groupIds.end(), [&](const auto &p) { return p.first == d.group; });
            if (known == groupIds.end()) {
                groupIds.emplace_back(d.group, m_nextGroup++);
                m_groups.push_back({groupIds.back().second, nullptr, nullptr});
                known = groupIds.end() - 1;
            }
            e.group = known->second;
            if (d.current)
                groupOf(e.group)->current = e.dock;
        }
        e.dock->setVisible(!d.hidden);
        dockEntries.push_back(e);
    }
    dockEntries.insert(dockEntries.end(), m_docks.begin(), m_docks.end());
    m_docks = dockEntries;
    prune();
    for (const auto &[saved, id] : groupIds)
        syncGroup(id);
    relayout();
    return true;
}

/*!
    \since 6.0

    Sets the options of the dock widgets to \a options. Nt keeps them: dock widgets are always tabbed with
    tabifyDockWidget() and never animated.

    \sa dockOptions()
*/
void QMainWindow::setDockOptions(DockOptions options) { m_dockOptions = options; }

/*!
    \fn QMainWindow::DockOptions QMainWindow::dockOptions() const
    \since 6.0

    Returns the options of the dock widgets, AnimatedDocks | AllowTabbedDocks by default.
*/

/*!
    \fn bool QMainWindow::isAnimated() const
    \since 6.0

    Returns \c true if the AnimatedDocks option is set.
*/

/*!
    \since 6.0

    Sets the AnimatedDocks option if \a enabled is \c true, and clears it otherwise.
*/
void QMainWindow::setAnimated(bool enabled) { m_dockOptions.setFlag(AnimatedDocks, enabled); }

/*!
    \fn bool QMainWindow::isDockNestingEnabled() const
    \since 6.0

    Returns \c true if the AllowNestedDocks option is set.
*/

/*!
    \since 6.0

    Sets the AllowNestedDocks option if \a enabled is \c true, and clears it otherwise.
*/
void QMainWindow::setDockNestingEnabled(bool enabled) { m_dockOptions.setFlag(AllowNestedDocks, enabled); }

/*!
    \since 6.0

    Returns the dock area that takes the \a corner of the window.
*/
Qt::DockWidgetArea QMainWindow::corner(Qt::Corner corner) const { return m_corners[static_cast<int>(corner) & 3]; }

/*!
    \since 6.0

    Gives the \a corner of the window to the dock \a area. Nt keeps the value; the top and bottom areas always
    take the corners.
*/
void QMainWindow::setCorner(Qt::Corner corner, Qt::DockWidgetArea area) {
    m_corners[static_cast<int>(corner) & 3] = area;
}

/*!
    \fn bool QMainWindow::documentMode() const
    \since 6.0

    Returns whether the tabs of tabbed dock widgets are drawn as for documents.
*/

/*!
    \since 6.0

    Sets whether the tabs of tabbed dock widgets are drawn as for documents to \a enabled.
*/
void QMainWindow::setDocumentMode(bool enabled) {
    m_documentMode = enabled;
    for (const DockGroup &g : m_groups)
        syncGroup(g.id);
}

/*!
    \fn QTabWidget::TabShape QMainWindow::tabShape() const
    \since 6.0

    Returns the shape of the tabs of tabbed dock widgets.
*/

/*!
    \since 6.0

    Sets the shape of the tabs of tabbed dock widgets to \a tabShape.
*/
void QMainWindow::setTabShape(QTabWidget::TabShape tabShape) {
    m_tabShape = tabShape;
    for (const DockGroup &g : m_groups)
        syncGroup(g.id);
    relayout();
}

namespace {
int areaIndex(Qt::DockWidgetArea area) {
    switch (area) {
    case Qt::LeftDockWidgetArea:
        return 0;
    case Qt::RightDockWidgetArea:
        return 1;
    case Qt::TopDockWidgetArea:
        return 2;
    default:
        return 3;
    }
}
} // namespace

/*!
    \since 6.0

    Returns where the tabs of tabbed dock widgets in \a area are, QTabWidget::South by default.
*/
QTabWidget::TabPosition QMainWindow::tabPosition(Qt::DockWidgetArea area) const {
    return m_tabPositions[areaIndex(area)];
}

/*!
    \since 6.0

    Puts the tabs of tabbed dock widgets in \a areas at \a tabPosition. Nt puts them above the dock widgets for
    QTabWidget::North and below them otherwise.
*/
void QMainWindow::setTabPosition(Qt::DockWidgetAreas areas, QTabWidget::TabPosition tabPosition) {
    for (const Qt::DockWidgetArea area :
         {Qt::LeftDockWidgetArea, Qt::RightDockWidgetArea, Qt::TopDockWidgetArea, Qt::BottomDockWidgetArea})
        if (areas & area)
            m_tabPositions[areaIndex(area)] = tabPosition;
    for (const DockGroup &g : m_groups)
        syncGroup(g.id);
    relayout();
}

/*!
    \fn bool QMainWindow::unifiedTitleAndToolBarOnMac() const
    \since 6.0

    Returns whether the title bar and the top tool bar are drawn as one on macOS. Nt keeps the value.
*/

/*!
    \fn void QMainWindow::setUnifiedTitleAndToolBarOnMac(bool set)
    \since 6.0

    Sets whether the title bar and the top tool bar are drawn as one on macOS to \a set.
*/

/*!
    \since 6.0

    Returns \c true if \a pos is on a separator that resizes a dock area. Nt has no such separators, so this
    returns \c false.
*/
bool QMainWindow::isSeparator(const QPoint &pos) const {
    (void)pos;
    return false;
}

/*!
    \since 6.0

    Resizes the dock widgets \a docks to \a sizes, one for each, across if \a orientation is Qt::Horizontal and down
    otherwise. A size across sets the thickness of a left or right dock area, a size down that of a top or bottom one.

    \code
    resizeDocks({propertiesDock}, {320}, Qt::Horizontal);
    \endcode
*/
void QMainWindow::resizeDocks(const QList<QDockWidget *> &docks, const QList<int> &sizes, Qt::Orientation orientation) {
    for (qsizetype i = 0; i < docks.size() && i < sizes.size(); ++i) {
        for (DockEntry &entry : m_docks) {
            if (entry.dock != docks.at(i))
                continue;
            if (orientation == Qt::Horizontal)
                entry.width = sizes.at(i);
            else
                entry.height = sizes.at(i);
        }
    }
    relayout();
}

#include "moc_qmainwindow.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
