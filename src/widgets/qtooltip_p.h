// MIT License - see LICENSE
#pragma once

class QWidget;
class QPoint;

namespace QtPrivate {

// How long the mouse rests on a widget, in milliseconds, before the window asks it for a tool tip; the tests make it short.
int &toolTipDelay();

// The mouse moved to `pos` in `under`: takes the tool tip away unless it is still for that widget, and returns
// whether one is shown.
bool toolTipFollowsMouse(QWidget *under, const QPoint &pos);

} // namespace QtPrivate
