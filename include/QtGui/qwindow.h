// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtGui/qcursor.h>
#include <QtGui/qsurface.h>
#include <QtGui/qsurfaceformat.h>

#include <memory>

class QCloseEvent;
class QEnterEvent;
class QExposeEvent;
class QFocusEvent;
class QHideEvent;
class QKeyEvent;
class QMouseEvent;
class QMoveEvent;
class QPlatformWindow;
class QResizeEvent;
class QScreen;
class QShowEvent;
class QVulkanInstance;
class QWheelEvent;

namespace QtPrivate {
struct GuiAccess;
}

class QWindow : public QObject, public QSurface {
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(int x READ x WRITE setX NOTIFY xChanged)
    Q_PROPERTY(int y READ y WRITE setY NOTIFY yChanged)
    Q_PROPERTY(int width READ width WRITE setWidth NOTIFY widthChanged)
    Q_PROPERTY(int height READ height WRITE setHeight NOTIFY heightChanged)
public:
    explicit QWindow(QScreen *screen = nullptr);
    ~QWindow() override;

    quintptr winId() const;
    void create();
    void destroy();
    bool isTopLevel() const { return true; }

    QString title() const;
    void setTitle(const QString &title);
    QWindow *transientParent() const;
    void setTransientParent(QWindow *parent);
    bool isVisible() const;
    bool isExposed() const;
    bool isActive() const;
    QScreen *screen() const;
    qreal devicePixelRatio() const;

    QRect geometry() const;
    void setGeometry(const QRect &rect);
    void setGeometry(int x, int y, int w, int h) { setGeometry(QRect(x, y, w, h)); }
    QPoint position() const { return geometry().topLeft(); }
    void setPosition(const QPoint &pt);
    void setPosition(int x, int y) { setPosition(QPoint(x, y)); }
    QSize size() const override { return geometry().size(); }
    void resize(const QSize &newSize);
    void resize(int w, int h) { resize(QSize(w, h)); }
    int x() const { return geometry().x(); }
    int y() const { return geometry().y(); }
    int width() const { return geometry().width(); }
    int height() const { return geometry().height(); }
    void setX(int arg);
    void setY(int arg);
    void setWidth(int arg);
    void setHeight(int arg);
    QPoint mapToGlobal(const QPoint &pos) const { return pos + position(); }
    QPoint mapFromGlobal(const QPoint &pos) const { return pos - position(); }

    QPlatformWindow *handle() const;

    void setSurfaceType(QSurface::SurfaceType surfaceType);
    QSurface::SurfaceType surfaceType() const override;
    void setFormat(const QSurfaceFormat &format);
    QSurfaceFormat requestedFormat() const;
    QSurfaceFormat format() const override;

    void setVulkanInstance(QVulkanInstance *instance);
    QVulkanInstance *vulkanInstance() const;

    QCursor cursor() const;
    void setCursor(const QCursor &cursor);
    void unsetCursor();

public slots:
    void setVisible(bool visible);
    void show();
    void hide();
    bool close();
    void raise();
    void requestActivate();
    void requestUpdate();

signals:
    /*!
        This signal is emitted when the window is shown or hidden; \a arg is
        the new visibility.
    */
    void visibleChanged(bool arg) { emitSignal(&QWindow::visibleChanged, arg); }
    /*!
        This signal is emitted when the title changes to \a title.
    */
    void titleChanged(const QString &title) { emitSignal(&QWindow::titleChanged, title); }
    /*!
        This signal is emitted when the transient parent of the window changes to \a transientParent.
    */
    void transientParentChanged(QWindow *transientParent) {
        emitSignal(&QWindow::transientParentChanged, transientParent);
    }
    /*!
        This signal is emitted when the x position changes to \a arg.
    */
    void xChanged(int arg) { emitSignal(&QWindow::xChanged, arg); }
    /*!
        This signal is emitted when the y position changes to \a arg.
    */
    void yChanged(int arg) { emitSignal(&QWindow::yChanged, arg); }
    /*!
        This signal is emitted when the width changes to \a arg.
    */
    void widthChanged(int arg) { emitSignal(&QWindow::widthChanged, arg); }
    /*!
        This signal is emitted when the height changes to \a arg.
    */
    void heightChanged(int arg) { emitSignal(&QWindow::heightChanged, arg); }
    /*!
        This signal is emitted when the window becomes the active window
        or stops being it; \a active tells which.
    */
    void activeChanged(bool active) { emitSignal(&QWindow::activeChanged, active); }

protected:
    virtual void exposeEvent(QExposeEvent *event);
    virtual void resizeEvent(QResizeEvent *event);
    virtual void moveEvent(QMoveEvent *event);
    virtual void focusInEvent(QFocusEvent *event);
    virtual void focusOutEvent(QFocusEvent *event);
    virtual void showEvent(QShowEvent *event);
    virtual void hideEvent(QHideEvent *event);
    virtual void closeEvent(QCloseEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void keyReleaseEvent(QKeyEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mouseDoubleClickEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void wheelEvent(QWheelEvent *event);
    bool event(QEvent *event) override;

private:
    friend struct QtPrivate::GuiAccess;
    struct Data;
    std::unique_ptr<Data> m_d;
};
