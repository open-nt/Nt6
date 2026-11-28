// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>

#include <QtCore/qvariant.h>
#include <QtGui/qcolor.h>
#include <QtGui/qicon.h>
#include <QtGui/qtransform.h>
#include <QtWidgets/qwidget.h>

#include <vector>

class QStyleOptionTab;

class QTabBar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)
    Q_PROPERTY(int count READ count)
    Q_PROPERTY(bool tabsClosable READ tabsClosable WRITE setTabsClosable)
    Q_PROPERTY(bool movable READ isMovable WRITE setMovable)
    Q_PROPERTY(
        SelectionBehavior selectionBehaviorOnRemove READ selectionBehaviorOnRemove WRITE setSelectionBehaviorOnRemove)
    Q_PROPERTY(Shape shape READ shape WRITE setShape)
    Q_PROPERTY(QSize iconSize READ iconSize WRITE setIconSize)
    Q_PROPERTY(Qt::TextElideMode elideMode READ elideMode WRITE setElideMode)
    Q_PROPERTY(bool usesScrollButtons READ usesScrollButtons WRITE setUsesScrollButtons)
    Q_PROPERTY(bool documentMode READ documentMode WRITE setDocumentMode)
    Q_PROPERTY(bool autoHide READ autoHide WRITE setAutoHide)
    Q_PROPERTY(bool changeCurrentOnDrag READ changeCurrentOnDrag WRITE setChangeCurrentOnDrag)
    Q_PROPERTY(bool expanding READ expanding WRITE setExpanding)
    Q_PROPERTY(bool drawBase READ drawBase WRITE setDrawBase)
public:
    enum Shape {
        RoundedNorth,
        RoundedSouth,
        RoundedWest,
        RoundedEast,
        TriangularNorth,
        TriangularSouth,
        TriangularWest,
        TriangularEast
    };
    enum SelectionBehavior { SelectLeftTab, SelectRightTab, SelectPreviousTab };
    enum ButtonPosition { LeftSide, RightSide };
    Q_ENUM(Shape)
    Q_ENUM(ButtonPosition)
    Q_ENUM(SelectionBehavior)

    explicit QTabBar(QWidget *parent = nullptr);
    ~QTabBar() override;

    int addTab(const QString &text);
    int insertTab(int index, const QString &text);
    int addTab(const QIcon &icon, const QString &text);
    int insertTab(int index, const QIcon &icon, const QString &text);
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
    QIcon tabIcon(int index) const;
    void setTabIcon(int index, const QIcon &icon);
    QColor tabTextColor(int index) const;
    void setTabTextColor(int index, const QColor &color);
    QString tabWhatsThis(int index) const;
    void setTabWhatsThis(int index, const QString &text);
    QString accessibleTabName(int index) const;
    void setAccessibleTabName(int index, const QString &name);
    bool isTabVisible(int index) const;
    void setTabVisible(int index, bool visible);
    Shape shape() const { return m_shape; }
    void setShape(Shape shape);
    QSize iconSize() const;
    void setIconSize(const QSize &size);
    Qt::TextElideMode elideMode() const { return m_elideMode; }
    void setElideMode(Qt::TextElideMode mode);
    bool autoHide() const { return m_autoHide; }
    void setAutoHide(bool hide);
    bool changeCurrentOnDrag() const { return m_changeCurrentOnDrag; }
    void setChangeCurrentOnDrag(bool change) { m_changeCurrentOnDrag = change; }

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
    QWidget *tabButton(int index, ButtonPosition position) const;
    void setTabButton(int index, ButtonPosition position, QWidget *widget);
    bool expanding() const { return m_expanding; }
    void setExpanding(bool enabled) { m_expanding = enabled; }
    bool drawBase() const { return m_drawBase; }
    void setDrawBase(bool drawTheBase) { m_drawBase = drawTheBase; }
    bool documentMode() const { return m_documentMode; }
    void setDocumentMode(bool set);
    bool usesScrollButtons() const { return m_scrollButtons; }
    void setUsesScrollButtons(bool useButtons) { m_scrollButtons = useButtons; }

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
    virtual QSize minimumTabSizeHint(int index) const;
    void initStyleOption(QStyleOptionTab *option, int tabIndex) const;
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
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Tab {
        QString text;
        QString toolTip;
        QVariant data;
        bool enabled = true;
        QPointer<QWidget> left;  // tabButton(): owned by the tab bar
        QPointer<QWidget> right;
        QIcon icon;
        QColor textColor;
        QString whatsThis;
        QString accessibleName;
        bool visible = true;
    };
    void placeButtons();
    bool vertical() const;
    int along(const QPoint &point) const { return vertical() ? point.y() : point.x(); }
    int lengthOf(const QRect &rect) const { return vertical() ? rect.height() : rect.width(); }
    std::vector<QRect> layoutTabs() const; // the rectangles of the tabs
    // The transform from the coordinates of the tab at `rect`, drawn as a tab at the top, to the tab bar.
    QTransform tabTransform(const QRect &rect) const;
    QSize horizontalSize(int index, bool minimum) const;
    void autoHideChanged();
    // A navigation rail of the Material style whose destinations do not fit scrolls them.
    bool isRail() const;
    int railRange() const;  // how far the rail scrolls: 0 when its destinations fit
    int railScroll() const; // how far it is scrolled, within railRange()
    int railHeader() const; // the bottom of the corner widget at the top of a rail, or 0
    int railTop() const;    // the top of its first destination
    int railShown() const;  // how far it is drawn scrolled, on its way to railScroll()
    void scrollRailTo(int scroll, bool spring = false);
    void showInRail(int index, bool spring = false);

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
    bool m_expanding = true;
    bool m_drawBase = true;
    bool m_documentMode = false;
    bool m_scrollButtons = true;
    SelectionBehavior m_onRemove = SelectRightTab;
    Shape m_shape = RoundedNorth;
    QSize m_iconSize; // invalid: the size of the style
    Qt::TextElideMode m_elideMode = Qt::ElideNone;
    bool m_autoHide = false;
    bool m_changeCurrentOnDrag = false;
    int m_railScroll = 0;      // how far the destinations of a rail are scrolled up
    int m_railPressScroll = 0; // and how far they were when it was pressed
    int m_railPressed = -1;    // the destination pressed on a rail that scrolls, chosen at the release
    bool m_railPressing = false;
    bool m_railDragged = false; // the press has moved the destinations
    bool m_railSprung = false;  // the destinations spring to m_railScroll rather than being there
    int m_railPlaced = 0;       // where they were drawn when the buttons were last placed
};
