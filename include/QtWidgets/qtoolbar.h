// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtGui/qaction.h>
#include <QtWidgets/qwidget.h>

#include <vector>

class QBoxLayout;
class QToolButton;

class QToolBar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool movable READ isMovable WRITE setMovable NOTIFY movableChanged)
    Q_PROPERTY(Qt::ToolBarAreas allowedAreas READ allowedAreas WRITE setAllowedAreas NOTIFY allowedAreasChanged)
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    Q_PROPERTY(
        Qt::ToolButtonStyle toolButtonStyle READ toolButtonStyle WRITE setToolButtonStyle NOTIFY toolButtonStyleChanged)
    Q_PROPERTY(bool floating READ isFloating)
    Q_PROPERTY(bool floatable READ isFloatable WRITE setFloatable)
public:
    explicit QToolBar(const QString &title, QWidget *parent = nullptr);
    explicit QToolBar(QWidget *parent = nullptr);
    ~QToolBar() override;

    void setMovable(bool movable);
    bool isMovable() const { return m_movable; }
    void setAllowedAreas(Qt::ToolBarAreas areas);
    Qt::ToolBarAreas allowedAreas() const { return m_allowedAreas; }
    bool isAreaAllowed(Qt::ToolBarArea area) const { return m_allowedAreas.testFlag(area); }
    void setOrientation(Qt::Orientation orientation);
    Qt::Orientation orientation() const { return m_orientation; }
    void clear();

    using QWidget::addAction;
    QAction *addAction(const QString &text);
    template <typename Functor> QAction *addAction(const QString &text, Functor functor) {
        QAction *action = addAction(text);
        QObject::connect(action, &QAction::triggered, action, std::move(functor));
        return action;
    }
    QAction *addSeparator();
    QAction *insertSeparator(QAction *before);
    QAction *addWidget(QWidget *widget);
    QAction *insertWidget(QAction *before, QWidget *widget);

    QRect actionGeometry(QAction *action) const;
    QAction *actionAt(const QPoint &p) const;
    QAction *actionAt(int x, int y) const { return actionAt(QPoint(x, y)); }
    QAction *toggleViewAction() const { return m_toggleView; }
    Qt::ToolButtonStyle toolButtonStyle() const { return m_style; }
    QWidget *widgetForAction(QAction *action) const;
    bool isFloatable() const { return m_floatable; }
    void setFloatable(bool floatable) { m_floatable = floatable; }
    bool isFloating() const { return false; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setToolButtonStyle(Qt::ToolButtonStyle toolButtonStyle);

signals:
    /*!
        This signal is emitted when \a action of the tool bar is triggered.
    */
    void actionTriggered(QAction *action) { emitSignal(&QToolBar::actionTriggered, action); }
    /*!
        This signal is emitted when the tool bar becomes \a movable or stops being so.
    */
    void movableChanged(bool movable) { emitSignal(&QToolBar::movableChanged, movable); }
    /*!
        This signal is emitted when the areas the tool bar may go in change to \a allowedAreas.
    */
    void allowedAreasChanged(Qt::ToolBarAreas allowedAreas) {
        emitSignal(&QToolBar::allowedAreasChanged, allowedAreas);
    }
    /*!
        This signal is emitted when the orientation changes to \a orientation.
    */
    void orientationChanged(Qt::Orientation orientation) { emitSignal(&QToolBar::orientationChanged, orientation); }
    /*!
        This signal is emitted when the style of the buttons changes to \a toolButtonStyle.
    */
    void toolButtonStyleChanged(Qt::ToolButtonStyle toolButtonStyle) {
        emitSignal(&QToolBar::toolButtonStyleChanged, toolButtonStyle);
    }
    /*!
        This signal is emitted when the tool bar starts or stops floating; \a topLevel says which. Tool bars do not
        float in Nt, so it is never emitted.
    */
    void topLevelChanged(bool topLevel) { emitSignal(&QToolBar::topLevelChanged, topLevel); }
    /*!
        This signal is emitted when the tool bar is shown or hidden; \a visible says which.
    */
    void visibilityChanged(bool visible) { emitSignal(&QToolBar::visibilityChanged, visible); }

protected:
    void actionEvent(QActionEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    struct Item {
        QAction *action;
        QWidget *widget;
        bool owned;      // a button or a separator that the tool bar made
        bool fromAction; // a widget that a QWidgetAction gave
    };

    void init(const QString &title);
    void relayout();
    QWidget *createItemWidget(QAction *action, bool *fromAction);
    void syncVisibility(const Item &item);
    int handleExtent() const;

    QBoxLayout *m_layout = nullptr;
    std::vector<Item> m_items;
    std::vector<std::pair<QPointer<QAction>, QPointer<QWidget>>> m_widgetActions;
    QAction *m_toggleView = nullptr;
    Qt::ToolBarAreas m_allowedAreas = Qt::AllToolBarAreas;
    Qt::Orientation m_orientation = Qt::Horizontal;
    Qt::ToolButtonStyle m_style = Qt::ToolButtonIconOnly;
    bool m_movable = true;
    bool m_floatable = true;
};
