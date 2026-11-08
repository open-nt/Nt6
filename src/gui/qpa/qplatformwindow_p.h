// MIT License - see LICENSE
#pragma once

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
    virtual quintptr winId() const { return 0; }

private:
    QWindow *m_window;
    QRect m_geometry;
};
