// MIT License - see LICENSE
#include <QtWaylandCompositor/qwaylandoutput.h>

#include "qwaylandcompositor_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qcoreevent.h>
#include <QtCore/qdebug.h>
#include <QtGui/qwindow.h>
#include <QtWaylandCompositor/qwaylandclient.h>
#include <QtWaylandCompositor/qwaylandsurface.h>

using namespace QtWaylandPrivate;

class QWaylandOutput::Private::Handler : public ResourceHandler {
public:
    explicit Handler(QWaylandOutput *output) : q(output) {}
    void request(Resource *resource, std::uint32_t opcode, const Args &) override {
        if (opcode == kOutputRelease)
            resource->destroy();
    }
    void resourceDestroyed(Resource *resource) override {
        auto &v = q->d->resources;
        v.erase(std::remove(v.begin(), v.end(), resource), v.end());
    }
    QWaylandOutput *q;
};

/*!
    \class QWaylandOutputMode
    \brief The QWaylandOutputMode class is a size and refresh rate an output can run at.

    \differences
    None.
    \enddifferences
*/

/*!
    Constructs an invalid mode.
*/
QWaylandOutputMode::QWaylandOutputMode() = default;

/*!
    Constructs a mode of \a size pixels at \a refreshRate millihertz.
*/
QWaylandOutputMode::QWaylandOutputMode(const QSize &size, int refreshRate) : m_size(size), m_refreshRate(refreshRate) {}

/*!
    \class QWaylandOutput
    \brief The QWaylandOutput class is a screen the compositor tells its clients about.

    An output has a position in the compositor's coordinate space, modes (sizes and refresh rates) of which one
    is current, a scale factor and physical properties. Clients learn of it through the \c wl_output global, and
    a surface is told which outputs it is on with QWaylandSurface::sendEnter(). With a window, the output follows
    the window's size when sizeFollowsWindow() is set.

    \differences
    fromResource() takes an opaque handle. The \c wl_output name and description events are sent with the model
    and manufacturer.
    \enddifferences

    \sa QWaylandCompositor::defaultOutput()
*/

/*!
    \enum QWaylandOutput::Subpixel

    The subpixel layout of the screen.

    \value SubpixelUnknown Unknown.
    \value SubpixelNone No subpixels, as on a projector.
    \value SubpixelHorizontalRgb Horizontal RGB.
    \value SubpixelHorizontalBgr Horizontal BGR.
    \value SubpixelVerticalRgb Vertical RGB.
    \value SubpixelVerticalBgr Vertical BGR.
*/

/*!
    \enum QWaylandOutput::Transform

    How the screen is rotated or flipped relative to the content.

    \value TransformNormal Not rotated.
    \value Transform90 Rotated by 90 degrees.
    \value Transform180 Rotated by 180 degrees.
    \value Transform270 Rotated by 270 degrees.
    \value TransformFlipped Flipped.
    \value TransformFlipped90 Flipped and rotated by 90 degrees.
    \value TransformFlipped180 Flipped and rotated by 180 degrees.
    \value TransformFlipped270 Flipped and rotated by 270 degrees.
*/

/*!
    Constructs an output without a compositor; call setCompositor().
*/
QWaylandOutput::QWaylandOutput() : QWaylandObject(nullptr), d(std::make_unique<Private>()) {
    d->handler = std::make_unique<Private::Handler>(this);
    QCoreApplication::postEvent(this, new QEvent(QEvent::User));
}

/*!
    Constructs an output of \a compositor shown in \a window, which may be \c nullptr.
*/
QWaylandOutput::QWaylandOutput(QWaylandCompositor *compositor, QWindow *window)
    : QWaylandObject(compositor), d(std::make_unique<Private>()) {
    d->handler = std::make_unique<Private::Handler>(this);
    d->window = window;
    // Not setCompositor(): that may call initialize(), which is virtual, from a constructor. The posted event
    // initializes the output once the loop runs.
    d->compositor = compositor;
    if (compositor)
        QWaylandCompositorPrivateAccess::addOutput(compositor, this);
    QCoreApplication::postEvent(this, new QEvent(QEvent::User));
}

/*!
    Destroys the output; clients are told it is gone.
*/
QWaylandOutput::~QWaylandOutput() {
    if (d->compositor) {
        if (d->global)
            QWaylandCompositorPrivateAccess::display(d->compositor)->removeGlobal(d->global);
        QWaylandCompositorPrivateAccess::removeOutput(d->compositor, this);
    }
    for (Resource *r : std::vector<Resource *>(d->resources))
        r->setHandler(nullptr);
}

/*!
    Returns the output behind the opaque handle \a resource, or \c nullptr.
*/
QWaylandOutput *QWaylandOutput::fromResource(void *resource) {
    auto *r = static_cast<Resource *>(resource);
    if (!r || r->interface() != &kOutputInterface || !r->handler())
        return nullptr;
    return static_cast<Private::Handler *>(r->handler())->q;
}

/*!
    Returns the compositor.
*/
QWaylandCompositor *QWaylandOutput::compositor() const { return d->compositor; }

/*!
    Adds the output to \a compositor. Can be done once.
*/
void QWaylandOutput::setCompositor(QWaylandCompositor *compositor) {
    if (d->compositor == compositor)
        return;
    if (d->compositor) {
        qWarning("QWaylandOutput: the compositor cannot change");
        return;
    }
    d->compositor = compositor;
    if (compositor) {
        QWaylandCompositorPrivateAccess::addOutput(compositor, this);
        if (compositor->isCreated())
            initialize();
    }
    emit compositorChanged();
}

/*!
    Returns the window that shows the output, or \c nullptr.
*/
QWindow *QWaylandOutput::window() const { return d->window; }

/*!
    Sets the window that shows the output to \a window.
*/
void QWaylandOutput::setWindow(QWindow *window) {
    if (d->window == window)
        return;
    d->window = window;
    if (window)
        QObject::connect(window, &QObject::destroyed, this, [this] {
            d->window = nullptr;
            emit windowDestroyed();
        });
    emit windowChanged();
    update();
}

/*!
    Returns the manufacturer of the screen.
*/
QString QWaylandOutput::manufacturer() const { return d->manufacturer; }

/*!
    Sets the manufacturer of the screen to \a manufacturer.
*/
void QWaylandOutput::setManufacturer(const QString &manufacturer) {
    if (d->manufacturer == manufacturer)
        return;
    d->manufacturer = manufacturer;
    emit manufacturerChanged();
    d->sendAll();
}

/*!
    Returns the model of the screen.
*/
QString QWaylandOutput::model() const { return d->model; }

/*!
    Sets the model of the screen to \a model.
*/
void QWaylandOutput::setModel(const QString &model) {
    if (d->model == model)
        return;
    d->model = model;
    emit modelChanged();
    d->sendAll();
}

/*!
    Returns the position of the output in the compositor's space.
*/
QPoint QWaylandOutput::position() const { return d->position; }

/*!
    Moves the output to \a pt in the compositor's space.
*/
void QWaylandOutput::setPosition(const QPoint &pt) {
    if (d->position == pt)
        return;
    d->position = pt;
    emit positionChanged();
    emit geometryChanged();
    d->sendAll();
}

/*!
    Returns the modes, in the order they were added.
*/
QList<QWaylandOutputMode> QWaylandOutput::modes() const { return d->modes; }

/*!
    Adds \a mode. With \a preferred, it is the mode clients are told to prefer. The first mode added becomes the
    current one.
*/
void QWaylandOutput::addMode(const QWaylandOutputMode &mode, bool preferred) {
    if (!mode.isValid()) {
        qWarning("QWaylandOutput::addMode(): the mode is invalid");
        return;
    }
    d->modes.append(mode);
    if (preferred)
        d->preferredMode = static_cast<int>(d->modes.size()) - 1;
    emit modeAdded();
    if (d->currentMode < 0)
        setCurrentMode(mode);
    else
        d->sendAll();
}

/*!
    Returns the current mode, or an invalid one if no mode was added.
*/
QWaylandOutputMode QWaylandOutput::currentMode() const {
    return d->currentMode >= 0 ? d->modes.at(d->currentMode) : QWaylandOutputMode();
}

/*!
    Makes \a mode, which must have been added, the current one.
*/
void QWaylandOutput::setCurrentMode(const QWaylandOutputMode &mode) {
    const qsizetype index = d->modes.indexOf(mode);
    if (index < 0) {
        qWarning("QWaylandOutput::setCurrentMode(): the mode was not added");
        return;
    }
    if (d->currentMode == index)
        return;
    d->currentMode = static_cast<int>(index);
    emit currentModeChanged();
    emit geometryChanged();
    if (d->availableGeometry.isNull())
        emit availableGeometryChanged();
    d->sendAll();
}

/*!
    Returns the geometry: the position and the size of the current mode divided by the scale factor.
*/
QRect QWaylandOutput::geometry() const {
    return QRect(d->position, currentMode().size() / std::max(1, d->scaleFactor));
}

/*!
    Returns the part of the geometry that windows may use: what was set, or the whole geometry.
*/
QRect QWaylandOutput::availableGeometry() const {
    return d->availableGeometry.isNull() ? geometry() : d->availableGeometry;
}

/*!
    Sets the part of the geometry that windows may use to \a availableGeometry.
*/
void QWaylandOutput::setAvailableGeometry(const QRect &availableGeometry) {
    if (d->availableGeometry == availableGeometry)
        return;
    d->availableGeometry = availableGeometry;
    emit availableGeometryChanged();
}

/*!
    Returns the physical size of the screen in millimetres.
*/
QSize QWaylandOutput::physicalSize() const { return d->physicalSize; }

/*!
    Sets the physical size of the screen to \a size millimetres.
*/
void QWaylandOutput::setPhysicalSize(const QSize &size) {
    if (d->physicalSize == size)
        return;
    d->physicalSize = size;
    emit physicalSizeChanged();
    d->sendAll();
}

/*!
    Returns the subpixel layout.
*/
QWaylandOutput::Subpixel QWaylandOutput::subpixel() const { return d->subpixel; }

/*!
    Sets the subpixel layout to \a subpixel.
*/
void QWaylandOutput::setSubpixel(const Subpixel &subpixel) {
    if (d->subpixel == subpixel)
        return;
    d->subpixel = subpixel;
    emit subpixelChanged();
    d->sendAll();
}

/*!
    Returns the transform of the screen.
*/
QWaylandOutput::Transform QWaylandOutput::transform() const { return d->transform; }

/*!
    Sets the transform of the screen to \a transform.
*/
void QWaylandOutput::setTransform(const Transform &transform) {
    if (d->transform == transform)
        return;
    d->transform = transform;
    emit transformChanged();
    d->sendAll();
}

/*!
    Returns the scale factor: device pixels per unit of surface coordinates, 1 by default.
*/
int QWaylandOutput::scaleFactor() const { return d->scaleFactor; }

/*!
    Sets the scale factor to \a scale.
*/
void QWaylandOutput::setScaleFactor(int scale) {
    if (d->scaleFactor == scale || scale < 1)
        return;
    d->scaleFactor = scale;
    emit scaleFactorChanged();
    emit geometryChanged();
    d->sendAll();
}

/*!
    Returns whether the current mode follows the size of the window.
*/
bool QWaylandOutput::sizeFollowsWindow() const { return d->sizeFollowsWindow; }

/*!
    Sets whether the current mode follows the size of the window to \a follow.
*/
void QWaylandOutput::setSizeFollowsWindow(bool follow) {
    if (d->sizeFollowsWindow == follow)
        return;
    d->sizeFollowsWindow = follow;
    emit sizeFollowsWindowChanged();
    update();
}

/*!
    Returns whether the physical size follows the mode at 96 dots per inch.
*/
bool QWaylandOutput::physicalSizeFollowsSize() const { return d->physicalSizeFollowsSize; }

/*!
    Sets whether the physical size follows the mode at 96 dots per inch to \a follow.
*/
void QWaylandOutput::setPhysicalSizeFollowsSize(bool follow) {
    if (d->physicalSizeFollowsSize == follow)
        return;
    d->physicalSizeFollowsSize = follow;
    emit physicalSizeFollowsSizeChanged();
    update();
}

/*!
    Marks the start of a frame on this output.
*/
void QWaylandOutput::frameStarted() {
    if (!d->compositor)
        return;
    for (QWaylandSurface *s : d->compositor->surfaces())
        s->frameStarted();
}

/*!
    Answers the frame callbacks of every surface that is on this output (see surfaceEnter()), or of every
    surface when none has entered any output and this is the default output.
*/
void QWaylandOutput::sendFrameCallbacks() {
    if (!d->compositor)
        return;
    bool anyEntered = false;
    for (QWaylandSurface *s : d->compositor->surfaces()) {
        QWaylandSurface::Private *sd = QWaylandSurfacePrivateAccess::d(s);
        if (!sd->outputs.empty())
            anyEntered = true;
        if (sd->outputs.count(this) != 0)
            s->sendFrameCallbacks();
    }
    if (!anyEntered && d->compositor->defaultOutput() == this) {
        for (QWaylandSurface *s : d->compositor->surfaces())
            s->sendFrameCallbacks();
    }
}

/*!
    Tells the client of \a surface that the surface is now on this output.
*/
void QWaylandOutput::surfaceEnter(QWaylandSurface *surface) {
    if (surface)
        surface->sendEnter(this);
}

/*!
    Tells the client of \a surface that the surface left this output.
*/
void QWaylandOutput::surfaceLeave(QWaylandSurface *surface) {
    if (surface)
        surface->sendLeave(this);
}

/*!
    Brings the output up to date with its window: with sizeFollowsWindow(), a mode of the window's size becomes
    current; with physicalSizeFollowsSize(), the physical size is derived at 96 dots per inch.
*/
void QWaylandOutput::update() {
    if (d->window && d->sizeFollowsWindow) {
        const QWaylandOutputMode mode(d->window->size() * std::max(1, d->scaleFactor), 60000);
        if (!d->modes.contains(mode))
            addMode(mode);
        setCurrentMode(mode);
    }
    if (d->physicalSizeFollowsSize) {
        const QSize px = currentMode().size();
        setPhysicalSize(QSize(qRound(px.width() * 25.4 / 96.0), qRound(px.height() * 25.4 / 96.0)));
    }
}

/*!
    \reimp
*/
bool QWaylandOutput::event(QEvent *event) {
    if (event->type() == QEvent::User) {
        if (d->compositor && d->compositor->isCreated())
            initialize();
        return true;
    }
    return QWaylandObject::event(event);
}

/*!
    Announces the output to the clients. Called by the compositor when it is created, or when the output joins
    a created compositor.
*/
void QWaylandOutput::initialize() {
    if (d->initialized || !d->compositor || !d->compositor->isCreated())
        return;
    d->initialized = true;
    update();
    d->global =
        QWaylandCompositorPrivateAccess::display(d->compositor)
            ->addGlobal(&kOutputInterface, kOutputInterface.version, [this](Resource *r) {
                r->setHandler(d->handler.get());
                d->resources.push_back(r);
                d->sendGeometry(r);
                d->sendModes(r);
                if (r->version() >= 2) {
                    r->send(kOutputScale, {Arg::Int(d->scaleFactor)});
                }
                if (r->version() >= 4) {
                    r->send(kOutputName,
                            {Arg::String(d->model.isEmpty() ? QString::fromLatin1("Nt-%1").arg(d->global) : d->model)});
                    r->send(kOutputDescription, {Arg::String(d->manufacturer + QLatin1Char(' ') + d->model)});
                }
                d->sendDone(r);
            });
}

void QWaylandOutput::Private::sendGeometry(Resource *r) const {
    r->send(kOutputGeometry, {Arg::Int(position.x()), Arg::Int(position.y()), Arg::Int(physicalSize.width()),
                              Arg::Int(physicalSize.height()), Arg::Int(static_cast<int>(subpixel)),
                              Arg::String(manufacturer), Arg::String(model), Arg::Int(static_cast<int>(transform))});
}

void QWaylandOutput::Private::sendModes(Resource *r) const {
    for (int i = 0; i < modes.size(); ++i) {
        std::uint32_t flags = 0;
        if (i == currentMode)
            flags |= kOutputModeCurrent;
        if (i == preferredMode)
            flags |= kOutputModePreferred;
        const QWaylandOutputMode &m = modes.at(i);
        r->send(kOutputMode,
                {Arg::Uint(flags), Arg::Int(m.size().width()), Arg::Int(m.size().height()), Arg::Int(m.refreshRate())});
    }
}

void QWaylandOutput::Private::sendDone(Resource *r) const {
    if (r->version() >= 2)
        r->send(kOutputDone);
}

void QWaylandOutput::Private::sendAll() const {
    for (Resource *r : resources) {
        sendGeometry(r);
        sendModes(r);
        if (r->version() >= 2)
            r->send(kOutputScale, {Arg::Int(scaleFactor)});
        sendDone(r);
    }
}

std::vector<Resource *> QWaylandOutputPrivateAccess::resourcesFor(QWaylandOutput *o, QWaylandClient *client) {
    std::vector<Resource *> out;
    if (!client)
        return out;
    auto *connection = reinterpret_cast<Connection *>(client->client());
    for (Resource *r : o->d->resources) {
        if (r->connection() == connection)
            out.push_back(r);
    }
    return out;
}

#include "moc_qwaylandoutput.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
