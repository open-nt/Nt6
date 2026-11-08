// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qpointer.h>
#include <QtGui/qbrush.h>
#include <QtWidgets/qabstractscrollarea.h>
#include <QtWidgets/qmdisubwindow.h>

#include <vector>

class QTabBar;

class QMdiArea : public QAbstractScrollArea {
    Q_OBJECT
    Q_PROPERTY(QBrush background READ background WRITE setBackground)
    Q_PROPERTY(WindowOrder activationOrder READ activationOrder WRITE setActivationOrder)
    Q_PROPERTY(ViewMode viewMode READ viewMode WRITE setViewMode)
    Q_PROPERTY(bool tabsClosable READ tabsClosable WRITE setTabsClosable)
    Q_PROPERTY(bool tabsMovable READ tabsMovable WRITE setTabsMovable)
public:
    enum AreaOption { DontMaximizeSubWindowOnActivation = 0x1 };
    Q_DECLARE_FLAGS(AreaOptions, AreaOption)
    enum WindowOrder { CreationOrder, StackingOrder, ActivationHistoryOrder };
    Q_ENUM(WindowOrder)
    enum ViewMode { SubWindowView, TabbedView };
    Q_ENUM(ViewMode)

    explicit QMdiArea(QWidget *parent = nullptr);
    ~QMdiArea() override;

    QMdiSubWindow *addSubWindow(QWidget *widget, Qt::WindowFlags flags = Qt::WindowFlags());
    void removeSubWindow(QWidget *widget);
    QList<QMdiSubWindow *> subWindowList(WindowOrder order = CreationOrder) const;
    QMdiSubWindow *currentSubWindow() const;
    QMdiSubWindow *activeSubWindow() const;

    QBrush background() const { return m_background; }
    void setBackground(const QBrush &background);
    WindowOrder activationOrder() const { return m_order; }
    void setActivationOrder(WindowOrder order) { m_order = order; }
    void setOption(AreaOption option, bool on = true);
    bool testOption(AreaOption option) const { return m_options.testFlag(option); }
    ViewMode viewMode() const { return m_viewMode; }
    void setViewMode(ViewMode mode);
    bool tabsClosable() const;
    void setTabsClosable(bool closable);
    bool tabsMovable() const;
    void setTabsMovable(bool movable);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setActiveSubWindow(QMdiSubWindow *window);
    void tileSubWindows();
    void cascadeSubWindows();
    void closeActiveSubWindow();
    void closeAllSubWindows();
    void activateNextSubWindow();
    void activatePreviousSubWindow();

signals:
    /*!
        This signal is emitted when the active sub-window changes to \a window, which is \c nullptr if there is none.
    */
    void subWindowActivated(QMdiSubWindow *window) { emitSignal(&QMdiArea::subWindowActivated, window); }

protected:
    bool viewportEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    friend class QMdiSubWindow;
    QTabBar *tabBar();
    void forget(QMdiSubWindow *window);
    void activate(QMdiSubWindow *window);
    void layoutTabbed();
    void syncTabs();
    QList<QMdiSubWindow *> visibleWindows() const;

    std::vector<QPointer<QMdiSubWindow>> m_windows; // in creation order
    std::vector<QPointer<QMdiSubWindow>> m_history; // the most recently active last
    QPointer<QMdiSubWindow> m_active;
    QTabBar *m_tabs = nullptr;
    QBrush m_background;
    WindowOrder m_order = CreationOrder;
    AreaOptions m_options;
    ViewMode m_viewMode = SubWindowView;
    bool m_syncing = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QMdiArea::AreaOptions)
