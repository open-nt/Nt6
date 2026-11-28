// MIT License - see LICENSE
#pragma once

#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>

#include <cstddef>
#include <vector>

// Which window of the android platform a finger that comes down goes to. Kept apart from the platform, which needs
// the NDK, so that the tests can run it on any platform.
namespace QtAndroidPrivate {

// A visible top-level window, as the platform sees it.
struct TouchLayer {
    QRect geometry;           // where it is on the screen
    bool fillsScreen = false; // the window that fills the screen (not a dialog over one): every touch that gets to it
    bool modal = true;        // blocks the windows below it; a window that does not say is taken as modal
    bool sheet = false;       // a sheet (_nt_sheet), which a tap beside it closes when it is modal
    bool transparent = false; // input passes through it (Qt::WindowTransparentForInput)
};

struct TouchRoute {
    int index = -1;       // the window in the stack that takes the touch; -1 for none
    bool dismiss = false; // the touch closes that window instead, and goes nowhere
};

// The windows are hit from the top down (the topmost last in `stack`): the first that `at` is in takes the touch. A
// window that `at` is beside lets it through to the one below, unless it is modal: a modal sheet is closed by it, and
// another modal window takes it, as every touch went to the window on top before. The window that fills the screen
// takes whatever reaches it.
inline TouchRoute routeTouch(const std::vector<TouchLayer> &stack, const QPointF &at) {
    for (std::size_t i = stack.size(); i-- > 0;) {
        const TouchLayer &layer = stack[i];
        if (layer.transparent)
            continue;
        if (layer.fillsScreen || QRectF(layer.geometry).contains(at))
            return TouchRoute{static_cast<int>(i), false};
        if (layer.modal)
            return TouchRoute{static_cast<int>(i), layer.sheet};
    }
    return TouchRoute{};
}

} // namespace QtAndroidPrivate
