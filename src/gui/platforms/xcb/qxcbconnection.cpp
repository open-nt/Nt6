// MIT License - see LICENSE
#include "qxcbconnection_p.h"

#include "qpa/qwindowsysteminterface_p.h"
#include "qwindow_p.h"
#include "qxcbwindow_p.h"

#include <QtCore/qabstracteventdispatcher.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <QtCore/qlibrary.h>
#include <QtCore/qsocketnotifier.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qwindow.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

constexpr int kDoubleClickMs = 400;
constexpr int kDoubleClickDistance = 5;

QEvent::Type processEventType() {
    static const int type = QEvent::registerEventType();
    return static_cast<QEvent::Type>(type);
}

Qt::MouseButton translateButton(int detail) {
    switch (detail) {
    case 1:
        return Qt::LeftButton;
    case 2:
        return Qt::MiddleButton;
    case 3:
        return Qt::RightButton;
    case 8:
        return Qt::BackButton;
    case 9:
        return Qt::ForwardButton;
    default:
        return Qt::NoButton;
    }
}

int keysymToQtKey(xcb_keysym_t sym) {
    if (sym >= 0x61 && sym <= 0x7a)
        return static_cast<int>(sym) - 0x20; // letters are upper case in Qt::Key
    if (sym >= 0x20 && sym <= 0x7e)
        return static_cast<int>(sym);
    if (sym >= 0xa0 && sym <= 0xff)
        return static_cast<int>(sym);
    if (sym >= 0xff80 && sym <= 0xffb9) {
        // keypad: digits and operators map to the ASCII keys
        switch (sym) {
        case 0xff80:
            return Qt::Key_Space;
        case 0xff8d:
            return Qt::Key_Enter;
        case 0xffaa:
            return '*';
        case 0xffab:
            return '+';
        case 0xffac:
            return ',';
        case 0xffad:
            return '-';
        case 0xffae:
            return '.';
        case 0xffaf:
            return '/';
        default:
            break;
        }
        if (sym >= 0xffb0)
            return Qt::Key_0 + static_cast<int>(sym - 0xffb0);
    }
    if (sym >= 0xffbe && sym <= 0xffc9)
        return Qt::Key_F1 + static_cast<int>(sym - 0xffbe);
    switch (sym) {
    case 0xff1b:
        return Qt::Key_Escape;
    case 0xff09:
        return Qt::Key_Tab;
    case 0xfe20:
        return Qt::Key_Backtab;
    case 0xff08:
        return Qt::Key_Backspace;
    case 0xff0d:
        return Qt::Key_Return;
    case 0xff63:
        return Qt::Key_Insert;
    case 0xffff:
        return Qt::Key_Delete;
    case 0xff13:
        return Qt::Key_Pause;
    case 0xff61:
        return Qt::Key_Print;
    case 0xff67:
        return Qt::Key_Menu;
    case 0xff50:
    case 0xff95:
        return Qt::Key_Home;
    case 0xff51:
    case 0xff96:
        return Qt::Key_Left;
    case 0xff52:
    case 0xff97:
        return Qt::Key_Up;
    case 0xff53:
    case 0xff98:
        return Qt::Key_Right;
    case 0xff54:
    case 0xff99:
        return Qt::Key_Down;
    case 0xff55:
    case 0xff9a:
        return Qt::Key_PageUp;
    case 0xff56:
    case 0xff9b:
        return Qt::Key_PageDown;
    case 0xff57:
    case 0xff9c:
        return Qt::Key_End;
    case 0xff9e:
        return Qt::Key_Insert;
    case 0xff9f:
        return Qt::Key_Delete;
    case 0xffe1:
    case 0xffe2:
        return Qt::Key_Shift;
    case 0xffe3:
    case 0xffe4:
        return Qt::Key_Control;
    case 0xffe5:
        return Qt::Key_CapsLock;
    case 0xffe7:
    case 0xffe8:
    case 0xffeb:
    case 0xffec:
        return Qt::Key_Meta;
    case 0xffe9:
    case 0xffea:
        return Qt::Key_Alt;
    case 0xff7f:
        return Qt::Key_NumLock;
    case 0xff14:
        return Qt::Key_ScrollLock;
    default:
        return Qt::Key_unknown;
    }
}

QString keysymToText(xcb_keysym_t sym) {
    if ((sym >= 0x20 && sym <= 0x7e) || (sym >= 0xa0 && sym <= 0xff))
        return QString(QChar(static_cast<ushort>(sym)));
    if (sym >= 0xffb0 && sym <= 0xffb9)
        return QString(QChar(static_cast<ushort>('0' + (sym - 0xffb0))));
    switch (sym) {
    case 0xff0d:
    case 0xff8d:
        return QString(QChar(static_cast<ushort>('\r')));
    case 0xff09:
        return QString(QChar(static_cast<ushort>('\t')));
    case 0xff08:
        return QString(QChar(static_cast<ushort>('\b')));
    case 0xff1b:
        return QString(QChar(static_cast<ushort>(0x1b)));
    case 0xffaa:
        return QString(QChar(static_cast<ushort>('*')));
    case 0xffab:
        return QString(QChar(static_cast<ushort>('+')));
    case 0xffad:
        return QString(QChar(static_cast<ushort>('-')));
    case 0xffae:
        return QString(QChar(static_cast<ushort>('.')));
    case 0xffaf:
        return QString(QChar(static_cast<ushort>('/')));
    default:
        break;
    }
    if (sym >= 0x1000000 && sym <= 0x110ffff)
        return QString(QChar(static_cast<ushort>(sym - 0x1000000)));
    return QString();
}

bool isKeypadKeysym(xcb_keysym_t sym) { return sym >= 0xff80 && sym <= 0xffbd; }

// The root visual must be a 24 or 32 bit true color visual with the usual masks, which is what the
// backing store's QImage::Format_RGB32 pixels look like.
bool checkVisual(const xcb_setup_t *setup, const xcb_screen_t *screen, QString *error) {
    const xcb_visualtype_t *visual = nullptr;
    for (xcb_depth_iterator_t d = xcb_screen_allowed_depths_iterator(screen); d.rem; xcb_depth_next(&d)) {
        for (xcb_visualtype_iterator_t v = xcb_depth_visuals_iterator(d.data); v.rem; xcb_visualtype_next(&v)) {
            if (v.data->visual_id == screen->root_visual)
                visual = v.data;
        }
    }
    if (!visual || visual->_class != XCB_VISUAL_CLASS_TRUE_COLOR || visual->red_mask != 0xff0000u ||
        visual->green_mask != 0x00ff00u || visual->blue_mask != 0x0000ffu) {
        *error = QString::fromLatin1("the root visual of the X server is not 24 bit true color (RGB)");
        return false;
    }
    bool format32 = false;
    for (xcb_format_iterator_t f = xcb_setup_pixmap_formats_iterator(setup); f.rem; xcb_format_next(&f)) {
        if (f.data->depth == screen->root_depth && f.data->bits_per_pixel == 32 && f.data->scanline_pad == 32)
            format32 = true;
    }
    if (!format32 || setup->image_byte_order != XCB_IMAGE_ORDER_LSB_FIRST) {
        *error = QString::fromLatin1("the X server does not use 32 bit little endian pixmaps");
        return false;
    }
    return true;
}

// The Xft.dpi resource of the X server, which desktops set to scale the interface; 0 if it is not set.
double xftDpi(xcb_connection_t *c, xcb_window_t root) {
    xcb_get_property_reply_t *reply = xcb_get_property_reply(
        c, xcb_get_property(c, 0, root, XCB_ATOM_RESOURCE_MANAGER, XCB_ATOM_STRING, 0, 16384), nullptr);
    if (!reply)
        return 0.0;
    const std::string resources(static_cast<const char *>(xcb_get_property_value(reply)),
                                static_cast<size_t>(xcb_get_property_value_length(reply)));
    std::free(reply);
    const std::string key = "Xft.dpi:";
    size_t pos = 0;
    while ((pos = resources.find(key, pos)) != std::string::npos) {
        if (pos == 0 || resources[pos - 1] == '\n')
            return std::atof(resources.c_str() + pos + key.size());
        pos += key.size();
    }
    return 0.0;
}

// How many X pixels one unit of window coordinates takes: QT_SCALE_FACTOR if it is set, otherwise
// the density that Xft.dpi asks for relative to 96 dpi. Never less than 1.
qreal screenScale(xcb_connection_t *c, xcb_window_t root) {
    if (const char *env = std::getenv("QT_SCALE_FACTOR")) {
        const double factor = std::atof(env);
        if (factor > 0.0)
            return factor;
    }
    return std::max(1.0, xftDpi(c, root) / 96.0);
}

} // namespace

QXcbScreen::QXcbScreen(xcb_screen_t *screen, int number, qreal scale)
    : m_geometry(0, 0, qRound(screen->width_in_pixels / scale), qRound(screen->height_in_pixels / scale)),
      m_depth(screen->root_depth), m_name(QString::fromLatin1("X11 screen %1").arg(number)),
      m_physicalSize(screen->width_in_millimeters, screen->height_in_millimeters), m_root(screen->root),
      m_scale(scale) {
    if (m_physicalSize.width() <= 0 || m_physicalSize.height() <= 0)
        m_physicalSize = QPlatformScreen::physicalSize();
}

std::unique_ptr<QXcbConnection> QXcbConnection::create(QString *error) {
    int screenNumber = 0;
    xcb_connection_t *c = xcb_connect(nullptr, &screenNumber);
    if (!c || xcb_connection_has_error(c)) {
        const char *display = std::getenv("DISPLAY");
        *error = QString::fromLatin1("could not connect to the X display \"%1\"")
                     .arg(QString::fromLocal8Bit(display ? display : ""));
        if (c)
            xcb_disconnect(c);
        return nullptr;
    }
    std::unique_ptr<QXcbConnection> connection(new QXcbConnection(c, screenNumber));
    if (!checkVisual(xcb_get_setup(c), connection->m_screen, error))
        return nullptr;
    return connection;
}

QXcbConnection::QXcbConnection(xcb_connection_t *connection, int screenNumber)
    : m_connection(connection), m_screenNumber(screenNumber) {
    const xcb_setup_t *setup = xcb_get_setup(connection);
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screenNumber && it.rem; ++i)
        xcb_screen_next(&it);
    m_screen = it.data;
    m_platformScreen = std::make_unique<QXcbScreen>(m_screen, screenNumber, screenScale(connection, m_screen->root));
    m_minKeycode = setup->min_keycode;
    m_maxKeycode = setup->max_keycode;

    m_wmProtocols = atom("WM_PROTOCOLS");
    m_wmDeleteWindow = atom("WM_DELETE_WINDOW");
    m_netWmPing = atom("_NET_WM_PING");
    m_netWmName = atom("_NET_WM_NAME");
    m_netWmPid = atom("_NET_WM_PID");
    m_utf8String = atom("UTF8_STRING");
    updateKeyboardMapping();
    m_maxRequestBytes = static_cast<size_t>(xcb_get_maximum_request_length(connection)) * 4;

    m_notifier = new QSocketNotifier(xcb_get_file_descriptor(connection), QSocketNotifier::Read, this);
    QObject::connect(m_notifier, &QSocketNotifier::activated, this, [this] { processEvents(); });
    // Other users of the connection, such as Mesa while an EGL window renders, read events into XCB's queue
    // without a flush() here, so the queue is looked at before the thread waits.
    if (QAbstractEventDispatcher *dispatcher = QAbstractEventDispatcher::instance())
        QObject::connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, this, &QXcbConnection::checkQueue,
                         Qt::DirectConnection);
    scheduleProcessing();
}

QXcbConnection::~QXcbConnection() {
    if (m_cursorContext && cursorLibrary().contextFree)
        cursorLibrary().contextFree(m_cursorContext);
    if (m_pending)
        std::free(m_pending);
    xcb_disconnect(m_connection);
}

size_t QXcbConnection::maxRequestBytes() const { return m_maxRequestBytes; }

QXcbWindow *QXcbConnection::window(xcb_window_t id) const {
    const auto it = m_windows.find(id);
    return it == m_windows.end() ? nullptr : it->second;
}

xcb_atom_t QXcbConnection::atom(const char *name) {
    const auto it = m_atoms.find(name);
    if (it != m_atoms.end())
        return it->second;
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(
        m_connection, xcb_intern_atom(m_connection, 0, static_cast<uint16_t>(std::strlen(name)), name), nullptr);
    const xcb_atom_t a = reply ? reply->atom : static_cast<xcb_atom_t>(XCB_ATOM_NONE);
    std::free(reply);
    m_atoms[name] = a;
    return a;
}

// XCB reads from the socket while it writes requests, and what it reads waits in its own queue, where the
// socket notifier cannot see it. So every flush is followed by a look at the queue.
void QXcbConnection::flush() {
    xcb_flush(m_connection);
    scheduleProcessing();
}

void QXcbConnection::scheduleProcessing() {
    if (m_processScheduled)
        return;
    m_processScheduled = true;
    QCoreApplication::postEvent(this, new QEvent(processEventType()));
}

void QXcbConnection::checkQueue() {
    if (m_processing)
        return;
    if (!m_pending)
        m_pending = xcb_poll_for_queued_event(m_connection);
    if (m_pending)
        scheduleProcessing();
}

bool QXcbConnection::event(QEvent *e) {
    if (e->type() == processEventType()) {
        m_processScheduled = false;
        processEvents();
        return true;
    }
    return QObject::event(e);
}

xcb_generic_event_t *QXcbConnection::nextEvent() {
    if (m_pending) {
        xcb_generic_event_t *e = m_pending;
        m_pending = nullptr;
        return e;
    }
    return xcb_poll_for_event(m_connection);
}

void QXcbConnection::processEvents() {
    if (m_processing)
        return;
    m_processing = true;
    static const QByteArray eventType = QByteArrayLiteral("xcb_generic_event_t");
    QAbstractEventDispatcher *dispatcher = QAbstractEventDispatcher::instance();
    for (;;) {
        while (xcb_generic_event_t *e = nextEvent()) {
            qintptr result = 0;
            if (!dispatcher || !dispatcher->filterNativeEvent(eventType, e, &result))
                handleEvent(e);
            std::free(e);
        }
        // Flushing the requests that the handlers made can bring in more events.
        xcb_flush(m_connection);
        m_pending = xcb_poll_for_queued_event(m_connection);
        if (!m_pending)
            break;
    }
    m_processing = false;
    if (xcb_connection_has_error(m_connection)) {
        std::fputs("Nt GUI: the connection to the X server broke\n", stderr);
        m_notifier->setEnabled(false);
        QCoreApplication::exit(1);
    }
}

void QXcbConnection::updateKeyboardMapping() {
    const int count = m_maxKeycode - m_minKeycode + 1;
    xcb_get_keyboard_mapping_reply_t *reply = xcb_get_keyboard_mapping_reply(
        m_connection, xcb_get_keyboard_mapping(m_connection, m_minKeycode, static_cast<uint8_t>(count)), nullptr);
    if (!reply)
        return;
    m_keysymsPerKeycode = reply->keysyms_per_keycode;
    const xcb_keysym_t *syms = xcb_get_keyboard_mapping_keysyms(reply);
    m_keysyms.assign(syms, syms + xcb_get_keyboard_mapping_keysyms_length(reply));
    std::free(reply);
    scheduleProcessing();
}

Qt::KeyboardModifiers QXcbConnection::translateModifiers(uint16_t state) {
    Qt::KeyboardModifiers m;
    if (state & XCB_MOD_MASK_SHIFT)
        m |= Qt::ShiftModifier;
    if (state & XCB_MOD_MASK_CONTROL)
        m |= Qt::ControlModifier;
    if (state & XCB_MOD_MASK_1)
        m |= Qt::AltModifier;
    if (state & XCB_MOD_MASK_4)
        m |= Qt::MetaModifier;
    return m;
}

QXcbConnection::Key QXcbConnection::translateKey(xcb_keycode_t code, uint16_t state) const {
    if (code < m_minKeycode || code > m_maxKeycode || m_keysymsPerKeycode == 0)
        return {Qt::Key_unknown, QString()};
    const size_t base = static_cast<size_t>(code - m_minKeycode) * static_cast<size_t>(m_keysymsPerKeycode);
    if (base >= m_keysyms.size())
        return {Qt::Key_unknown, QString()};
    const xcb_keysym_t first = m_keysyms[base];
    xcb_keysym_t second = m_keysymsPerKeycode > 1 ? m_keysyms[base + 1] : 0;
    if (second == 0)
        second = first;
    const bool shift = state & XCB_MOD_MASK_SHIFT;
    const bool lock = state & XCB_MOD_MASK_LOCK;
    const bool numlock = state & XCB_MOD_MASK_2;
    bool useSecond;
    if (isKeypadKeysym(first) || isKeypadKeysym(second))
        useSecond = numlock != shift;
    else if (first >= 0x61 && first <= 0x7a)
        useSecond = shift != lock;
    else
        useSecond = shift;
    const xcb_keysym_t sym = useSecond ? second : first;
    Key key{keysymToQtKey(sym), keysymToText(sym)};
    if (state & XCB_MOD_MASK_CONTROL)
        key.text.clear();
    return key;
}

namespace {

// The glyph of the X cursor font for a shape; the mask is the glyph after it. Shapes that the font does not
// have get the nearest one.
int cursorGlyph(Qt::CursorShape shape) {
    switch (shape) {
    case Qt::UpArrowCursor:
        return 22; // center_ptr
    case Qt::CrossCursor:
        return 34; // crosshair
    case Qt::WaitCursor:
    case Qt::BusyCursor:
        return 150; // watch
    case Qt::IBeamCursor:
        return 152; // xterm
    case Qt::SizeVerCursor:
    case Qt::SplitVCursor:
        return 116; // sb_v_double_arrow
    case Qt::SizeHorCursor:
    case Qt::SplitHCursor:
        return 108; // sb_h_double_arrow
    case Qt::SizeBDiagCursor:
        return 136; // top_right_corner
    case Qt::SizeFDiagCursor:
        return 134; // top_left_corner
    case Qt::SizeAllCursor:
    case Qt::OpenHandCursor:
    case Qt::ClosedHandCursor:
    case Qt::DragMoveCursor:
        return 52; // fleur
    case Qt::PointingHandCursor:
    case Qt::DragLinkCursor:
        return 60; // hand2
    case Qt::ForbiddenCursor:
        return 0; // X_cursor
    case Qt::WhatsThisCursor:
        return 92; // question_arrow
    case Qt::DragCopyCursor:
        return 90; // plus
    default:
        return 68; // left_ptr
    }
}

// The names of a shape in cursor themes, most specific first, as the freedesktop.org cursor specification
// and the older X names have them.
// Unused entries are null.
std::array<const char *, 3> cursorNames(Qt::CursorShape shape) {
    switch (shape) {
    case Qt::UpArrowCursor:
        return {"up-arrow", "up_arrow", "center_ptr"};
    case Qt::CrossCursor:
        return {"crosshair", "cross"};
    case Qt::WaitCursor:
        return {"wait", "watch"};
    case Qt::BusyCursor:
        return {"progress", "left_ptr_watch", "watch"};
    case Qt::IBeamCursor:
        return {"text", "xterm", "ibeam"};
    case Qt::SizeVerCursor:
        return {"ns-resize", "size_ver", "sb_v_double_arrow"};
    case Qt::SizeHorCursor:
        return {"ew-resize", "size_hor", "sb_h_double_arrow"};
    case Qt::SizeBDiagCursor:
        return {"nesw-resize", "size_bdiag", "fd_double_arrow"};
    case Qt::SizeFDiagCursor:
        return {"nwse-resize", "size_fdiag", "bd_double_arrow"};
    case Qt::SizeAllCursor:
        return {"all-scroll", "size_all", "fleur"};
    case Qt::SplitVCursor:
        return {"row-resize", "split_v", "sb_v_double_arrow"};
    case Qt::SplitHCursor:
        return {"col-resize", "split_h", "sb_h_double_arrow"};
    case Qt::PointingHandCursor:
        return {"pointer", "pointing_hand", "hand2"};
    case Qt::ForbiddenCursor:
        return {"not-allowed", "forbidden", "crossed_circle"};
    case Qt::WhatsThisCursor:
        return {"help", "whats_this", "question_arrow"};
    case Qt::OpenHandCursor:
        return {"grab", "openhand", "fleur"};
    case Qt::ClosedHandCursor:
        return {"grabbing", "closedhand", "fleur"};
    case Qt::DragCopyCursor:
        return {"copy", "dnd-copy"};
    case Qt::DragMoveCursor:
        return {"move", "dnd-move"};
    case Qt::DragLinkCursor:
        return {"alias", "dnd-link"};
    default:
        return {"default", "left_ptr"};
    }
}

} // namespace

// libxcb-cursor is loaded when the first cursor is made, so that Nt neither needs it to build nor to run: the
// cursors of the X font are used without it.
const QXcbConnection::CursorLibrary &QXcbConnection::cursorLibrary() {
    static const CursorLibrary lib = [] {
        CursorLibrary l{};
        static QLibrary library(QStringLiteral("xcb-cursor"), 0); // kept loaded for the process
        if (!library.load())
            return l;
        // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast): the functions come from dlsym().
        l.contextNew = reinterpret_cast<decltype(l.contextNew)>(library.resolve("xcb_cursor_context_new"));
        l.loadCursor = reinterpret_cast<decltype(l.loadCursor)>(library.resolve("xcb_cursor_load_cursor"));
        l.contextFree = reinterpret_cast<decltype(l.contextFree)>(library.resolve("xcb_cursor_context_free"));
        // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
        if (!l.contextNew || !l.loadCursor || !l.contextFree)
            l = CursorLibrary{};
        return l;
    }();
    return lib;
}

xcb_cursor_t QXcbConnection::themeCursor(Qt::CursorShape shape) {
    const CursorLibrary &lib = cursorLibrary();
    if (!m_cursorContextTried) {
        m_cursorContextTried = true;
        if (lib.contextNew && lib.contextNew(m_connection, m_screen, &m_cursorContext) < 0)
            m_cursorContext = nullptr;
    }
    if (!m_cursorContext || !lib.loadCursor)
        return XCB_NONE;
    for (const char *name : cursorNames(shape)) {
        if (!name)
            break;
        const xcb_cursor_t c = lib.loadCursor(m_cursorContext, name);
        if (c != XCB_NONE)
            return c;
    }
    return XCB_NONE;
}

xcb_cursor_t QXcbConnection::cursor(Qt::CursorShape shape) {
    const auto known = m_cursors.find(static_cast<int>(shape));
    if (known != m_cursors.end())
        return known->second;
    xcb_cursor_t cursor = XCB_NONE;
    if (shape == Qt::BlankCursor) {
        // A cursor of one pixel with an empty mask: nothing shows.
        const xcb_pixmap_t pixmap = xcb_generate_id(m_connection);
        xcb_create_pixmap(m_connection, 1, pixmap, m_screen->root, 1, 1);
        const xcb_gcontext_t gc = xcb_generate_id(m_connection);
        const uint32_t zero = 0;
        xcb_create_gc(m_connection, gc, pixmap, XCB_GC_FOREGROUND, &zero);
        const xcb_rectangle_t all{0, 0, 1, 1};
        xcb_poly_fill_rectangle(m_connection, pixmap, gc, 1, &all);
        cursor = xcb_generate_id(m_connection);
        xcb_create_cursor(m_connection, cursor, pixmap, pixmap, 0, 0, 0, 0, 0, 0, 0, 0);
        xcb_free_gc(m_connection, gc);
        xcb_free_pixmap(m_connection, pixmap);
    } else {
        cursor = themeCursor(shape);
    }
    if (cursor == XCB_NONE && shape != Qt::BlankCursor) {
        if (m_cursorFont == XCB_NONE) {
            m_cursorFont = xcb_generate_id(m_connection);
            xcb_open_font(m_connection, m_cursorFont, 6, "cursor");
        }
        const auto glyph = static_cast<uint16_t>(cursorGlyph(shape));
        cursor = xcb_generate_id(m_connection);
        // Black with a white outline, as the cursors of X have always been.
        xcb_create_glyph_cursor(m_connection, cursor, m_cursorFont, m_cursorFont, glyph,
                                static_cast<uint16_t>(glyph + 1), 0, 0, 0, 0xffff, 0xffff, 0xffff);
    }
    m_cursors[static_cast<int>(shape)] = cursor;
    return cursor;
}

QPoint QXcbConnection::pointerPosition() const {
    xcb_query_pointer_reply_t *reply =
        xcb_query_pointer_reply(m_connection, xcb_query_pointer(m_connection, m_screen->root), nullptr);
    if (!reply)
        return QtPrivate::GuiAccess::cursorPosition();
    const QPoint pos(toLogical(reply->root_x), toLogical(reply->root_y));
    std::free(reply);
    return pos;
}

void QXcbConnection::warpPointer(const QPoint &logical) {
    xcb_warp_pointer(m_connection, XCB_NONE, m_screen->root, 0, 0, 0, 0, static_cast<int16_t>(toNative(logical.x())),
                     static_cast<int16_t>(toNative(logical.y())));
    xcb_flush(m_connection);
    QtPrivate::GuiAccess::setCursorPosition(logical);
}

QRect QXcbConnection::toLogicalCovering(const QRect &native) const {
    const int left = static_cast<int>(std::floor(native.left() / scale()));
    const int top = static_cast<int>(std::floor(native.top() / scale()));
    const int right = static_cast<int>(std::ceil((native.right() + 1) / scale()));
    const int bottom = static_cast<int>(std::ceil((native.bottom() + 1) / scale()));
    return QRect(left, top, right - left, bottom - top);
}

void QXcbConnection::handleEvent(xcb_generic_event_t *event) {
    const int type = event->response_type & ~0x80;
    switch (type) {
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE:
        handleKey(event, false);
        break;
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE:
        handleButton(event);
        break;
    case XCB_MOTION_NOTIFY: {
        auto *e = reinterpret_cast<xcb_motion_notify_event_t *>(event);
        if (QXcbWindow *w = window(e->event))
            QWindowSystemInterface::handleMouseEvent(w->window(), QPointF(e->event_x / scale(), e->event_y / scale()),
                                                     QPointF(e->root_x / scale(), e->root_y / scale()), m_buttons,
                                                     Qt::NoButton, QEvent::MouseMove, translateModifiers(e->state));
        break;
    }
    case XCB_ENTER_NOTIFY: {
        auto *e = reinterpret_cast<xcb_enter_notify_event_t *>(event);
        if (e->mode == XCB_NOTIFY_MODE_NORMAL && e->detail != XCB_NOTIFY_DETAIL_INFERIOR) {
            if (QXcbWindow *w = window(e->event))
                QWindowSystemInterface::handleEnterEvent(w->window(),
                                                         QPointF(e->event_x / scale(), e->event_y / scale()),
                                                         QPointF(e->root_x / scale(), e->root_y / scale()));
        }
        break;
    }
    case XCB_LEAVE_NOTIFY: {
        auto *e = reinterpret_cast<xcb_leave_notify_event_t *>(event);
        if (e->mode == XCB_NOTIFY_MODE_NORMAL && e->detail != XCB_NOTIFY_DETAIL_INFERIOR) {
            if (QXcbWindow *w = window(e->event))
                QWindowSystemInterface::handleLeaveEvent(w->window());
        }
        break;
    }
    case XCB_FOCUS_IN: {
        auto *e = reinterpret_cast<xcb_focus_in_event_t *>(event);
        if (e->mode != XCB_NOTIFY_MODE_GRAB && e->mode != XCB_NOTIFY_MODE_UNGRAB)
            if (QXcbWindow *w = window(e->event))
                QWindowSystemInterface::handleWindowActivated(w->window());
        break;
    }
    case XCB_FOCUS_OUT: {
        auto *e = reinterpret_cast<xcb_focus_out_event_t *>(event);
        if (e->mode != XCB_NOTIFY_MODE_GRAB && e->mode != XCB_NOTIFY_MODE_UNGRAB &&
            e->detail != XCB_NOTIFY_DETAIL_INFERIOR) {
            if (QXcbWindow *w = window(e->event))
                if (QGuiApplication::focusWindow() == w->window())
                    QWindowSystemInterface::handleWindowActivated(nullptr);
        }
        break;
    }
    case XCB_EXPOSE: {
        auto *e = reinterpret_cast<xcb_expose_event_t *>(event);
        QRegion &region = m_exposed[e->window];
        region += toLogicalCovering(QRect(e->x, e->y, e->width, e->height));
        if (e->count == 0) {
            const QRegion done = region;
            m_exposed.erase(e->window);
            if (QXcbWindow *w = window(e->window))
                QWindowSystemInterface::handleExposeEvent(w->window(), done);
        }
        break;
    }
    case XCB_UNMAP_NOTIFY: {
        auto *e = reinterpret_cast<xcb_unmap_notify_event_t *>(event);
        if (QXcbWindow *w = window(e->window))
            QWindowSystemInterface::handleExposeEvent(w->window(), QRegion());
        break;
    }
    case XCB_CONFIGURE_NOTIFY:
        handleConfigure(event);
        break;
    case XCB_CLIENT_MESSAGE:
        handleClientMessage(event);
        break;
    case XCB_MAPPING_NOTIFY: {
        auto *e = reinterpret_cast<xcb_mapping_notify_event_t *>(event);
        if (e->request == XCB_MAPPING_KEYBOARD)
            updateKeyboardMapping();
        break;
    }
    default:
        break;
    }
}

void QXcbConnection::handleKey(xcb_generic_event_t *event, bool autoRepeat) {
    auto *e = reinterpret_cast<xcb_key_press_event_t *>(event);
    const bool press = (event->response_type & ~0x80) == XCB_KEY_PRESS;
    if (!press) {
        // A key that repeats is reported as a release and a press with the same time stamp.
        xcb_generic_event_t *next = xcb_poll_for_queued_event(m_connection);
        if (next && (next->response_type & ~0x80) == XCB_KEY_PRESS) {
            auto *p = reinterpret_cast<xcb_key_press_event_t *>(next);
            if (p->time == e->time && p->detail == e->detail) {
                handleKey(next, true);
                std::free(next);
                return;
            }
        }
        m_pending = next;
    }
    QXcbWindow *w = window(e->event);
    if (!w)
        return;
    const Key key = translateKey(e->detail, e->state);
    Qt::KeyboardModifiers modifiers = translateModifiers(e->state);
    const size_t base = static_cast<size_t>(e->detail - m_minKeycode) * static_cast<size_t>(m_keysymsPerKeycode);
    if (e->detail >= m_minKeycode && base < m_keysyms.size() && isKeypadKeysym(m_keysyms[base]))
        modifiers |= Qt::KeypadModifier;
    QWindowSystemInterface::handleKeyEvent(w->window(), press ? QEvent::KeyPress : QEvent::KeyRelease, key.qtKey,
                                           modifiers, key.text, autoRepeat);
}

void QXcbConnection::handleButton(xcb_generic_event_t *event) {
    auto *e = reinterpret_cast<xcb_button_press_event_t *>(event);
    QXcbWindow *w = window(e->event);
    if (!w)
        return;
    const bool press = (event->response_type & ~0x80) == XCB_BUTTON_PRESS;
    const QPointF local(e->event_x / scale(), e->event_y / scale());
    const QPointF global(e->root_x / scale(), e->root_y / scale());
    const Qt::KeyboardModifiers modifiers = translateModifiers(e->state);
    if (e->detail >= 4 && e->detail <= 7) {
        if (press) {
            QPoint angle;
            if (e->detail == 4)
                angle = QPoint(0, 120);
            else if (e->detail == 5)
                angle = QPoint(0, -120);
            else if (e->detail == 6)
                angle = QPoint(120, 0);
            else
                angle = QPoint(-120, 0);
            QWindowSystemInterface::handleWheelEvent(w->window(), local, global, QPoint(), angle, modifiers);
        }
        return;
    }
    const Qt::MouseButton button = translateButton(e->detail);
    if (button == Qt::NoButton)
        return;
    QEvent::Type type = QEvent::MouseButtonRelease;
    if (press) {
        m_buttons |= button;
        type = QEvent::MouseButtonPress;
        const QPoint pos(e->root_x, e->root_y);
        if (e->detail == m_lastPressButton && e->time - m_lastPressTime < kDoubleClickMs &&
            (pos - m_lastPressPos).manhattanLength() < kDoubleClickDistance) {
            type = QEvent::MouseButtonDblClick;
            m_lastPressTime = 0;
        } else {
            m_lastPressTime = e->time;
            m_lastPressButton = e->detail;
            m_lastPressPos = pos;
        }
    } else {
        m_buttons &= ~Qt::MouseButtons(button);
    }
    QWindowSystemInterface::handleMouseEvent(w->window(), local, global, m_buttons, button, type, modifiers);
}

void QXcbConnection::handleConfigure(xcb_generic_event_t *event) {
    auto *e = reinterpret_cast<xcb_configure_notify_event_t *>(event);
    QXcbWindow *w = window(e->window);
    if (!w)
        return;
    QPoint pos(e->x, e->y);
    // Unless the window manager sent it, the position is relative to the frame of the window.
    if (!(event->response_type & 0x80)) {
        xcb_translate_coordinates_reply_t *reply = xcb_translate_coordinates_reply(
            m_connection, xcb_translate_coordinates(m_connection, e->window, m_screen->root, 0, 0), nullptr);
        if (reply) {
            pos = QPoint(reply->dst_x, reply->dst_y);
            std::free(reply);
        }
        scheduleProcessing();
    }
    const QRect rect(QPoint(toLogical(pos.x()), toLogical(pos.y())),
                     QSize(std::max(1, toLogical(e->width)), std::max(1, toLogical(e->height))));
    w->setServerGeometry(rect);
    QWindowSystemInterface::handleGeometryChange(w->window(), rect);
}

void QXcbConnection::handleClientMessage(xcb_generic_event_t *event) {
    auto *e = reinterpret_cast<xcb_client_message_event_t *>(event);
    if (e->type != m_wmProtocols || e->format != 32)
        return;
    QXcbWindow *w = window(e->window);
    if (!w)
        return;
    const xcb_atom_t protocol = e->data.data32[0];
    if (protocol == m_wmDeleteWindow) {
        QWindowSystemInterface::handleCloseEvent(w->window());
    } else if (protocol == m_netWmPing) {
        // Tell the window manager that the application is alive.
        xcb_client_message_event_t reply = *e;
        reply.window = m_screen->root;
        xcb_send_event(m_connection, 0, m_screen->root,
                       XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                       reinterpret_cast<const char *>(&reply));
        flush();
    }
}
