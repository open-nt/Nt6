// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>

class QObject;
class QShortcut;
class QWindow;

namespace QtPrivate {

// Whether a shortcut whose parent is `parent` and whose context is `context` is active for key presses in `window`.
using ShortcutContextMatcher = bool (*)(QObject *parent, Qt::ShortcutContext context, QWindow *window);

// Nt Widgets installs the matcher that knows widgets; the default one knows windows only.
void setShortcutContextMatcher(ShortcutContextMatcher matcher);

// Activates the shortcut that `key` with `modifiers`, pressed in `window`, stands for. Returns true if a shortcut
// took the key press.
bool tryShortcut(QWindow *window, int key, Qt::KeyboardModifiers modifiers, bool autoRepeat);

} // namespace QtPrivate
