// MIT License - see LICENSE
#pragma once

#include <QtCore/qrect.h>
#include <QtGui/qpaintdevice.h>
#include <QtGui/qregion.h>
#include <QtGui/qwindow.h>

#include <functional>
#include <memory>

class QBackingStore;
class QPaintEvent;

class QPaintDeviceWindow : public QWindow, public QPaintDevice {
    Q_OBJECT
public:
    ~QPaintDeviceWindow() override;

    int width() const { return QWindow::width(); }
    int height() const { return QWindow::height(); }
    qreal devicePixelRatio() const { return QWindow::devicePixelRatio(); }

    QPaintEngine *paintEngine() const override;

public slots:
    void update();
    void update(const QRect &rect);
    void update(const QRegion &region);

protected:
    QPaintDeviceWindow();
    virtual void paintEvent(QPaintEvent *event);
    int metric(PaintDeviceMetric metric) const override;
    QPaintDevice *redirected(QPoint *offset) const override;
    void exposeEvent(QExposeEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool event(QEvent *event) override;

private:
    void doFlush();
    std::unique_ptr<QBackingStore> m_store;
    QRegion m_dirty;
    // Set by a widget that shows the whole window with OpenGL itself (QOpenGLWidget
    // as a top-level window): it presents the frame and returns true, and the
    // backing store is not painted or flushed. False falls back to the store.
    std::function<bool()> m_present;
    friend struct QtPrivate::GuiAccess;
};
