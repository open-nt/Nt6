// MIT License - see LICENSE
#pragma once

#include <QtGui/qimage.h>
#include <QtGui/qopengl.h>
#include <QtGui/qsurfaceformat.h>
#include <QtWidgets/qwidget.h>

#include <memory>

class QOpenGLContext;

class QOpenGLWidget : public QWidget {
    Q_OBJECT
public:
    enum UpdateBehavior { NoPartialUpdate, PartialUpdate };

    explicit QOpenGLWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~QOpenGLWidget() override;

    void setUpdateBehavior(UpdateBehavior updateBehavior);
    UpdateBehavior updateBehavior() const;

    void setFormat(const QSurfaceFormat &format);
    QSurfaceFormat format() const;

    GLenum textureFormat() const;
    void setTextureFormat(GLenum texFormat);

    bool isValid() const;

    void makeCurrent();
    void doneCurrent();

    QOpenGLContext *context() const;
    GLuint defaultFramebufferObject() const;

    QImage grabFramebuffer();

signals:
    /*!
        This signal is emitted before the frame that paintGL() rendered is put
        on the widget.
    */
    void aboutToCompose() { emitSignal(&QOpenGLWidget::aboutToCompose); }
    /*!
        This signal is emitted after the frame that paintGL() rendered has been
        put on the widget.
    */
    void frameSwapped() { emitSignal(&QOpenGLWidget::frameSwapped); }
    /*!
        This signal is emitted before the framebuffer of the widget is resized.
    */
    void aboutToResize() { emitSignal(&QOpenGLWidget::aboutToResize); }
    /*!
        This signal is emitted after the framebuffer of the widget has been
        resized and resizeGL() called.
    */
    void resized() { emitSignal(&QOpenGLWidget::resized); }

protected:
    virtual void initializeGL();
    virtual void resizeGL(int w, int h);
    virtual void paintGL();

    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    bool event(QEvent *e) override;
    QPaintDevice *redirected(QPoint *p) const override;
    bool systemClip(QRegion *region) const override;

private:
    Q_DISABLE_COPY(QOpenGLWidget)
    bool initialize();
    void ensureFramebuffer();
    bool render();
    bool renderFrame();
    void readBack();
    void hookWindow();
    bool presentDirect();
    struct Data;
    std::unique_ptr<Data> m_d;
};
