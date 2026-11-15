// MIT License - see LICENSE
#include <QtWaylandCompositor/qwaylandseat.h>

#include "qwaylandcompositor_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qcoreevent.h>
#include <QtCore/qdebug.h>
#include <QtGui/qevent.h>
#include <QtWaylandCompositor/qwaylandclient.h>
#include <QtWaylandCompositor/qwaylandkeyboard.h>
#include <QtWaylandCompositor/qwaylandkeymap.h>
#include <QtWaylandCompositor/qwaylandpointer.h>
#include <QtWaylandCompositor/qwaylandsurface.h>
#include <QtWaylandCompositor/qwaylandtouch.h>
#include <QtWaylandCompositor/qwaylandview.h>

#include <algorithm>
#include <cstring>
#include <set>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#ifdef NT_HAVE_XKBCOMMON
#include <xkbcommon/xkbcommon.h>
#endif

using namespace QtWaylandPrivate;

namespace {

Connection *connectionOf(QWaylandClient *client) {
    return client ? reinterpret_cast<Connection *>(client->client()) : nullptr;
}

Connection *connectionOf(QWaylandSurface *surface) { return surface ? connectionOf(surface->client()) : nullptr; }

Connection *connectionOf(QWaylandView *view) { return view ? connectionOf(view->surface()) : nullptr; }

// The objects of one device interface (wl_pointer, wl_keyboard or wl_touch) that the clients created.
class DeviceHandler : public ResourceHandler {
public:
    std::vector<Resource *> resources;
    std::function<void(Resource *, std::uint32_t, const Args &)> onRequest;

    void request(Resource *resource, std::uint32_t opcode, const Args &args) override {
        if (onRequest)
            onRequest(resource, opcode, args);
    }
    void resourceDestroyed(Resource *resource) override {
        resources.erase(std::remove(resources.begin(), resources.end(), resource), resources.end());
    }
    void add(Resource *resource) {
        resource->setHandler(this);
        resources.push_back(resource);
    }
    // The objects of `connection`, which is where events for a client go.
    std::vector<Resource *> of(Connection *connection) const {
        std::vector<Resource *> out;
        for (Resource *r : resources) {
            if (r->connection() == connection)
                out.push_back(r);
        }
        return out;
    }
    void sendTo(Connection *connection, std::uint32_t opcode, const Args &args = Args()) {
        if (!connection)
            return;
        for (Resource *r : of(connection))
            r->send(opcode, args);
    }
};

// Linux input event codes.
constexpr std::uint32_t kBtnLeft = 0x110;
constexpr std::uint32_t kBtnRight = 0x111;
constexpr std::uint32_t kBtnMiddle = 0x112;
constexpr std::uint32_t kBtnForward = 0x115;
constexpr std::uint32_t kBtnBack = 0x116;

std::uint32_t buttonCode(Qt::MouseButton button) {
    switch (button) {
    case Qt::LeftButton:
        return kBtnLeft;
    case Qt::RightButton:
        return kBtnRight;
    case Qt::MiddleButton:
        return kBtnMiddle;
    case Qt::BackButton:
        return kBtnBack;
    case Qt::ForwardButton:
        return kBtnForward;
    default:
        return kBtnLeft;
    }
}

// Scan codes of the keys of a US keyboard, as evdev numbers them.
struct KeyCode {
    int qtKey;
    std::uint32_t code;
};
const KeyCode kKeyCodes[] = {
    {Qt::Key_Escape, 1},      {Qt::Key_1, 2},          {Qt::Key_2, 3},        {Qt::Key_3, 4},
    {Qt::Key_4, 5},           {Qt::Key_5, 6},          {Qt::Key_6, 7},        {Qt::Key_7, 8},
    {Qt::Key_8, 9},           {Qt::Key_9, 10},         {Qt::Key_0, 11},       {Qt::Key_Minus, 12},
    {Qt::Key_Equal, 13},      {Qt::Key_Backspace, 14}, {Qt::Key_Tab, 15},     {Qt::Key_Q, 16},
    {Qt::Key_W, 17},          {Qt::Key_E, 18},         {Qt::Key_R, 19},       {Qt::Key_T, 20},
    {Qt::Key_Y, 21},          {Qt::Key_U, 22},         {Qt::Key_I, 23},       {Qt::Key_O, 24},
    {Qt::Key_P, 25},          {0x5b, 26} /* [ */,      {0x5d, 27} /* ] */,    {Qt::Key_Return, 28},
    {Qt::Key_Control, 29},    {Qt::Key_A, 30},         {Qt::Key_S, 31},       {Qt::Key_D, 32},
    {Qt::Key_F, 33},          {Qt::Key_G, 34},         {Qt::Key_H, 35},       {Qt::Key_J, 36},
    {Qt::Key_K, 37},          {Qt::Key_L, 38},         {0x3b, 39} /* ; */,    {0x27, 40} /* ' */,
    {0x60, 41} /* ` */,       {Qt::Key_Shift, 42},     {0x5c, 43} /* \\ */,   {Qt::Key_Z, 44},
    {Qt::Key_X, 45},          {Qt::Key_C, 46},         {Qt::Key_V, 47},       {Qt::Key_B, 48},
    {Qt::Key_N, 49},          {Qt::Key_M, 50},         {Qt::Key_Comma, 51},   {Qt::Key_Period, 52},
    {Qt::Key_Slash, 53},      {0x2a, 55} /* * */,      {Qt::Key_Alt, 56},     {Qt::Key_Space, 57},
    {Qt::Key_CapsLock, 58},   {Qt::Key_F1, 59},        {Qt::Key_F2, 60},      {Qt::Key_F3, 61},
    {Qt::Key_F4, 62},         {Qt::Key_F5, 63},        {Qt::Key_F6, 64},      {Qt::Key_F7, 65},
    {Qt::Key_F8, 66},         {Qt::Key_F9, 67},        {Qt::Key_F10, 68},     {Qt::Key_NumLock, 69},
    {Qt::Key_ScrollLock, 70}, {Qt::Key_F11, 87},       {Qt::Key_F12, 88},     {Qt::Key_Enter, 96},
    {Qt::Key_Home, 102},      {Qt::Key_Up, 103},       {Qt::Key_PageUp, 104}, {Qt::Key_Left, 105},
    {Qt::Key_Right, 106},     {Qt::Key_End, 107},      {Qt::Key_Down, 108},   {Qt::Key_PageDown, 109},
    {Qt::Key_Insert, 110},    {Qt::Key_Delete, 111},   {Qt::Key_Pause, 119},  {Qt::Key_Meta, 125},
    {Qt::Key_Menu, 127},      {Qt::Key_Print, 99},
};

// xkb modifier bits, as the US keymap lays them out.
constexpr std::uint32_t kModShift = 1u << 0;
constexpr std::uint32_t kModLock = 1u << 1;
constexpr std::uint32_t kModControl = 1u << 2;
constexpr std::uint32_t kModAlt = 1u << 3;
constexpr std::uint32_t kModSuper = 1u << 6;

// Writes `data` into a sealed memory file and returns its descriptor, or -1.
int memoryFile(const QByteArray &data) {
    const int fd = ::memfd_create("nt-keymap", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0)
        return -1;
    if (::ftruncate(fd, data.size()) < 0) {
        ::close(fd);
        return -1;
    }
    if (!data.isEmpty()) {
        void *p = ::mmap(nullptr, static_cast<std::size_t>(data.size()), PROT_WRITE, MAP_SHARED, fd, 0);
        if (p == MAP_FAILED) {
            ::close(fd);
            return -1;
        }
        std::memcpy(p, data.constData(), static_cast<std::size_t>(data.size()));
        ::munmap(p, static_cast<std::size_t>(data.size()));
    }
    ::fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE | F_SEAL_SEAL);
    return fd;
}

} // namespace

// ---- private parts of the devices ----------------------------------------------------------------------------

struct QWaylandPointer::Private {
    QWaylandSeat *seat = nullptr;
    QPointer<QWaylandOutput> output;
    DeviceHandler handler;
    QPointer<QWaylandView> focus;
    QPointF localPosition;
    QPointF spacePosition;
    std::set<Qt::MouseButton> pressed;
    std::uint32_t enterSerial = 0;
};

struct QWaylandKeyboard::Private {
    QWaylandSeat *seat = nullptr;
    DeviceHandler handler;
    QPointer<QWaylandSurface> focus;
    quint32 repeatRate = 40;
    quint32 repeatDelay = 400;
    std::set<std::uint32_t> keysDown;
    std::uint32_t modsDepressed = 0;
    std::uint32_t modsLatched = 0;
    std::uint32_t modsLocked = 0;
    std::uint32_t group = 0;
    QByteArray keymapText;
    std::uint32_t keymapFormat = kKeymapFormatNone;
#ifdef NT_HAVE_XKBCOMMON
    xkb_context *xkbContext = nullptr;
    xkb_keymap *xkbKeymap = nullptr;
    xkb_state *xkbState = nullptr;
#endif
    void buildKeymap(const QWaylandKeymap *keymap);
    void sendKeymap(Resource *r) const;
    void updateModifiers(std::uint32_t code, bool pressed);
};

struct QWaylandTouch::Private {
    QWaylandSeat *seat = nullptr;
    DeviceHandler handler;
    QPointer<QWaylandSurface> focus;
};

class QWaylandSeat::Private::Handler : public ResourceHandler {
public:
    explicit Handler(QWaylandSeat *seat) : q(seat) {}
    void request(Resource *resource, std::uint32_t opcode, const Args &args) override {
        Private *d = q->d.get();
        switch (opcode) {
        case kSeatGetPointer:
            if (Resource *r = resource->connection()->resource(args[0].u)) {
                if (d->pointer)
                    d->pointer->d->handler.add(r);
            }
            break;
        case kSeatGetKeyboard:
            if (Resource *r = resource->connection()->resource(args[0].u)) {
                if (d->keyboard) {
                    d->keyboard->d->handler.add(r);
                    d->keyboard->d->sendKeymap(r);
                    if (r->version() >= 4)
                        r->send(kKeyboardRepeatInfo, {Arg::Int(static_cast<int>(d->keyboard->d->repeatRate)),
                                                      Arg::Int(static_cast<int>(d->keyboard->d->repeatDelay))});
                }
            }
            break;
        case kSeatGetTouch:
            if (Resource *r = resource->connection()->resource(args[0].u)) {
                if (d->touch)
                    d->touch->d->handler.add(r);
            }
            break;
        case kSeatRelease:
            resource->destroy();
            break;
        default:
            break;
        }
    }
    void resourceDestroyed(Resource *) override {}
    QWaylandSeat *q;
};

// ---- QWaylandSeat --------------------------------------------------------------------------------------------

/*!
    \class QWaylandSeat
    \brief The QWaylandSeat class is a set of input devices: a pointer, a keyboard and a touch screen.

    The application feeds the seat with what its own input devices report, and the seat sends the protocol events
    to the client that has the focus: setMouseFocus() or sendMouseMoveEvent() for the pointer, setKeyboardFocus()
    for the keyboard, and the surface given to each touch event for touch.

    \code
    seat->sendMouseMoveEvent(view, localPos);
    seat->sendMousePressEvent(Qt::LeftButton);
    seat->sendMouseReleaseEvent(Qt::LeftButton);
    seat->setKeyboardFocus(surface);
    seat->sendKeyEvent(Qt::Key_A, true);
    \endcode

    \differences
    Key codes are those of a US keyboard, and the keymap sent to clients is built with xkbcommon when Nt was built
    with it (otherwise clients get no keymap and interpret the codes themselves). There is no drag and drop
    (drag() returns \c nullptr), no \c sendFullTouchEvent() as Nt has no QTouchEvent, and no text input.
    \enddifferences

    \sa QWaylandPointer, QWaylandKeyboard, QWaylandTouch
*/

/*!
    \enum QWaylandSeat::CapabilityFlag

    The devices a seat has.

    \value Pointer A mouse or similar.
    \value Keyboard A keyboard.
    \value Touch A touch screen.
    \value DefaultCapabilities All three.
*/

/*!
    Constructs a seat of \a compositor with the devices \a capabilityFlags. The compositor adds it to its seats.
*/
QWaylandSeat::QWaylandSeat(QWaylandCompositor *compositor, CapabilityFlags capabilityFlags)
    : QWaylandObject(compositor), d(std::make_unique<Private>()) {
    d->compositor = compositor;
    d->capabilities = capabilityFlags;
    d->handler = std::make_unique<Private::Handler>(this);
    if (capabilityFlags & Pointer)
        d->pointer = new QWaylandPointer(this, this);
    if (capabilityFlags & Keyboard)
        d->keyboard = new QWaylandKeyboard(this, this);
    if (capabilityFlags & Touch)
        d->touch = new QWaylandTouch(this, this);
    if (compositor)
        QWaylandCompositorPrivateAccess::addSeat(compositor, this);
    // initialize() is virtual, so it waits for the event loop rather than running from the constructor.
    QCoreApplication::postEvent(this, new QEvent(QEvent::User));
}

/*!
    Destroys the seat; clients are told it is gone.
*/
QWaylandSeat::~QWaylandSeat() {
    if (d->compositor) {
        if (d->global)
            QWaylandCompositorPrivateAccess::display(d->compositor)->removeGlobal(d->global);
        QWaylandCompositorPrivateAccess::removeSeat(d->compositor, this);
    }
}

/*!
    Announces the seat to the clients. The compositor calls it when it is created.
*/
void QWaylandSeat::initialize() {
    if (d->initialized || !d->compositor || !d->compositor->isCreated())
        return;
    d->initialized = true;
    Display *display = QWaylandCompositorPrivateAccess::display(d->compositor);
    display->registerInterface(&kPointerInterface);
    display->registerInterface(&kKeyboardInterface);
    display->registerInterface(&kTouchInterface);
    if (d->keyboard)
        d->keyboard->d->buildKeymap(keymap());
    d->global = display->addGlobal(&kSeatInterface, kSeatInterface.version, [this](Resource *r) {
        r->setHandler(d->handler.get());
        r->send(kSeatCapabilities, {Arg::Uint(static_cast<std::uint32_t>(d->capabilities.toInt()))});
        if (r->version() >= 2)
            r->send(kSeatName, {Arg::String(QByteArray("seat0"))});
    });
}

/*!
    Returns \c true once initialize() has run.
*/
bool QWaylandSeat::isInitialized() const { return d->initialized; }

/*!
    \reimp
*/
bool QWaylandSeat::event(QEvent *event) {
    if (event->type() == QEvent::User) {
        if (d->compositor && d->compositor->isCreated())
            initialize();
        return true;
    }
    return QWaylandObject::event(event);
}

/*!
    Sends a press of \a button to the client with the mouse focus.
*/
void QWaylandSeat::sendMousePressEvent(Qt::MouseButton button) {
    if (d->pointer)
        d->pointer->sendMousePressEvent(button);
}

/*!
    Sends a release of \a button to the client with the mouse focus.
*/
void QWaylandSeat::sendMouseReleaseEvent(Qt::MouseButton button) {
    if (d->pointer)
        d->pointer->sendMouseReleaseEvent(button);
}

/*!
    Moves the pointer to \a localPos in \a surface (a view), which gets the mouse focus if it had not; the
    position is \a outputSpacePos in the output.
*/
void QWaylandSeat::sendMouseMoveEvent(QWaylandView *surface, const QPointF &localPos, const QPointF &outputSpacePos) {
    if (d->pointer)
        d->pointer->sendMouseMoveEvent(surface, localPos, outputSpacePos);
}

/*!
    Sends a wheel turn of \a delta (120 per notch, as QWheelEvent counts) along \a orientation.
*/
void QWaylandSeat::sendMouseWheelEvent(Qt::Orientation orientation, int delta) {
    if (d->pointer)
        d->pointer->sendMouseWheelEvent(orientation, delta);
}

/*!
    Sends a press of the key with the scan \a code to the client with the keyboard focus.
*/
void QWaylandSeat::sendKeyPressEvent(uint code) {
    if (d->keyboard)
        d->keyboard->sendKeyPressEvent(code);
}

/*!
    Sends a release of the key with the scan \a code to the client with the keyboard focus.
*/
void QWaylandSeat::sendKeyReleaseEvent(uint code) {
    if (d->keyboard)
        d->keyboard->sendKeyReleaseEvent(code);
}

/*!
    Sends the key of \a event, pressed or released as its type says.
*/
void QWaylandSeat::sendFullKeyEvent(QKeyEvent *event) {
    if (!event || !d->keyboard)
        return;
    if (event->type() == QEvent::KeyPress)
        sendKeyEvent(event->key(), true);
    else if (event->type() == QEvent::KeyRelease)
        sendKeyEvent(event->key(), false);
}

/*!
    Sends \a qtKey (a Qt::Key value), \a pressed or released, as the scan code of a US keyboard.
*/
void QWaylandSeat::sendKeyEvent(int qtKey, bool pressed) {
    if (!d->keyboard)
        return;
    const uint code = d->keyboard->keyToScanCode(qtKey);
    if (code == 0) {
        qWarning("QWaylandSeat::sendKeyEvent(): no scan code for Qt key 0x%x", qtKey);
        return;
    }
    if (pressed)
        d->keyboard->sendKeyPressEvent(code);
    else
        d->keyboard->sendKeyReleaseEvent(code);
}

/*!
    Sends a press of the key that types the character \a unicode, if a US keyboard has one.
*/
void QWaylandSeat::sendUnicodeKeyPressEvent(uint unicode) {
    if (unicode < 0x80)
        sendKeyEvent(static_cast<int>(unicode), true);
}

/*!
    Sends a release of the key that types the character \a unicode.
*/
void QWaylandSeat::sendUnicodeKeyReleaseEvent(uint unicode) {
    if (unicode < 0x80)
        sendKeyEvent(static_cast<int>(unicode), false);
}

/*!
    Sends the touch point \a id at \a point in \a surface with \a state. Returns the serial of the event.
*/
uint QWaylandSeat::sendTouchPointEvent(QWaylandSurface *surface, int id, const QPointF &point,
                                       Qt::TouchPointState state) {
    return d->touch ? d->touch->sendTouchPointEvent(surface, id, point, state) : 0;
}

/*!
    Sends a press of the touch point \a id at \a position in \a surface. Returns the serial.
*/
uint QWaylandSeat::sendTouchPointPressed(QWaylandSurface *surface, int id, const QPointF &position) {
    return sendTouchPointEvent(surface, id, position, Qt::TouchPointPressed);
}

/*!
    Sends a release of the touch point \a id at \a position in \a surface. Returns the serial.
*/
uint QWaylandSeat::sendTouchPointReleased(QWaylandSurface *surface, int id, const QPointF &position) {
    return sendTouchPointEvent(surface, id, position, Qt::TouchPointReleased);
}

/*!
    Sends a move of the touch point \a id to \a position in \a surface. Returns the serial.
*/
uint QWaylandSeat::sendTouchPointMoved(QWaylandSurface *surface, int id, const QPointF &position) {
    return sendTouchPointEvent(surface, id, position, Qt::TouchPointMoved);
}

/*!
    Tells \a client that the touch points sent so far form one frame.
*/
void QWaylandSeat::sendTouchFrameEvent(QWaylandClient *client) {
    if (d->touch)
        d->touch->sendFrameEvent(client);
}

/*!
    Tells \a client that the touch sequence was cancelled.
*/
void QWaylandSeat::sendTouchCancelEvent(QWaylandClient *client) {
    if (d->touch)
        d->touch->sendCancelEvent(client);
}

/*!
    Returns the pointer, or \c nullptr if the seat has none.
*/
QWaylandPointer *QWaylandSeat::pointer() const { return d->pointer; }

/*!
    Returns the keyboard, or \c nullptr if the seat has none.
*/
QWaylandKeyboard *QWaylandSeat::keyboard() const { return d->keyboard; }

/*!
    Returns the touch device, or \c nullptr if the seat has none.
*/
QWaylandTouch *QWaylandSeat::touch() const { return d->touch; }

/*!
    Returns the view the pointer is in.
*/
QWaylandView *QWaylandSeat::mouseFocus() const { return d->pointer ? d->pointer->mouseFocus() : nullptr; }

/*!
    Moves the pointer into \a view: its client gets an enter event, the previous one a leave.
*/
void QWaylandSeat::setMouseFocus(QWaylandView *view) {
    if (!d->pointer)
        return;
    QWaylandView *old = d->pointer->mouseFocus();
    if (old == view)
        return;
    d->pointer->sendEnter(view, d->pointer->currentLocalPosition());
    emit mouseFocusChanged(view, old);
}

/*!
    Returns the surface that has the keyboard focus.
*/
QWaylandSurface *QWaylandSeat::keyboardFocus() const { return d->keyboardFocus; }

/*!
    Gives \a surface the keyboard focus. Returns \c false if the seat has no keyboard.
*/
bool QWaylandSeat::setKeyboardFocus(QWaylandSurface *surface) {
    if (!d->keyboard)
        return false;
    QWaylandSurface *old = d->keyboardFocus;
    if (old == surface)
        return true;
    d->keyboardFocus = surface;
    d->keyboard->setFocus(surface);
    emit keyboardFocusChanged(surface, old);
    return true;
}

/*!
    Returns the keymap of the keyboard, made on first use with the US layout.
*/
QWaylandKeymap *QWaylandSeat::keymap() {
    if (!d->keymap)
        d->keymap = new QWaylandKeymap(QString::fromLatin1("us"), QString(), QString(), QString(), QString(), this);
    return d->keymap;
}

/*!
    Returns the drag in progress: always \c nullptr, as Nt has no drag and drop.
*/
QWaylandDrag *QWaylandSeat::drag() const { return nullptr; }

/*!
    Returns the compositor.
*/
QWaylandCompositor *QWaylandSeat::compositor() const { return d->compositor; }

/*!
    Returns the devices the seat has.
*/
QWaylandSeat::CapabilityFlags QWaylandSeat::capabilities() const { return d->capabilities; }

/*!
    Returns \c true if \a inputEvent came from this seat's devices. Nt has one seat per device set, so this is
    \c true for every event.
*/
bool QWaylandSeat::isOwner(QInputEvent *inputEvent) const {
    (void)inputEvent;
    return true;
}

/*!
    Returns the seat behind the opaque handle \a resource, or \c nullptr.
*/
QWaylandSeat *QWaylandSeat::fromSeatResource(void *resource) {
    auto *r = static_cast<Resource *>(resource);
    if (!r || r->interface() != &kSeatInterface || !r->handler())
        return nullptr;
    return static_cast<Private::Handler *>(r->handler())->q;
}

// ---- QWaylandPointer -----------------------------------------------------------------------------------------

/*!
    \class QWaylandPointer
    \brief The QWaylandPointer class sends mouse events to the client with the mouse focus.

    \differences
    Wheel events carry the axis value, discrete steps and, for clients of \c wl_pointer version 8 and later, the
    120ths of a notch. There is no cursor surface tracking beyond QWaylandSeat::cursorSurfaceRequested().
    \enddifferences

    \sa QWaylandSeat
*/

/*!
    Constructs the pointer of \a seat with the given \a parent.
*/
QWaylandPointer::QWaylandPointer(QWaylandSeat *seat, QObject *parent)
    : QWaylandObject(parent), d(std::make_unique<Private>()) {
    d->seat = seat;
    d->handler.onRequest = [this](Resource *resource, std::uint32_t opcode, const Args &args) {
        if (opcode == kPointerSetCursor) {
            QWaylandSurface *surface =
                args[1].null ? nullptr
                             : QWaylandSurfacePrivateAccess::fromResource(resource->connection()->resource(args[1].u));
            if (surface)
                surface->markAsCursorSurface(true);
            QWaylandClient *client =
                QWaylandCompositorPrivateAccess::clientFor(d->seat->compositor(), resource->connection());
            emit d->seat->cursorSurfaceRequested(surface, args[2].i, args[3].i, client);
        } else if (opcode == kPointerRelease) {
            resource->destroy();
        }
    };
}

/*!
    Destroys the pointer.
*/
QWaylandPointer::~QWaylandPointer() = default;

/*!
    Returns the seat.
*/
QWaylandSeat *QWaylandPointer::seat() const { return d->seat; }

/*!
    Returns the compositor.
*/
QWaylandCompositor *QWaylandPointer::compositor() const { return d->seat->compositor(); }

/*!
    Returns the output the pointer is on.
*/
QWaylandOutput *QWaylandPointer::output() const { return d->output; }

/*!
    Puts the pointer on \a output.
*/
void QWaylandPointer::setOutput(QWaylandOutput *output) {
    if (d->output == output)
        return;
    d->output = output;
    emit outputChanged();
}

/*!
    Sends a press of \a button to the client with the mouse focus. Returns the serial of the event.
*/
uint QWaylandPointer::sendMousePressEvent(Qt::MouseButton button) {
    Connection *c = connectionOf(d->focus);
    const std::uint32_t serial = compositor()->nextSerial();
    const bool wasPressed = !d->pressed.empty();
    d->pressed.insert(button);
    if (c) {
        d->handler.sendTo(
            c, kPointerButton,
            {Arg::Uint(serial), Arg::Uint(Display::timestamp()), Arg::Uint(buttonCode(button)), Arg::Uint(1)});
        d->handler.sendTo(c, kPointerFrame);
    }
    if (!wasPressed)
        emit buttonPressedChanged();
    return serial;
}

/*!
    Sends a release of \a button to the client with the mouse focus. Returns the serial of the event.
*/
uint QWaylandPointer::sendMouseReleaseEvent(Qt::MouseButton button) {
    Connection *c = connectionOf(d->focus);
    const std::uint32_t serial = compositor()->nextSerial();
    d->pressed.erase(button);
    if (c) {
        d->handler.sendTo(
            c, kPointerButton,
            {Arg::Uint(serial), Arg::Uint(Display::timestamp()), Arg::Uint(buttonCode(button)), Arg::Uint(0)});
        d->handler.sendTo(c, kPointerFrame);
    }
    if (d->pressed.empty())
        emit buttonPressedChanged();
    return serial;
}

/*!
    Moves the pointer to \a localPos in \a view, at \a outputSpacePos on the output. The view gets the mouse
    focus first if it did not have it.
*/
void QWaylandPointer::sendMouseMoveEvent(QWaylandView *view, const QPointF &localPos, const QPointF &outputSpacePos) {
    d->spacePosition = outputSpacePos;
    if (view != d->focus) {
        QWaylandView *old = d->focus;
        sendEnter(view, localPos);
        emit d->seat->mouseFocusChanged(view, old);
        return;
    }
    d->localPosition = localPos;
    if (Connection *c = connectionOf(view)) {
        d->handler.sendTo(c, kPointerMotion,
                          {Arg::Uint(Display::timestamp()), Arg::Fixed(localPos.x()), Arg::Fixed(localPos.y())});
        d->handler.sendTo(c, kPointerFrame);
    }
}

/*!
    Sends a wheel turn of \a delta (120 per notch) along \a orientation to the client with the mouse focus.
*/
void QWaylandPointer::sendMouseWheelEvent(Qt::Orientation orientation, int delta) {
    Connection *c = connectionOf(d->focus);
    if (!c || delta == 0)
        return;
    const std::uint32_t axis = orientation == Qt::Vertical ? 0 : 1;
    const std::uint32_t time = Display::timestamp();
    for (Resource *r : d->handler.of(c)) {
        if (r->version() >= 8)
            r->send(kPointerAxisValue120, {Arg::Uint(axis), Arg::Int(-delta)});
        else if (r->version() >= 5)
            r->send(kPointerAxisDiscrete, {Arg::Uint(axis), Arg::Int(-delta / 120)});
        r->send(kPointerAxis, {Arg::Uint(time), Arg::Uint(axis), Arg::Fixed(-delta / 12.0)});
        r->send(kPointerFrame);
    }
}

/*!
    Returns the view the pointer is in.
*/
QWaylandView *QWaylandPointer::mouseFocus() const { return d->focus; }

/*!
    Returns the position of the pointer in the view it is in.
*/
QPointF QWaylandPointer::currentLocalPosition() const { return d->localPosition; }

/*!
    Returns the position of the pointer on the output.
*/
QPointF QWaylandPointer::currentSpacePosition() const { return d->spacePosition; }

/*!
    Returns \c true while a button is down.
*/
bool QWaylandPointer::isButtonPressed() const { return !d->pressed.empty(); }

/*!
    Moves the pointer into \a view at \a localPos: a leave event to the previous client, an enter event to the
    new one. A null \a view leaves without entering.
*/
void QWaylandPointer::sendEnter(QWaylandView *view, const QPointF &localPos) {
    if (d->focus) {
        if (Connection *c = connectionOf(d->focus)) {
            Resource *surface = QWaylandSurfacePrivateAccess::d(d->focus->surface())->resource;
            if (surface) {
                d->handler.sendTo(c, kPointerLeave, {Arg::Uint(compositor()->nextSerial()), Arg::Object(surface)});
                d->handler.sendTo(c, kPointerFrame);
            }
        }
    }
    d->focus = view;
    d->localPosition = localPos;
    if (view && view->surface()) {
        if (Connection *c = connectionOf(view)) {
            Resource *surface = QWaylandSurfacePrivateAccess::d(view->surface())->resource;
            if (surface) {
                d->enterSerial = compositor()->nextSerial();
                d->handler.sendTo(c, kPointerEnter,
                                  {Arg::Uint(d->enterSerial), Arg::Object(surface), Arg::Fixed(localPos.x()),
                                   Arg::Fixed(localPos.y())});
                d->handler.sendTo(c, kPointerFrame);
            }
        }
    }
}

/*!
    Takes the pointer out of the view it is in.
*/
void QWaylandPointer::sendLeave() { sendEnter(nullptr, QPointF()); }

// ---- QWaylandKeyboard ----------------------------------------------------------------------------------------

/*!
    \class QWaylandKeyboard
    \brief The QWaylandKeyboard class sends key events to the surface with the keyboard focus.

    Keys are sent as scan codes (the evdev numbers); keyToScanCode() maps a Qt::Key to the code of a US keyboard.
    When Nt is built with xkbcommon, clients get an XKB keymap made from the seat's QWaylandKeymap and the
    modifier state is tracked with it; otherwise they get no keymap and the modifiers are tracked from the
    modifier keys themselves.

    \differences
    See QWaylandSeat.
    \enddifferences
*/

/*!
    Constructs the keyboard of \a seat with the given \a parent.
*/
QWaylandKeyboard::QWaylandKeyboard(QWaylandSeat *seat, QObject *parent)
    : QWaylandObject(parent), d(std::make_unique<Private>()) {
    d->seat = seat;
    d->handler.onRequest = [](Resource *resource, std::uint32_t opcode, const Args &) {
        if (opcode == kKeyboardRelease)
            resource->destroy();
    };
}

/*!
    Destroys the keyboard.
*/
QWaylandKeyboard::~QWaylandKeyboard() {
#ifdef NT_HAVE_XKBCOMMON
    if (d->xkbState)
        xkb_state_unref(d->xkbState);
    if (d->xkbKeymap)
        xkb_keymap_unref(d->xkbKeymap);
    if (d->xkbContext)
        xkb_context_unref(d->xkbContext);
#endif
}

/*!
    Returns the seat.
*/
QWaylandSeat *QWaylandKeyboard::seat() const { return d->seat; }

/*!
    Returns the compositor.
*/
QWaylandCompositor *QWaylandKeyboard::compositor() const { return d->seat->compositor(); }

/*!
    Returns the key repeat rate told to clients, in keys per second; 40 by default.
*/
quint32 QWaylandKeyboard::repeatRate() const { return d->repeatRate; }

/*!
    Sets the key repeat rate told to clients to \a rate keys per second.
*/
void QWaylandKeyboard::setRepeatRate(quint32 rate) {
    if (d->repeatRate == rate)
        return;
    d->repeatRate = rate;
    for (Resource *r : d->handler.resources) {
        if (r->version() >= 4)
            r->send(kKeyboardRepeatInfo,
                    {Arg::Int(static_cast<int>(d->repeatRate)), Arg::Int(static_cast<int>(d->repeatDelay))});
    }
    emit repeatRateChanged(rate);
}

/*!
    Returns the delay before keys repeat, in milliseconds; 400 by default.
*/
quint32 QWaylandKeyboard::repeatDelay() const { return d->repeatDelay; }

/*!
    Sets the delay before keys repeat to \a delay milliseconds.
*/
void QWaylandKeyboard::setRepeatDelay(quint32 delay) {
    if (d->repeatDelay == delay)
        return;
    d->repeatDelay = delay;
    for (Resource *r : d->handler.resources) {
        if (r->version() >= 4)
            r->send(kKeyboardRepeatInfo,
                    {Arg::Int(static_cast<int>(d->repeatRate)), Arg::Int(static_cast<int>(d->repeatDelay))});
    }
    emit repeatDelayChanged(delay);
}

/*!
    Gives \a surface the keyboard focus: a leave event to the previous client, an enter event with the keys that
    are down and the modifiers to the new one.
*/
void QWaylandKeyboard::setFocus(QWaylandSurface *surface) {
    if (d->focus == surface)
        return;
    if (d->focus) {
        Resource *res = QWaylandSurfacePrivateAccess::d(d->focus)->resource;
        if (res)
            d->handler.sendTo(connectionOf(d->focus), kKeyboardLeave,
                              {Arg::Uint(compositor()->nextSerial()), Arg::Object(res)});
    }
    d->focus = surface;
    if (surface) {
        Resource *res = QWaylandSurfacePrivateAccess::d(surface)->resource;
        if (res) {
            QByteArray keys;
            for (std::uint32_t code : d->keysDown) {
                keys.append(reinterpret_cast<const char *>(&code), 4);
            }
            const std::uint32_t serial = compositor()->nextSerial();
            d->handler.sendTo(connectionOf(surface), kKeyboardEnter,
                              {Arg::Uint(serial), Arg::Object(res), Arg::Array(keys)});
            sendKeyModifiers(surface->client(), serial);
        }
    }
    emit focusChanged(surface);
}

/*!
    Sends the modifier state to \a client with \a serial.
*/
void QWaylandKeyboard::sendKeyModifiers(QWaylandClient *client, uint serial) {
    d->handler.sendTo(connectionOf(client), kKeyboardModifiers,
                      {Arg::Uint(serial), Arg::Uint(d->modsDepressed), Arg::Uint(d->modsLatched),
                       Arg::Uint(d->modsLocked), Arg::Uint(d->group)});
}

/*!
    Sends a press of the key with the scan \a code to the surface with the focus.
*/
void QWaylandKeyboard::sendKeyPressEvent(uint code) {
    d->keysDown.insert(code);
    const std::uint32_t before = d->modsDepressed | d->modsLocked;
    d->updateModifiers(code, true);
    const std::uint32_t serial = compositor()->nextSerial();
    Connection *c = connectionOf(d->focus);
    d->handler.sendTo(c, kKeyboardKey,
                      {Arg::Uint(serial), Arg::Uint(Display::timestamp()), Arg::Uint(code), Arg::Uint(1)});
    if ((d->modsDepressed | d->modsLocked) != before && d->focus)
        sendKeyModifiers(d->focus->client(), serial);
}

/*!
    Sends a release of the key with the scan \a code to the surface with the focus.
*/
void QWaylandKeyboard::sendKeyReleaseEvent(uint code) {
    d->keysDown.erase(code);
    const std::uint32_t before = d->modsDepressed | d->modsLocked;
    d->updateModifiers(code, false);
    const std::uint32_t serial = compositor()->nextSerial();
    Connection *c = connectionOf(d->focus);
    d->handler.sendTo(c, kKeyboardKey,
                      {Arg::Uint(serial), Arg::Uint(Display::timestamp()), Arg::Uint(code), Arg::Uint(0)});
    if ((d->modsDepressed | d->modsLocked) != before && d->focus)
        sendKeyModifiers(d->focus->client(), serial);
}

/*!
    Returns the scan code of \a qtKey on a US keyboard, or 0 if it has none.
*/
uint QWaylandKeyboard::keyToScanCode(int qtKey) const {
    for (const KeyCode &k : kKeyCodes) {
        if (k.qtKey == qtKey)
            return k.code;
    }
    if (qtKey >= 'a' && qtKey <= 'z')
        return keyToScanCode(qtKey - 0x20);
    return 0;
}

/*!
    Returns the surface with the keyboard focus.
*/
QWaylandSurface *QWaylandKeyboard::focus() const { return d->focus; }

/*!
    Returns the client of the surface with the keyboard focus.
*/
QWaylandClient *QWaylandKeyboard::focusClient() const { return d->focus ? d->focus->client() : nullptr; }

void QWaylandKeyboard::Private::buildKeymap(const QWaylandKeymap *keymap) {
    keymapText.clear();
    keymapFormat = kKeymapFormatNone;
#ifdef NT_HAVE_XKBCOMMON
    if (!xkbContext)
        xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!xkbContext)
        return;
    const QByteArray layout = keymap ? keymap->layout().toUtf8() : QByteArray("us");
    const QByteArray variant = keymap ? keymap->variant().toUtf8() : QByteArray();
    const QByteArray options = keymap ? keymap->options().toUtf8() : QByteArray();
    const QByteArray model = keymap ? keymap->model().toUtf8() : QByteArray();
    const QByteArray rules = keymap ? keymap->rules().toUtf8() : QByteArray();
    xkb_rule_names names{};
    names.rules = rules.isEmpty() ? nullptr : rules.constData();
    names.model = model.isEmpty() ? nullptr : model.constData();
    names.layout = layout.isEmpty() ? nullptr : layout.constData();
    names.variant = variant.isEmpty() ? nullptr : variant.constData();
    names.options = options.isEmpty() ? nullptr : options.constData();
    if (xkbState)
        xkb_state_unref(xkbState);
    if (xkbKeymap)
        xkb_keymap_unref(xkbKeymap);
    xkbKeymap = xkb_keymap_new_from_names(xkbContext, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
    xkbState = xkbKeymap ? xkb_state_new(xkbKeymap) : nullptr;
    if (xkbKeymap) {
        char *text = xkb_keymap_get_as_string(xkbKeymap, XKB_KEYMAP_FORMAT_TEXT_V1);
        if (text) {
            keymapText = QByteArray(text);
            keymapText.append('\0');
            std::free(text);
            keymapFormat = kKeymapFormatXkbV1;
        }
    }
#else
    (void)keymap;
#endif
}

void QWaylandKeyboard::Private::sendKeymap(Resource *r) const {
    const int fd = memoryFile(keymapText);
    if (fd < 0)
        return;
    r->send(kKeyboardKeymap,
            {Arg::Uint(keymapFormat), Arg::Fd(fd), Arg::Uint(static_cast<std::uint32_t>(keymapText.size()))});
    ::close(fd);
}

void QWaylandKeyboard::Private::updateModifiers(std::uint32_t code, bool pressed) {
#ifdef NT_HAVE_XKBCOMMON
    if (xkbState) {
        xkb_state_update_key(xkbState, code + 8, pressed ? XKB_KEY_DOWN : XKB_KEY_UP);
        modsDepressed = xkb_state_serialize_mods(xkbState, XKB_STATE_MODS_DEPRESSED);
        modsLatched = xkb_state_serialize_mods(xkbState, XKB_STATE_MODS_LATCHED);
        modsLocked = xkb_state_serialize_mods(xkbState, XKB_STATE_MODS_LOCKED);
        group = xkb_state_serialize_layout(xkbState, XKB_STATE_LAYOUT_EFFECTIVE);
        return;
    }
#endif
    std::uint32_t bit = 0;
    switch (code) {
    case 42:
    case 54:
        bit = kModShift;
        break;
    case 29:
    case 97:
        bit = kModControl;
        break;
    case 56:
    case 100:
        bit = kModAlt;
        break;
    case 125:
    case 126:
        bit = kModSuper;
        break;
    case 58:
        if (pressed)
            modsLocked ^= kModLock;
        return;
    default:
        return;
    }
    if (pressed)
        modsDepressed |= bit;
    else
        modsDepressed &= ~bit;
}

// ---- QWaylandTouch -------------------------------------------------------------------------------------------

/*!
    \class QWaylandTouch
    \brief The QWaylandTouch class sends touch events to the client of the touched surface.

    \differences
    There is no \c sendFullTouchEvent(), as Nt has no QTouchEvent; send the points one by one and finish with
    sendFrameEvent().
    \enddifferences

    \sa QWaylandSeat
*/

/*!
    Constructs the touch device of \a seat with the given \a parent.
*/
QWaylandTouch::QWaylandTouch(QWaylandSeat *seat, QObject *parent)
    : QWaylandObject(parent), d(std::make_unique<Private>()) {
    d->seat = seat;
    d->handler.onRequest = [](Resource *resource, std::uint32_t opcode, const Args &) {
        if (opcode == kTouchRelease)
            resource->destroy();
    };
}

/*!
    Destroys the touch device.
*/
QWaylandTouch::~QWaylandTouch() = default;

/*!
    Returns the seat.
*/
QWaylandSeat *QWaylandTouch::seat() const { return d->seat; }

/*!
    Returns the compositor.
*/
QWaylandCompositor *QWaylandTouch::compositor() const { return d->seat->compositor(); }

/*!
    Sends the touch point \a id at \a position in \a surface, with \a state saying whether it went down, moved
    or came up. Returns the serial of the event, or 0 for a move, which has none.
*/
uint QWaylandTouch::sendTouchPointEvent(QWaylandSurface *surface, int id, const QPointF &position,
                                        Qt::TouchPointState state) {
    if (!surface)
        return 0;
    Connection *c = connectionOf(surface);
    Resource *res = QWaylandSurfacePrivateAccess::d(surface)->resource;
    if (!c || !res)
        return 0;
    const std::uint32_t time = Display::timestamp();
    std::uint32_t serial = 0;
    switch (state) {
    case Qt::TouchPointPressed:
        serial = compositor()->nextSerial();
        d->focus = surface;
        d->handler.sendTo(c, kTouchDown,
                          {Arg::Uint(serial), Arg::Uint(time), Arg::Object(res), Arg::Int(id), Arg::Fixed(position.x()),
                           Arg::Fixed(position.y())});
        break;
    case Qt::TouchPointMoved:
        d->handler.sendTo(c, kTouchMotion,
                          {Arg::Uint(time), Arg::Int(id), Arg::Fixed(position.x()), Arg::Fixed(position.y())});
        break;
    case Qt::TouchPointReleased:
        serial = compositor()->nextSerial();
        d->handler.sendTo(c, kTouchUp, {Arg::Uint(serial), Arg::Uint(time), Arg::Int(id)});
        break;
    default:
        break;
    }
    return serial;
}

/*!
    Tells \a client that the points sent so far form one frame.
*/
void QWaylandTouch::sendFrameEvent(QWaylandClient *client) { d->handler.sendTo(connectionOf(client), kTouchFrame); }

/*!
    Tells \a client that the touch sequence was cancelled.
*/
void QWaylandTouch::sendCancelEvent(QWaylandClient *client) { d->handler.sendTo(connectionOf(client), kTouchCancel); }

/*!
    Returns the surface the last touch went down in.
*/
QWaylandSurface *QWaylandTouch::focus() const { return d->focus; }

// ---- QWaylandKeymap ------------------------------------------------------------------------------------------

/*!
    \class QWaylandKeymap
    \brief The QWaylandKeymap class names the keyboard layout clients are given.

    The names are those of XKB: layout (\c "us", \c "de"), variant, options, model and rules. They take effect
    when the seat is initialized, and only when Nt was built with xkbcommon.

    \differences
    Changes after the seat was initialized are not sent to clients.
    \enddifferences
*/

/*!
    Constructs a keymap of \a layout, \a variant, \a options, \a model and \a rules with the given \a parent.
*/
QWaylandKeymap::QWaylandKeymap(const QString &layout, const QString &variant, const QString &options,
                               const QString &model, const QString &rules, QObject *parent)
    : QObject(parent), m_layout(layout), m_variant(variant), m_options(options), m_rules(rules), m_model(model) {}

/*!
    Sets the layout to \a layout.
*/
void QWaylandKeymap::setLayout(const QString &layout) {
    if (m_layout == layout)
        return;
    m_layout = layout;
    emit layoutChanged();
}

/*!
    Sets the variant to \a variant.
*/
void QWaylandKeymap::setVariant(const QString &variant) {
    if (m_variant == variant)
        return;
    m_variant = variant;
    emit variantChanged();
}

/*!
    Sets the options to \a options.
*/
void QWaylandKeymap::setOptions(const QString &options) {
    if (m_options == options)
        return;
    m_options = options;
    emit optionsChanged();
}

/*!
    Sets the rules to \a rules.
*/
void QWaylandKeymap::setRules(const QString &rules) {
    if (m_rules == rules)
        return;
    m_rules = rules;
    emit rulesChanged();
}

/*!
    Sets the model to \a model.
*/
void QWaylandKeymap::setModel(const QString &model) {
    if (m_model == model)
        return;
    m_model = model;
    emit modelChanged();
}

#include "moc_qwaylandkeyboard.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qwaylandkeymap.cpp"   // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qwaylandpointer.cpp"  // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qwaylandseat.cpp"     // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qwaylandtouch.cpp"    // NOLINT(bugprone-suspicious-include): generated by ntmoc
