// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qpointer.h>
#include <QtWidgets/qwidget.h>

class QAction;
class QToolButton;

class QDockWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool floating READ isFloating WRITE setFloating NOTIFY topLevelChanged)
    Q_PROPERTY(DockWidgetFeatures features READ features WRITE setFeatures NOTIFY featuresChanged)
    Q_PROPERTY(Qt::DockWidgetAreas allowedAreas READ allowedAreas WRITE setAllowedAreas NOTIFY allowedAreasChanged)
public:
    enum DockWidgetFeature {
        DockWidgetClosable = 0x01,
        DockWidgetMovable = 0x02,
        DockWidgetFloatable = 0x04,
        DockWidgetVerticalTitleBar = 0x08,
        DockWidgetFeatureMask = 0x0f,
        NoDockWidgetFeatures = 0x00
    };
    Q_DECLARE_FLAGS(DockWidgetFeatures, DockWidgetFeature)

    explicit QDockWidget(const QString &title, QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    explicit QDockWidget(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    ~QDockWidget() override;

    QWidget *widget() const { return m_widget.data(); }
    void setWidget(QWidget *widget);
    void setFeatures(DockWidgetFeatures features);
    DockWidgetFeatures features() const { return m_features; }
    void setFloating(bool floating);
    bool isFloating() const { return isWindow(); }
    void setAllowedAreas(Qt::DockWidgetAreas areas);
    Qt::DockWidgetAreas allowedAreas() const { return m_allowedAreas; }
    bool isAreaAllowed(Qt::DockWidgetArea area) const { return m_allowedAreas.testFlag(area); }
    void setTitleBarWidget(QWidget *widget);
    QWidget *titleBarWidget() const { return m_customTitle.data(); }
    QAction *toggleViewAction() const { return m_toggleView; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /*!
        This signal is emitted when the features of the dock widget change to \a features.
    */
    void featuresChanged(QDockWidget::DockWidgetFeatures features) {
        emitSignal(&QDockWidget::featuresChanged, features);
    }
    /*!
        This signal is emitted when the dock widget starts or stops floating; \a topLevel is \c true when it floats.
    */
    void topLevelChanged(bool topLevel) { emitSignal(&QDockWidget::topLevelChanged, topLevel); }
    /*!
        This signal is emitted when the areas the dock widget may go in change to \a allowedAreas.
    */
    void allowedAreasChanged(Qt::DockWidgetAreas allowedAreas) {
        emitSignal(&QDockWidget::allowedAreasChanged, allowedAreas);
    }
    /*!
        This signal is emitted when the dock widget is shown or hidden; \a visible says which.
    */
    void visibilityChanged(bool visible) { emitSignal(&QDockWidget::visibilityChanged, visible); }
    /*!
        This signal is emitted when a QMainWindow puts the dock widget in the area \a area.
    */
    void dockLocationChanged(Qt::DockWidgetArea area) { emitSignal(&QDockWidget::dockLocationChanged, area); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    friend class QMainWindow;
    void init(const QString &title);
    int titleHeight() const;
    QRect titleRect() const;
    void layoutParts();
    void updateButtons();

    QPointer<QWidget> m_widget;
    QPointer<QWidget> m_customTitle;
    QToolButton *m_closeButton = nullptr;
    QToolButton *m_floatButton = nullptr;
    QAction *m_toggleView = nullptr;
    DockWidgetFeatures m_features = DockWidgetFeatures(DockWidgetClosable) | DockWidgetMovable | DockWidgetFloatable;
    Qt::DockWidgetAreas m_allowedAreas = Qt::AllDockWidgetAreas;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QDockWidget::DockWidgetFeatures)
