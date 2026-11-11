// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qrect.h>
#include <QtCore/qstring.h>

class QWindow;

// The platform's side of a QWindow: created by QPlatformIntegration when the window is created.
class QPlatformWindow {
public:
    explicit QPlatformWindow(QWindow *window) : m_window(window) {}
    virtual ~QPlatformWindow();

    QWindow *window() const { return m_window; }
    virtual QRect geometry() const { return m_geometry; }
    // Called when the application moves or resizes the window. The platform reports the result
    // through QWindowSystemInterface::handleGeometryChange().
    virtual void setGeometry(const QRect &rect) { m_geometry = rect; }
    virtual void setVisible(bool visible) = 0;
    virtual void setWindowTitle(const QString &title) { (void)title; }
    virtual void requestActivateWindow() {}
    // Keeps the window above `parent`, as a dialog, or makes it a normal window again if `parent` is null.
    virtual void setTransientParent(const QPlatformWindow *parent) { (void)parent; }
    virtual quintptr winId() const { return 0; }
    // Shows the mouse cursor with this shape while it is over the window.
    virtual void setCursor(Qt::CursorShape shape) { (void)shape; }

private:
    QWindow *m_window;
    QRect m_geometry;
};
