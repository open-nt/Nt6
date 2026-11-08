// MIT License - see LICENSE
#pragma once

#include "qwidgetwindow_p.h"

#include <QtCore/qpointer.h>
#include <QtGui/qaction.h>
#include <QtGui/qpalette.h>
#include <QtWidgets/qlayout.h>
#include <QtWidgets/qwidget.h>

#include <functional>
#include <memory>
#include <vector>

struct QWidget::Data {
    struct ActionLink {
        QPointer<QAction> action;
        QObject *raw = nullptr;
        QMetaObject::Connection changed;
        QMetaObject::Connection destroyed;
    };
    std::vector<ActionLink> actions;
    // For a popup that is part of another one (the submenu of a menu): the popup that it belongs to.
    QPointer<QWidget> popupParent;
    // The widget that opened a popup: a press on it reaches it, so that it can switch or close the popup itself.
    QPointer<QWidget> popupAnchor;
    QRect geometry;
    bool geometrySet = false;
    QSize minimumSize{0, 0};
    QSize maximumSize{QLAYOUTSIZE_MAX, QLAYOUTSIZE_MAX};
    QSizePolicy sizePolicy;
    bool hiddenExplicitly = false; // hide() was called
    bool pendingShow = false;      // created below a visible parent, not shown yet
    bool disabled = false;         // setEnabled(false) on this widget
    QString title;
    Qt::WindowFlags windowFlags;
    Qt::WindowModality modality = Qt::NonModal;
    QPalette palette;
    bool paletteSet = false;
    QFont font;
    bool fontSet = false;
    QStyle *style = nullptr;
    bool autoFill = false;
    bool mouseTracking = false;
    QString toolTip;
    bool mouseTransparent = false; // not found by the mouse: the tool tip
    bool underMouse = false;
    // Called on a parent when a child widget is resized or its size hint changes: a scroll area follows its contents.
    std::function<void(QWidget *)> childChanged;
    Qt::FocusPolicy focusPolicy = Qt::NoFocus;
    QLayout *layout = nullptr;
    // Only top-level widgets:
    std::unique_ptr<QWidgetWindow> window;
    QPointer<QWidget> focusChild;
    // While the widget is painted:
    bool painting = false;
    QRegion paintClip;
};

namespace QtPrivate {

// Reaches the private parts of QWidget for the window, the layouts and the application.
struct WidgetAccess {
    static QWidget::Data &data(QWidget *w) { return *w->m_d; }
    static const QWidget::Data &data(const QWidget *w) { return *w->m_d; }

    // The widgets that are children of `w` in its window, bottom to top: not the child windows, such as dialogs.
    static std::vector<QWidget *> childWidgets(const QWidget *w);
    // The parent of `w` in its window: its parent widget, or null if `w` is a window.
    static QWidget *up(const QWidget *w) { return w->isWindow() ? nullptr : w->parentWidget(); }
    // Whether a modal window that is shown blocks the input of the window `top`.
    static bool blockedByModal(const QWidget *top);
    // Keeps the list of the modal windows that are shown, in the order they were shown.
    static void modalShown(QWidget *top);
    static void modalHidden(QWidget *top);
    // Position of `w` in the client area of its window.
    static QPoint windowOffset(const QWidget *w);
    // The part of `w` that is not clipped by an ancestor, in window coordinates.
    static QRect visibleRect(const QWidget *w);
    // `w` is shown as far as its own flags go; its ancestors may be hidden.
    static bool shownSelf(const QWidget *w);
    // The widget at `p` (in the coordinates of `root`): the deepest visible one, `root` if no child is.
    static QWidget *widgetAt(QWidget *root, const QPoint &p);
    static void paintRecursive(QWidget *w, const QPoint &origin, const QRect &parentVisible, const QRegion &dirty);
    static void paintTree(QWidget *top, const QRegion &region);
    // Paints `w` and its children on the device of the render() that is running; `origin` is where `w` is on it.
    static void renderRecursive(QWidget *w, const QPoint &origin, const QRect &parentVisible, const QRegion &dirty);
    static void applyLocalGeometry(QWidget *w, const QRect &rect);
    static void sendShowEvents(QWidget *w);
    static void sendHideEvents(QWidget *w);
    static void windowActivated(QWidget *top, bool active);
    static void setUnderMouse(QWidget *w, bool under);
    static QWidget *focusWidgetOf(QWidget *top);
    static bool focusable(const QWidget *w);
    static std::vector<QWidget *> tabChain(QWidget *top);
    static void setFocusWidget(QWidget *top, QWidget *w);
    static void clearReferences(QWidget *w);
    static void registerTopLevel(QWidget *w);
    static void unregisterTopLevel(QWidget *w);
    static void activateLayout(QWidget *w);
    // Makes `popup` the popup of the window of `w`, or removes it if it is null.
    static void setPopup(QWidget *w, QWidget *popup);
    // Opens the menu of the menu bar under `top` whose title has the mnemonic Alt and `key`; false if there is none.
    static bool activateMnemonic(QWidget *top, int key);
};

} // namespace QtPrivate
