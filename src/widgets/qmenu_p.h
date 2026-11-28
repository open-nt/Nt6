// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qmenu.h>

namespace QtPrivate {

// What a style asks of a menu beyond Qt's API: the actions whose rows it shows, which are those of a submenu that is
// open in its place, and the row that goes back from one.
struct MenuAccess {
    static QList<QAction *> shownActions(const QMenu *menu) { return menu->shownActions(); }
    static bool isBackAction(const QMenu *menu, const QAction *action) {
        return action && action == menu->m_backAction && !menu->m_inPlace.empty();
    }
};

} // namespace QtPrivate
