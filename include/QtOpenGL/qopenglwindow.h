// MIT License - see LICENSE
#pragma once

#include <QtGui/qimage.h>
#include <QtGui/qopengl.h>
#include <QtGui/qpaintdevicewindow.h>

#include <memory>

class QOpenGLContext;

class QOpenGLWindow : public QPaintDeviceWindow {
    Q_OBJECT
public:
    enum UpdateBehavior { NoPartialUpdate, PartialUpdateBlit, PartialUpdateBlend };

    explicit QOpenGLWindow(UpdateBehavior updateBehavior = NoPartialUpdate, QWindow *parent = nullptr);
    explicit QOpenGLWindow(QOpenGLContext *shareContext, UpdateBehavior updateBehavior = NoPartialUpdate,
                           QWindow *parent = nullptr);
    ~QOpenGLWindow() override;

    UpdateBehavior updateBehavior() const;
    bool isValid() const;

    void makeCurrent();
    void doneCurrent();

    QOpenGLContext *context() const;
    QOpenGLContext *shareContext() const;

    GLuint defaultFramebufferObject() const;

    QImage grabFramebuffer();

signals:
    /*!
        This signal is emitted after each frame has been swapped to the screen.
        With a swap interval of 1, frames come at the refresh rate of the display.
    */
    void frameSwapped() { emitSignal(&QOpenGLWindow::frameSwapped); }

protected:
    virtual void initializeGL();
    virtual void resizeGL(int w, int h);
    virtual void paintGL();
    virtual void paintUnderGL();
    virtual void paintOverGL();

    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void exposeEvent(QExposeEvent *event) override;
    bool event(QEvent *event) override;
    int metric(PaintDeviceMetric metric) const override;
    QPaintDevice *redirected(QPoint *) const override;

private:
    Q_DISABLE_COPY(QOpenGLWindow)
    friend class QOpenGLWindowPaintDevice;
    void render();
    struct Data;
    std::unique_ptr<Data> m_d;
};
