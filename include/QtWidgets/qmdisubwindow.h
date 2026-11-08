// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qpointer.h>
#include <QtWidgets/qwidget.h>

class QMdiArea;
class QToolButton;

class QMdiSubWindow : public QWidget {
    Q_OBJECT
public:
    enum SubWindowOption {
        AllowOutsideAreaHorizontally = 0x1,
        AllowOutsideAreaVertically = 0x2,
        RubberBandResize = 0x4,
        RubberBandMove = 0x8
    };
    Q_DECLARE_FLAGS(SubWindowOptions, SubWindowOption)

    explicit QMdiSubWindow(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    ~QMdiSubWindow() override;

    void setWidget(QWidget *widget);
    QWidget *widget() const { return m_widget.data(); }
    QMdiArea *mdiArea() const;
    void setOption(SubWindowOption option, bool on = true);
    bool testOption(SubWindowOption option) const { return m_options.testFlag(option); }
    Qt::WindowStates windowState() const { return m_state; }
    bool isMaximized() const { return m_state.testFlag(Qt::WindowMaximized); }
    bool isMinimized() const { return m_state.testFlag(Qt::WindowMinimized); }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void showMaximized();
    void showMinimized();
    void showNormal();

signals:
    /*!
        This signal is emitted just before the sub-window becomes the active one.
    */
    void aboutToActivate() { emitSignal(&QMdiSubWindow::aboutToActivate); }
    /*!
        This signal is emitted when the state of the sub-window changes from \a oldState to \a newState.
    */
    void windowStateChanged(Qt::WindowStates oldState, Qt::WindowStates newState) {
        emitSignal(&QMdiSubWindow::windowStateChanged, oldState, newState);
    }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

private:
    friend class QMdiArea;
    enum Edge { NoEdge = 0, Left = 1, Right = 2, Top = 4, Bottom = 8 };

    int titleHeight() const;
    int border() const;
    QRect titleRect() const;
    int edgeAt(const QPoint &pos) const;
    void layoutParts();
    void setState(Qt::WindowStates state);
    void setActive(bool active);
    void updateButtons();

    QPointer<QWidget> m_widget;
    QToolButton *m_minimize = nullptr;
    QToolButton *m_maximize = nullptr;
    QToolButton *m_close = nullptr;
    SubWindowOptions m_options;
    Qt::WindowStates m_state;
    QRect m_normalGeometry;
    QPoint m_pressPos; // in the parent
    QRect m_pressGeometry;
    int m_dragEdges = NoEdge;
    bool m_moving = false;
    bool m_active = false;
    bool m_frameless = false; // in the tabbed view of an area
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QMdiSubWindow::SubWindowOptions)
