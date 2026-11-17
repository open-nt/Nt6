// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtWidgets/qtabwidget.h>
#include <QtWidgets/qwidget.h>

#include <vector>

class QDockWidget;
class QMainWindowLayout;
class QMenu;
class QMenuBar;
class QTabBar;
class QStatusBar;
class QToolBar;

class QMainWindow : public QWidget {
    Q_OBJECT
    Q_PROPERTY(
        Qt::ToolButtonStyle toolButtonStyle READ toolButtonStyle WRITE setToolButtonStyle NOTIFY toolButtonStyleChanged)
    Q_PROPERTY(QSize iconSize READ iconSize WRITE setIconSize)
    Q_PROPERTY(bool animated READ isAnimated WRITE setAnimated)
    Q_PROPERTY(bool documentMode READ documentMode WRITE setDocumentMode)
    Q_PROPERTY(QTabWidget::TabShape tabShape READ tabShape WRITE setTabShape)
    Q_PROPERTY(bool dockNestingEnabled READ isDockNestingEnabled WRITE setDockNestingEnabled)
    Q_PROPERTY(DockOptions dockOptions READ dockOptions WRITE setDockOptions)
    Q_PROPERTY(bool unifiedTitleAndToolBarOnMac READ unifiedTitleAndToolBarOnMac WRITE setUnifiedTitleAndToolBarOnMac)
public:
    enum DockOption {
        AnimatedDocks = 0x01,
        AllowNestedDocks = 0x02,
        AllowTabbedDocks = 0x04,
        ForceTabbedDocks = 0x08,
        VerticalTabs = 0x10,
        GroupedDragging = 0x20
    };
    Q_ENUM(DockOption)
    Q_DECLARE_FLAGS(DockOptions, DockOption)
    Q_FLAG(DockOptions)

    explicit QMainWindow(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    ~QMainWindow() override;

    QMenuBar *menuBar() const;
    void setMenuBar(QMenuBar *menubar);
    QWidget *menuWidget() const { return m_menu.data(); }
    void setMenuWidget(QWidget *menubar);
    QStatusBar *statusBar() const;
    void setStatusBar(QStatusBar *statusbar);
    QWidget *centralWidget() const { return m_central.data(); }
    void setCentralWidget(QWidget *widget);
    QWidget *takeCentralWidget();

    void addToolBar(Qt::ToolBarArea area, QToolBar *toolbar);
    void addToolBar(QToolBar *toolbar);
    QToolBar *addToolBar(const QString &title);
    void insertToolBar(QToolBar *before, QToolBar *toolbar);
    void addToolBarBreak(Qt::ToolBarArea area = Qt::TopToolBarArea);
    void insertToolBarBreak(QToolBar *before);
    void removeToolBar(QToolBar *toolbar);
    void removeToolBarBreak(QToolBar *before);
    Qt::ToolBarArea toolBarArea(const QToolBar *toolbar) const;
    bool toolBarBreak(QToolBar *toolbar) const;

    void addDockWidget(Qt::DockWidgetArea area, QDockWidget *dockwidget);
    void addDockWidget(Qt::DockWidgetArea area, QDockWidget *dockwidget, Qt::Orientation orientation);
    void removeDockWidget(QDockWidget *dockwidget);
    Qt::DockWidgetArea dockWidgetArea(QDockWidget *dockwidget) const;
    bool restoreDockWidget(QDockWidget *dockwidget);
    void resizeDocks(const QList<QDockWidget *> &docks, const QList<int> &sizes, Qt::Orientation orientation);
    void splitDockWidget(QDockWidget *after, QDockWidget *dockwidget, Qt::Orientation orientation);
    void tabifyDockWidget(QDockWidget *first, QDockWidget *second);
    QList<QDockWidget *> tabifiedDockWidgets(QDockWidget *dockwidget) const;
    QByteArray saveState(int version = 0) const;
    bool restoreState(const QByteArray &state, int version = 0);
    DockOptions dockOptions() const { return m_dockOptions; }
    void setDockOptions(DockOptions options);
    bool isAnimated() const { return m_dockOptions.testFlag(AnimatedDocks); }
    bool isDockNestingEnabled() const { return m_dockOptions.testFlag(AllowNestedDocks); }
    Qt::DockWidgetArea corner(Qt::Corner corner) const;
    void setCorner(Qt::Corner corner, Qt::DockWidgetArea area);
    bool documentMode() const { return m_documentMode; }
    void setDocumentMode(bool enabled);
    QTabWidget::TabShape tabShape() const { return m_tabShape; }
    void setTabShape(QTabWidget::TabShape tabShape);
    QTabWidget::TabPosition tabPosition(Qt::DockWidgetArea area) const;
    void setTabPosition(Qt::DockWidgetAreas areas, QTabWidget::TabPosition tabPosition);
    bool unifiedTitleAndToolBarOnMac() const { return m_unifiedTitle; }
    void setUnifiedTitleAndToolBarOnMac(bool set) { m_unifiedTitle = set; }
    bool isSeparator(const QPoint &pos) const;

    Qt::ToolButtonStyle toolButtonStyle() const { return m_toolButtonStyle; }
    virtual QMenu *createPopupMenu();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setToolButtonStyle(Qt::ToolButtonStyle toolButtonStyle);
    QSize iconSize() const;
    void setIconSize(const QSize &iconSize);
    void setAnimated(bool enabled);
    void setDockNestingEnabled(bool enabled);

signals:
    /*!
        This signal is emitted when the style of the tool buttons changes to \a toolButtonStyle.
    */
    void toolButtonStyleChanged(Qt::ToolButtonStyle toolButtonStyle) {
        emitSignal(&QMainWindow::toolButtonStyleChanged, toolButtonStyle);
    }
    /*!
        \since 6.0
        This signal is emitted when the size of the icons of the tool bars changes to \a iconSize.
    */
    void iconSizeChanged(const QSize &iconSize) { emitSignal(&QMainWindow::iconSizeChanged, iconSize); }
    /*!
        \since 6.0
        This signal is emitted when the user makes \a dockWidget the current one of the dock widgets tabbed with it.
    */
    void tabifiedDockWidgetActivated(QDockWidget *dockWidget) {
        emitSignal(&QMainWindow::tabifiedDockWidgetActivated, dockWidget);
    }

private:
    friend class QMainWindowLayout;
    friend class QDockWidget;
    struct ToolBarEntry {
        QPointer<QWidget> bar; // a QToolBar
        Qt::ToolBarArea area;
        bool lineBreak; // starts a new line of its area
    };
    struct DockEntry {
        QPointer<QWidget> dock; // a QDockWidget
        Qt::DockWidgetArea area;
        int width = -1; // set by resizeDocks(); -1: the size hint
        int height = -1;
        int group = 0; // the tabs it shares the place of with other dock widgets; 0: none
    };
    struct DockGroup {
        int id = 0;
        QPointer<QTabBar> bar;
        QPointer<QWidget> current; // the dock widget that is shown
    };
    DockEntry *entryOf(const QWidget *dock) const;
    DockGroup *groupOf(int id) const;
    void leaveGroup(const QWidget *dock) const;
    void syncGroup(int id);

    void manage(QWidget *widget);
    void unmanage(QWidget *widget);
    void relayout();
    void prune() const;

    QMainWindowLayout *m_layout = nullptr;
    QPointer<QWidget> m_menu;
    QPointer<QWidget> m_status; // a QStatusBar
    QPointer<QWidget> m_central;
    mutable std::vector<ToolBarEntry> m_toolBars;
    mutable std::vector<DockEntry> m_docks;
    Qt::ToolButtonStyle m_toolButtonStyle = Qt::ToolButtonIconOnly;
    QSize m_iconSize;                 // invalid until set: the tool bars use the size of the style
    mutable std::vector<DockGroup> m_groups;
    int m_nextGroup = 1;
    DockOptions m_dockOptions = DockOptions(AnimatedDocks) | AllowTabbedDocks;
    Qt::DockWidgetArea m_corners[4] = {Qt::TopDockWidgetArea, Qt::TopDockWidgetArea, Qt::BottomDockWidgetArea,
                                       Qt::BottomDockWidgetArea};
    bool m_documentMode = false;
    bool m_unifiedTitle = false;
    QTabWidget::TabShape m_tabShape = QTabWidget::Rounded;
    QTabWidget::TabPosition m_tabPositions[4] = {QTabWidget::South, QTabWidget::South, QTabWidget::South,
                                                 QTabWidget::South};
    Qt::ToolBarAreas m_pendingBreaks; // the next tool bar added to these areas starts a new line
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QMainWindow::DockOptions)
