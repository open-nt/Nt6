// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtWidgets/qwidget.h>

#include <vector>

class QDockWidget;
class QMainWindowLayout;
class QMenu;
class QMenuBar;
class QStatusBar;
class QToolBar;

class QMainWindow : public QWidget {
    Q_OBJECT
    Q_PROPERTY(
        Qt::ToolButtonStyle toolButtonStyle READ toolButtonStyle WRITE setToolButtonStyle NOTIFY toolButtonStyleChanged)
public:
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

    Qt::ToolButtonStyle toolButtonStyle() const { return m_toolButtonStyle; }
    virtual QMenu *createPopupMenu();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setToolButtonStyle(Qt::ToolButtonStyle toolButtonStyle);

signals:
    /*!
        This signal is emitted when the style of the tool buttons changes to \a toolButtonStyle.
    */
    void toolButtonStyleChanged(Qt::ToolButtonStyle toolButtonStyle) {
        emitSignal(&QMainWindow::toolButtonStyleChanged, toolButtonStyle);
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
    };

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
    Qt::ToolBarAreas m_pendingBreaks; // the next tool bar added to these areas starts a new line
};
