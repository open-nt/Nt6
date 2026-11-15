// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtGui/qregion.h>
#include <QtWaylandCompositor/qwaylandbufferref.h>

#include <memory>

class QWaylandOutput;
class QWaylandSurface;

class QWaylandView : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *renderObject READ renderObject CONSTANT)
    Q_PROPERTY(QWaylandSurface *surface READ surface WRITE setSurface NOTIFY surfaceChanged)
    Q_PROPERTY(QWaylandOutput *output READ output WRITE setOutput NOTIFY outputChanged)
    Q_PROPERTY(bool bufferLocked READ isBufferLocked WRITE setBufferLocked NOTIFY bufferLockedChanged)
    Q_PROPERTY(bool allowDiscardFrontBuffer READ allowDiscardFrontBuffer WRITE setAllowDiscardFrontBuffer NOTIFY
                   allowDiscardFrontBufferChanged)
public:
    explicit QWaylandView(QObject *renderObject = nullptr, QObject *parent = nullptr);
    ~QWaylandView() override;

    QObject *renderObject() const;

    QWaylandSurface *surface() const;
    void setSurface(QWaylandSurface *surface);

    QWaylandOutput *output() const;
    void setOutput(QWaylandOutput *output);

    virtual void bufferCommitted(const QWaylandBufferRef &ref, const QRegion &damage);
    virtual bool advance();
    virtual void discardCurrentBuffer();
    virtual QWaylandBufferRef currentBuffer();
    virtual QRegion currentBufferDamage();

    bool isBufferLocked() const;
    void setBufferLocked(bool locked);

    bool allowDiscardFrontBuffer() const;
    void setAllowDiscardFrontBuffer(bool discard);

    void setPrimary();
    bool isPrimary() const;

signals:
    /*!
        This signal is emitted when the surface of the view changes.
    */
    void surfaceChanged() { emitSignal(&QWaylandView::surfaceChanged); }
    /*!
        This signal is emitted just before the surface of the view is destroyed.
    */
    void surfaceDestroyed() { emitSignal(&QWaylandView::surfaceDestroyed); }
    /*!
        This signal is emitted when the output of the view changes.
    */
    void outputChanged() { emitSignal(&QWaylandView::outputChanged); }
    /*!
        This signal is emitted when bufferLocked changes.
    */
    void bufferLockedChanged() { emitSignal(&QWaylandView::bufferLockedChanged); }
    /*!
        This signal is emitted when allowDiscardFrontBuffer changes.
    */
    void allowDiscardFrontBufferChanged() { emitSignal(&QWaylandView::allowDiscardFrontBufferChanged); }

private:
    friend class QWaylandSurface;
    struct Private;
    std::unique_ptr<Private> d;
};
