// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtWidgets/qwidget.h>

#include <vector>

class QTabBar;
class QStyleOptionTabWidgetFrame;

class QTabWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)
    Q_PROPERTY(int count READ count)
    Q_PROPERTY(TabPosition tabPosition READ tabPosition WRITE setTabPosition)
    Q_PROPERTY(TabShape tabShape READ tabShape WRITE setTabShape)
    Q_PROPERTY(QSize iconSize READ iconSize WRITE setIconSize)
    Q_PROPERTY(Qt::TextElideMode elideMode READ elideMode WRITE setElideMode)
    Q_PROPERTY(bool usesScrollButtons READ usesScrollButtons WRITE setUsesScrollButtons)
    Q_PROPERTY(bool documentMode READ documentMode WRITE setDocumentMode)
    Q_PROPERTY(bool tabsClosable READ tabsClosable WRITE setTabsClosable)
    Q_PROPERTY(bool movable READ isMovable WRITE setMovable)
    Q_PROPERTY(bool tabBarAutoHide READ tabBarAutoHide WRITE setTabBarAutoHide)
public:
    enum TabPosition { North, South, West, East };
    enum TabShape { Rounded, Triangular };
    Q_ENUM(TabPosition)
    Q_ENUM(TabShape)

    explicit QTabWidget(QWidget *parent = nullptr);
    ~QTabWidget() override;

    int addTab(QWidget *page, const QString &label);
    int insertTab(int index, QWidget *page, const QString &label);
    int addTab(QWidget *page, const QIcon &icon, const QString &label);
    int insertTab(int index, QWidget *page, const QIcon &icon, const QString &label);
    QIcon tabIcon(int index) const;
    void setTabIcon(int index, const QIcon &icon);
    bool isTabVisible(int index) const;
    void setTabVisible(int index, bool visible);
    QString tabWhatsThis(int index) const;
    void setTabWhatsThis(int index, const QString &text);
    TabPosition tabPosition() const { return m_position; }
    void setTabPosition(TabPosition position);
    TabShape tabShape() const { return m_shape; }
    void setTabShape(TabShape s);
    QWidget *cornerWidget(Qt::Corner corner = Qt::TopRightCorner) const;
    void setCornerWidget(QWidget *widget, Qt::Corner corner = Qt::TopRightCorner);
    QSize iconSize() const;
    void setIconSize(const QSize &size);
    Qt::TextElideMode elideMode() const;
    void setElideMode(Qt::TextElideMode mode);
    bool usesScrollButtons() const;
    void setUsesScrollButtons(bool useButtons);
    bool tabBarAutoHide() const;
    void setTabBarAutoHide(bool enabled);
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    void removeTab(int index);
    void clear();
    int count() const { return static_cast<int>(m_pages.size()); }
    int currentIndex() const;
    QWidget *currentWidget() const { return widget(currentIndex()); }
    QWidget *widget(int index) const;
    int indexOf(const QWidget *page) const;
    QString tabText(int index) const;
    void setTabText(int index, const QString &text);
    bool isTabEnabled(int index) const;
    void setTabEnabled(int index, bool enabled);
    QString tabToolTip(int index) const;
    void setTabToolTip(int index, const QString &tip);
    bool tabsClosable() const;
    void setTabsClosable(bool closable);
    bool isMovable() const;
    void setMovable(bool movable);
    QTabBar *tabBar() const { return m_bar; }
    bool documentMode() const;
    void setDocumentMode(bool set);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setCurrentIndex(int index);
    void setCurrentWidget(QWidget *page);

signals:
    /*!
        This signal is emitted when the current tab changes to the one at \a index, which is -1 if there are no
        tabs left.
    */
    void currentChanged(int index) { emitSignal(&QTabWidget::currentChanged, index); }
    /*!
        This signal is emitted when the user clicks the close button of the tab at \a index.
    */
    void tabCloseRequested(int index) { emitSignal(&QTabWidget::tabCloseRequested, index); }
    /*!
        This signal is emitted when the user presses on the tab at \a index, or -1 for the empty part of the bar.
    */
    void tabBarClicked(int index) { emitSignal(&QTabWidget::tabBarClicked, index); }
    /*!
        This signal is emitted when the user double-clicks the tab at \a index, or -1 for the empty part of the bar.
    */
    void tabBarDoubleClicked(int index) { emitSignal(&QTabWidget::tabBarDoubleClicked, index); }

protected:
    void setTabBar(QTabBar *tb);
    virtual void tabInserted(int index);
    virtual void tabRemoved(int index);
    void initStyleOption(QStyleOptionTabWidgetFrame *option) const;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void connectBar();
    void applyShape();
    bool verticalBar() const { return m_position == West || m_position == East; }
    int barThickness() const;
    bool isMaterialRail() const;  // the bar is a navigation rail of QMaterialStyle,
    bool isOpenModalRail() const; // and the modal expanded rail, shown over the pages
    QRect frameRect() const;
    void layoutChildren();
    void pageGone(QObject *page);
    void select(int index);
    bool focusIn(const QWidget *page) const;

    std::vector<QWidget *> m_pages;
    QPointer<QWidget> m_shown; // the page that is shown
    QTabBar *m_bar = nullptr;
    TabPosition m_position = North;
    TabShape m_shape = Rounded;
    QPointer<QWidget> m_leftCorner;
    QPointer<QWidget> m_rightCorner;
    QPointer<QWidget> m_scrim; // over the pages behind a modal rail of QMaterialStyle
};
