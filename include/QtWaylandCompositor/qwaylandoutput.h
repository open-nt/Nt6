// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtWaylandCompositor/qwaylandcompositorextension.h>

#include <memory>

class QWaylandClient;
class QWaylandCompositor;
class QWaylandSurface;
class QWaylandView;
class QWindow;

class QWaylandOutputMode {
public:
    QWaylandOutputMode();
    QWaylandOutputMode(const QSize &size, int refreshRate);

    /*!
        Returns \c true if the mode has a size and a refresh rate.
    */
    bool isValid() const { return m_size.isValid() && m_refreshRate > 0; }
    /*!
        Returns the size in pixels.
    */
    QSize size() const { return m_size; }
    /*!
        Returns the refresh rate in millihertz.
    */
    int refreshRate() const { return m_refreshRate; }

    /*!
        Returns \c true if \a other has the same size and refresh rate.
    */
    bool operator==(const QWaylandOutputMode &other) const {
        return m_size == other.m_size && m_refreshRate == other.m_refreshRate;
    }
    /*!
        Returns \c true if \a other differs in size or refresh rate.
    */
    bool operator!=(const QWaylandOutputMode &other) const { return !(*this == other); }

private:
    QSize m_size;
    int m_refreshRate = 0;
};

class QWaylandOutput : public QWaylandObject {
    Q_OBJECT
    Q_PROPERTY(QWaylandCompositor *compositor READ compositor WRITE setCompositor NOTIFY compositorChanged)
    Q_PROPERTY(QWindow *window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(QString manufacturer READ manufacturer WRITE setManufacturer NOTIFY manufacturerChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QPoint position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QRect geometry READ geometry NOTIFY geometryChanged)
    Q_PROPERTY(
        QRect availableGeometry READ availableGeometry WRITE setAvailableGeometry NOTIFY availableGeometryChanged)
    Q_PROPERTY(QSize physicalSize READ physicalSize WRITE setPhysicalSize NOTIFY physicalSizeChanged)
    Q_PROPERTY(QWaylandOutput::Subpixel subpixel READ subpixel WRITE setSubpixel NOTIFY subpixelChanged)
    Q_PROPERTY(QWaylandOutput::Transform transform READ transform WRITE setTransform NOTIFY transformChanged)
    Q_PROPERTY(int scaleFactor READ scaleFactor WRITE setScaleFactor NOTIFY scaleFactorChanged)
    Q_PROPERTY(bool sizeFollowsWindow READ sizeFollowsWindow WRITE setSizeFollowsWindow NOTIFY sizeFollowsWindowChanged)
public:
    enum Subpixel {
        SubpixelUnknown = 0,
        SubpixelNone,
        SubpixelHorizontalRgb,
        SubpixelHorizontalBgr,
        SubpixelVerticalRgb,
        SubpixelVerticalBgr
    };
    Q_ENUM(Subpixel)

    enum Transform {
        TransformNormal = 0,
        Transform90,
        Transform180,
        Transform270,
        TransformFlipped,
        TransformFlipped90,
        TransformFlipped180,
        TransformFlipped270
    };
    Q_ENUM(Transform)

    QWaylandOutput();
    QWaylandOutput(QWaylandCompositor *compositor, QWindow *window);
    ~QWaylandOutput() override;

    static QWaylandOutput *fromResource(void *resource);

    QWaylandCompositor *compositor() const;
    void setCompositor(QWaylandCompositor *compositor);

    QWindow *window() const;
    void setWindow(QWindow *window);

    QString manufacturer() const;
    void setManufacturer(const QString &manufacturer);

    QString model() const;
    void setModel(const QString &model);

    QPoint position() const;
    void setPosition(const QPoint &pt);

    QList<QWaylandOutputMode> modes() const;
    void addMode(const QWaylandOutputMode &mode, bool preferred = false);
    QWaylandOutputMode currentMode() const;
    void setCurrentMode(const QWaylandOutputMode &mode);

    QRect geometry() const;
    QRect availableGeometry() const;
    void setAvailableGeometry(const QRect &availableGeometry);

    QSize physicalSize() const;
    void setPhysicalSize(const QSize &size);

    Subpixel subpixel() const;
    void setSubpixel(const Subpixel &subpixel);

    Transform transform() const;
    void setTransform(const Transform &transform);

    int scaleFactor() const;
    void setScaleFactor(int scale);

    bool sizeFollowsWindow() const;
    void setSizeFollowsWindow(bool follow);

    bool physicalSizeFollowsSize() const;
    void setPhysicalSizeFollowsSize(bool follow);

    void frameStarted();
    void sendFrameCallbacks();

    void surfaceEnter(QWaylandSurface *surface);
    void surfaceLeave(QWaylandSurface *surface);

    virtual void update();

signals:
    /*!
        This signal is emitted when the compositor changes.
    */
    void compositorChanged() { emitSignal(&QWaylandOutput::compositorChanged); }
    /*!
        This signal is emitted when the window changes.
    */
    void windowChanged() { emitSignal(&QWaylandOutput::windowChanged); }
    /*!
        This signal is emitted when the position changes.
    */
    void positionChanged() { emitSignal(&QWaylandOutput::positionChanged); }
    /*!
        This signal is emitted when the geometry changes.
    */
    void geometryChanged() { emitSignal(&QWaylandOutput::geometryChanged); }
    /*!
        This signal is emitted when a mode has been added.
    */
    void modeAdded() { emitSignal(&QWaylandOutput::modeAdded); }
    /*!
        This signal is emitted when the current mode changes.
    */
    void currentModeChanged() { emitSignal(&QWaylandOutput::currentModeChanged); }
    /*!
        This signal is emitted when the available geometry changes.
    */
    void availableGeometryChanged() { emitSignal(&QWaylandOutput::availableGeometryChanged); }
    /*!
        This signal is emitted when the physical size changes.
    */
    void physicalSizeChanged() { emitSignal(&QWaylandOutput::physicalSizeChanged); }
    /*!
        This signal is emitted when the scale factor changes.
    */
    void scaleFactorChanged() { emitSignal(&QWaylandOutput::scaleFactorChanged); }
    /*!
        This signal is emitted when the subpixel layout changes.
    */
    void subpixelChanged() { emitSignal(&QWaylandOutput::subpixelChanged); }
    /*!
        This signal is emitted when the transform changes.
    */
    void transformChanged() { emitSignal(&QWaylandOutput::transformChanged); }
    /*!
        This signal is emitted when sizeFollowsWindow changes.
    */
    void sizeFollowsWindowChanged() { emitSignal(&QWaylandOutput::sizeFollowsWindowChanged); }
    /*!
        This signal is emitted when physicalSizeFollowsSize changes.
    */
    void physicalSizeFollowsSizeChanged() { emitSignal(&QWaylandOutput::physicalSizeFollowsSizeChanged); }
    /*!
        This signal is emitted when the manufacturer changes.
    */
    void manufacturerChanged() { emitSignal(&QWaylandOutput::manufacturerChanged); }
    /*!
        This signal is emitted when the model changes.
    */
    void modelChanged() { emitSignal(&QWaylandOutput::modelChanged); }
    /*!
        This signal is emitted when the window of the output is destroyed.
    */
    void windowDestroyed() { emitSignal(&QWaylandOutput::windowDestroyed); }

protected:
    bool event(QEvent *event) override;
    virtual void initialize();

private:
    friend class QWaylandCompositor;
    friend class QWaylandSurface;
    friend struct QWaylandOutputPrivateAccess;
    struct Private;
    std::unique_ptr<Private> d;
};
