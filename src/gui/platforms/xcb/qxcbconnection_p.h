// MIT License - see LICENSE
#pragma once

#include "qpa/qplatformscreen_p.h"

#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtGui/qregion.h>

#include <xcb/xcb.h>

#include <memory>
#include <unordered_map>
#include <vector>

class QSocketNotifier;
class QXcbWindow;

class QXcbScreen : public QPlatformScreen {
public:
    QXcbScreen(xcb_screen_t *screen, int number, qreal scale);

    QRect geometry() const override { return m_geometry; }
    int depth() const override { return m_depth; }
    QString name() const override { return m_name; }
    QSizeF physicalSize() const override { return m_physicalSize; }
    qreal devicePixelRatio() const override { return m_scale; }
    xcb_window_t root() const { return m_root; }

private:
    QRect m_geometry;
    int m_depth;
    QString m_name;
    QSizeF m_physicalSize;
    xcb_window_t m_root;
    qreal m_scale;
};

// One connection to the X server. It reads the events in the thread of the application object, from the
// event loop, through a socket notifier, and hands them to the windows.
class QXcbConnection : public QObject {
public:
    static std::unique_ptr<QXcbConnection> create(QString *error);
    ~QXcbConnection() override;

    xcb_connection_t *xcb() const { return m_connection; }
    xcb_screen_t *screen() const { return m_screen; }
    QXcbScreen *platformScreen() const { return m_platformScreen.get(); }
    xcb_visualid_t visual() const { return m_screen->root_visual; }
    int screenNumber() const { return m_screenNumber; }
    uint8_t depth() const { return m_screen->root_depth; }
    size_t maxRequestBytes() const;
    // Native pixels of the X server per unit of window coordinates.
    qreal scale() const { return m_platformScreen->devicePixelRatio(); }
    int toNative(int logical) const { return qRound(logical * scale()); }
    int toLogical(int native) const { return qRound(native / scale()); }
    // The smallest rectangle of window coordinates that covers `native`, a rectangle of X pixels.
    QRect toLogicalCovering(const QRect &native) const;

    // The cursor of the X server for a shape: a glyph of its cursor font, made the first time it is asked
    // for. XCB_NONE, which shows the cursor of the parent window, if it cannot be made.
    xcb_cursor_t cursor(Qt::CursorShape shape);
    // Where the pointer is, in the logical coordinates of the screen.
    QPoint pointerPosition() const;
    void warpPointer(const QPoint &logical);

    void registerWindow(xcb_window_t id, QXcbWindow *window) { m_windows[id] = window; }
    void unregisterWindow(xcb_window_t id) { m_windows.erase(id); }
    QXcbWindow *window(xcb_window_t id) const;

    xcb_atom_t atom(const char *name);
    xcb_atom_t wmProtocols() const { return m_wmProtocols; }
    xcb_atom_t wmDeleteWindow() const { return m_wmDeleteWindow; }
    xcb_atom_t netWmPing() const { return m_netWmPing; }
    xcb_atom_t netWmName() const { return m_netWmName; }
    xcb_atom_t netWmPid() const { return m_netWmPid; }
    xcb_atom_t utf8String() const { return m_utf8String; }

    void flush();
    // Reads and handles the events that are waiting.
    void processEvents();
    // Makes sure processEvents() runs soon, for events that a round trip pulled into XCB's queue.
    void scheduleProcessing();
    // Schedules processing if XCB's queue holds an event that no socket notification will report.
    void checkQueue();
    bool event(QEvent *e) override;

    // Keyboard
    struct Key {
        int qtKey;
        QString text;
    };
    Key translateKey(xcb_keycode_t code, uint16_t state) const;
    static Qt::KeyboardModifiers translateModifiers(uint16_t state);
    void updateKeyboardMapping();

private:
    QXcbConnection(xcb_connection_t *connection, int screenNumber);
    void handleEvent(xcb_generic_event_t *event);
    void handleKey(xcb_generic_event_t *event, bool autoRepeat);
    void handleButton(xcb_generic_event_t *event);
    void handleConfigure(xcb_generic_event_t *event);
    void handleClientMessage(xcb_generic_event_t *event);
    xcb_generic_event_t *nextEvent();
    // The cursor of the user's theme for a shape, or XCB_NONE without libxcb-cursor or a theme that has it.
    xcb_cursor_t themeCursor(Qt::CursorShape shape);
    struct CursorLibrary {
        int (*contextNew)(xcb_connection_t *, xcb_screen_t *, void **);
        xcb_cursor_t (*loadCursor)(void *, const char *);
        void (*contextFree)(void *);
    };
    static const CursorLibrary &cursorLibrary();

    xcb_connection_t *m_connection;
    xcb_screen_t *m_screen = nullptr;
    int m_screenNumber = 0;
    std::unique_ptr<QXcbScreen> m_platformScreen;
    std::unordered_map<xcb_window_t, QXcbWindow *> m_windows;
    std::unordered_map<int, xcb_cursor_t> m_cursors;
    xcb_font_t m_cursorFont = XCB_NONE;
    // The xcb_cursor_context_t of libxcb-cursor, which loads the cursors of the theme, or null without it.
    void *m_cursorContext = nullptr;
    bool m_cursorContextTried = false;
    std::unordered_map<std::string, xcb_atom_t> m_atoms;
    xcb_atom_t m_wmProtocols = 0;
    xcb_atom_t m_wmDeleteWindow = 0;
    xcb_atom_t m_netWmPing = 0;
    xcb_atom_t m_netWmName = 0;
    xcb_atom_t m_netWmPid = 0;
    xcb_atom_t m_utf8String = 0;
    QSocketNotifier *m_notifier = nullptr;
    bool m_processScheduled = false;
    bool m_processing = false;
    xcb_generic_event_t *m_pending = nullptr;
    size_t m_maxRequestBytes = 0;

    std::vector<xcb_keysym_t> m_keysyms;
    int m_keysymsPerKeycode = 0;
    xcb_keycode_t m_minKeycode = 0;
    xcb_keycode_t m_maxKeycode = 0;

    Qt::MouseButtons m_buttons;
    uint32_t m_lastPressTime = 0;
    int m_lastPressButton = 0;
    QPoint m_lastPressPos;
    std::unordered_map<xcb_window_t, QRegion> m_exposed;
};
