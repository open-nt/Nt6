// MIT License - see LICENSE
//
// Helpers for the xcb tests: a second XCB connection plays the user and sends key and button events to a window.
#pragma once

#include <QtCore>

#include <xcb/xcb.h>

#include <cstdlib>
#include <cstring>

namespace xcbtest {

template <typename Cond> inline bool waitFor(Cond cond, int timeoutMs = 5000) {
    QElapsedTimer timer;
    timer.start();
    while (!cond()) {
        if (timer.elapsed() > timeoutMs)
            return false;
        QEventLoop loop;
        QTimer::singleShot(5, [&] { loop.quit(); });
        loop.exec();
    }
    return true;
}

inline xcb_atom_t internAtom(xcb_connection_t *c, const char *name) {
    xcb_intern_atom_reply_t *r =
        xcb_intern_atom_reply(c, xcb_intern_atom(c, 0, static_cast<uint16_t>(std::strlen(name)), name), nullptr);
    const xcb_atom_t a = r ? r->atom : 0;
    std::free(r);
    return a;
}

// A keycode that produces the unshifted keysym `sym`.
inline xcb_keycode_t keycodeFor(xcb_connection_t *c, xcb_keysym_t sym) {
    const xcb_setup_t *setup = xcb_get_setup(c);
    const int count = setup->max_keycode - setup->min_keycode + 1;
    xcb_get_keyboard_mapping_reply_t *r = xcb_get_keyboard_mapping_reply(
        c, xcb_get_keyboard_mapping(c, setup->min_keycode, static_cast<uint8_t>(count)), nullptr);
    xcb_keycode_t found = 0;
    if (r) {
        const xcb_keysym_t *syms = xcb_get_keyboard_mapping_keysyms(r);
        for (int i = 0; i < count && !found; ++i)
            if (syms[i * r->keysyms_per_keycode] == sym)
                found = static_cast<xcb_keycode_t>(setup->min_keycode + i);
        std::free(r);
    }
    return found;
}

inline void sendKey(xcb_connection_t *c, xcb_window_t w, xcb_keycode_t code, uint16_t state, uint8_t type) {
    xcb_key_press_event_t e;
    std::memset(&e, 0, sizeof e);
    e.response_type = type;
    e.detail = code;
    e.time = XCB_CURRENT_TIME;
    e.root = xcb_setup_roots_iterator(xcb_get_setup(c)).data->root;
    e.event = w;
    e.event_x = 5;
    e.event_y = 5;
    e.state = state;
    e.same_screen = 1;
    xcb_send_event(c, 0, w, type == XCB_KEY_PRESS ? XCB_EVENT_MASK_KEY_PRESS : XCB_EVENT_MASK_KEY_RELEASE,
                   reinterpret_cast<const char *>(&e));
    xcb_flush(c);
}

inline void sendButton(xcb_connection_t *c, xcb_window_t w, uint8_t button, uint8_t type, uint32_t time, int16_t x,
                       int16_t y) {
    xcb_button_press_event_t e;
    std::memset(&e, 0, sizeof e);
    e.response_type = type;
    e.detail = button;
    e.time = time;
    e.root = xcb_setup_roots_iterator(xcb_get_setup(c)).data->root;
    e.event = w;
    e.event_x = x;
    e.event_y = y;
    e.root_x = x;
    e.root_y = y;
    e.same_screen = 1;
    xcb_send_event(c, 0, w, type == XCB_BUTTON_PRESS ? XCB_EVENT_MASK_BUTTON_PRESS : XCB_EVENT_MASK_BUTTON_RELEASE,
                   reinterpret_cast<const char *>(&e));
    xcb_flush(c);
}

inline void sendMotion(xcb_connection_t *c, xcb_window_t w, int16_t x, int16_t y) {
    xcb_motion_notify_event_t e;
    std::memset(&e, 0, sizeof e);
    e.response_type = XCB_MOTION_NOTIFY;
    e.time = XCB_CURRENT_TIME;
    e.root = xcb_setup_roots_iterator(xcb_get_setup(c)).data->root;
    e.event = w;
    e.event_x = x;
    e.event_y = y;
    e.same_screen = 1;
    xcb_send_event(c, 0, w, XCB_EVENT_MASK_POINTER_MOTION, reinterpret_cast<const char *>(&e));
    xcb_flush(c);
}

} // namespace xcbtest
