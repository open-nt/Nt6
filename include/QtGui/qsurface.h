// MIT License - see LICENSE
#pragma once

#include <QtCore/qsize.h>
#include <QtGui/qsurfaceformat.h>

class QSurface {
public:
    enum SurfaceClass { Window, Offscreen };
    enum SurfaceType {
        RasterSurface,
        OpenGLSurface,
        RasterGLSurface,
        OpenVGSurface,
        VulkanSurface,
        MetalSurface,
        Direct3DSurface
    };

    virtual ~QSurface();

    SurfaceClass surfaceClass() const { return m_type; }
    virtual QSurfaceFormat format() const = 0;
    virtual SurfaceType surfaceType() const = 0;
    bool supportsOpenGL() const;
    virtual QSize size() const = 0;

protected:
    explicit QSurface(SurfaceClass type) : m_type(type) {}

    SurfaceClass m_type;

private:
    Q_DISABLE_COPY(QSurface)
};
