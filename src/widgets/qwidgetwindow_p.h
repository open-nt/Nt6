// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtGui/qpaintdevicewindow.h>
#include <QtWidgets/qwidget.h>

// The window of a top-level widget. It paints the widget tree and turns the events of the window into
// events for the widgets.
class QWidgetWindow : public QPaintDeviceWindow {
public:
    explicit QWidgetWindow(QWidget *widget);
    ~QWidgetWindow() override;

    QWidget *widget() const { return m_widget; }
    // The widget is going away.
    void detach() { m_widget = nullptr; }
    // The widgets must not be referenced after they are deleted.
    void forget(QWidget *widget);
    // A popup (the list of a combo box) closes when the mouse is pressed outside it.
    void setPopup(QWidget *popup) { m_popup = popup; }
    // Shows the cursor of the widget that the mouse is over: its own, or that of the nearest ancestor that has one.
    void updateCursor();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    bool event(QEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

private:
    void deliverMouse(QMouseEvent *event, QWidget *target);
    void toolTipMoved(QWidget *under, const QPoint &pos, const QPoint &globalPos);
    void cancelToolTip();
    void updateUnderMouse(QWidget *target, const QPointF &local, const QPointF &global);

    QWidget *m_widget;
    QPointer<QWidget> m_grab;
    QPointer<QWidget> m_under;
    QPointer<QWidget> m_popup;
    int m_tipTimer = 0;
    QPointer<QWidget> m_tipWidget;
    QPoint m_tipPos;       // in the window
    QPoint m_tipGlobalPos; // on the screen
};
