// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtGui/qsurface.h>

class QScreen;

class QOffscreenSurface : public QObject, public QSurface {
    Q_OBJECT
public:
    explicit QOffscreenSurface(QScreen *screen = nullptr, QObject *parent = nullptr);
    ~QOffscreenSurface() override;

    SurfaceType surfaceType() const override { return QSurface::OpenGLSurface; }

    void create();
    void destroy();
    bool isValid() const { return m_valid; }

    void setFormat(const QSurfaceFormat &format) { m_format = format; }
    QSurfaceFormat format() const override { return m_format; }
    QSurfaceFormat requestedFormat() const { return m_format; }

    QSize size() const override { return QSize(1, 1); }

    QScreen *screen() const { return m_screen; }
    void setScreen(QScreen *screen);

signals:
    /*!
        This signal is emitted when the screen of the surface changes to \a screen.
    */
    void screenChanged(QScreen *screen) { emitSignal(&QOffscreenSurface::screenChanged, screen); }

private:
    QScreen *m_screen;
    QSurfaceFormat m_format = QSurfaceFormat::defaultFormat();
    bool m_valid = false;
};
