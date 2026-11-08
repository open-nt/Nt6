// MIT License - see LICENSE
#pragma once

#include <QtCore/qvariant.h>
#include <QtWidgets/qwidget.h>

#include <vector>

class QTabBar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)
    Q_PROPERTY(int count READ count)
    Q_PROPERTY(bool tabsClosable READ tabsClosable WRITE setTabsClosable)
    Q_PROPERTY(bool movable READ isMovable WRITE setMovable)
    Q_PROPERTY(
        SelectionBehavior selectionBehaviorOnRemove READ selectionBehaviorOnRemove WRITE setSelectionBehaviorOnRemove)
public:
    enum SelectionBehavior { SelectLeftTab, SelectRightTab, SelectPreviousTab };
    Q_ENUM(SelectionBehavior)

    explicit QTabBar(QWidget *parent = nullptr);
    ~QTabBar() override;

    int addTab(const QString &text);
    int insertTab(int index, const QString &text);
    void removeTab(int index);
    void moveTab(int from, int to);

    bool isTabEnabled(int index) const;
    void setTabEnabled(int index, bool enabled);
    QString tabText(int index) const;
    void setTabText(int index, const QString &text);
    QString tabToolTip(int index) const;
    void setTabToolTip(int index, const QString &tip);
    QVariant tabData(int index) const;
    void setTabData(int index, const QVariant &data);

    QRect tabRect(int index) const;
    int tabAt(const QPoint &position) const;
    int currentIndex() const { return m_current; }
    int count() const { return static_cast<int>(m_tabs.size()); }

    bool tabsClosable() const { return m_closable; }
    void setTabsClosable(bool closable);
    bool isMovable() const { return m_movable; }
    void setMovable(bool movable) { m_movable = movable; }
    SelectionBehavior selectionBehaviorOnRemove() const { return m_onRemove; }
    void setSelectionBehaviorOnRemove(SelectionBehavior behavior) { m_onRemove = behavior; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setCurrentIndex(int index);

signals:
    /*!
        This signal is emitted when the current tab changes to the one at \a index, which is -1 if there are no tabs
        left.
    */
    void currentChanged(int index) { emitSignal(&QTabBar::currentChanged, index); }
    /*!
        This signal is emitted when the user clicks the close button of the tab at \a index. The tab bar does not
        remove the tab itself.
    */
    void tabCloseRequested(int index) { emitSignal(&QTabBar::tabCloseRequested, index); }
    /*!
        This signal is emitted when the tab at \a from moves to \a to, by the user or by moveTab().
    */
    void tabMoved(int from, int to) { emitSignal(&QTabBar::tabMoved, from, to); }
    /*!
        This signal is emitted when the user presses on the tab at \a index, or -1 for the empty part of the bar.
    */
    void tabBarClicked(int index) { emitSignal(&QTabBar::tabBarClicked, index); }
    /*!
        This signal is emitted when the user double-clicks the tab at \a index, or -1 for the empty part of the bar.
    */
    void tabBarDoubleClicked(int index) { emitSignal(&QTabBar::tabBarDoubleClicked, index); }

protected:
    virtual QSize tabSizeHint(int index) const;
    virtual void tabInserted(int index);
    virtual void tabRemoved(int index);
    virtual void tabLayoutChange();

    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    struct Tab {
        QString text;
        QString toolTip;
        QVariant data;
        bool enabled = true;
    };

    QRect closeRect(int index) const;
    bool valid(int index) const { return index >= 0 && index < count(); }
    void step(int direction);
    void changed();

    std::vector<Tab> m_tabs;
    std::vector<int> m_history; // the tabs that were current, the last one most recently
    int m_current = -1;
    int m_hover = -1;
    int m_pressedClose = -1;
    int m_dragging = -1;
    int m_dragOffset = 0; // how far the dragged tab is drawn from its place
    int m_pressX = 0;
    bool m_closable = false;
    bool m_movable = false;
    SelectionBehavior m_onRemove = SelectRightTab;
};
