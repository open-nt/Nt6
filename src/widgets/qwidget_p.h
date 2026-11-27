// MIT License - see LICENSE
#pragma once

#include <QtGui/qicon.h>

#include "qwidgetwindow_p.h"

#include <QtCore/qlocale.h>
#include <QtCore/qpointer.h>
#include <QtGui/qaction.h>
#include <QtGui/qpalette.h>
#include <QtWidgets/qlayout.h>
#include <QtWidgets/qwidget.h>

#include <bitset>
#include <functional>
#include <optional>
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
    bool polished = false;         // the style polished it (ensurePolished())
    bool pendingShow = false;      // created below a visible parent, not shown yet
    bool shownOffscreen = false;   // a window shown with Qt::WA_DontShowOnScreen, which has no platform window
    bool disabled = false;         // setEnabled(false) on this widget
    QString title;
    Qt::WindowFlags windowFlags;
    QScreen *screen = nullptr; // set with setScreen(); the window's screen otherwise
    QString styleSheet;
    // Set while a QGraphicsProxyWidget shows this window in a scene: the window counts as shown without a native
    // window, and its updates go to the proxy.
    std::function<void(const QRegion &)> proxyUpdate;
    Qt::WindowModality modality = Qt::NonModal;
    QPalette palette;
    bool paletteSet = false;
    QFont font;
    bool fontSet = false;
    QStyle *style = nullptr;
    bool autoFill = false;
    bool mouseTracking = false;
    QString toolTip;
    Qt::ContextMenuPolicy contextMenuPolicy = Qt::DefaultContextMenu;
    QIcon icon;                    // of a window
    bool mouseTransparent = false; // not found by the mouse: the tool tip
    bool underMouse = false;
    QCursor cursor;
    bool cursorSet = false;
    bool updatesDisabled = false;
    bool windowModified = false;
    // -1: not set, so a window has it and another widget does not.
    int quitOnClose = -1;
    // The attributes that have no other member here.
    std::bitset<Qt::WA_AttributeCount> attributes;
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
    QMargins contentsMargins;
    QString statusTip;
    QString whatsThis;
    QString accessibleName;
    QString accessibleDescription;
    Qt::InputMethodHints inputMethodHints;
    QPointer<QGraphicsEffect> graphicsEffect; // a child of the widget
    QGraphicsProxyWidget *proxy = nullptr;    // the proxy that shows this window in a scene
    QPointer<QWidget> focusProxy;
    // Of a window: the pairs that QWidget::setTabOrder() put next to each other, in the order it was called.
    std::vector<std::pair<QPointer<QWidget>, QPointer<QWidget>>> tabOrder;
    std::optional<Qt::LayoutDirection> layoutDirection;
    std::optional<QLocale> locale;
    QPalette::ColorRole backgroundRole = QPalette::NoRole;
    QPalette::ColorRole foregroundRole = QPalette::NoRole;
    QRegion mask;
    QSize baseSize{0, 0};
    QSize sizeIncrement{0, 0};
    QRect normalGeometry; // of a window, before it was maximized or made full screen
    qreal windowOpacity = 1.0;
    QString windowFilePath;
    QString windowIconText;
    QString windowRole;
    QString accessibleIdentifier;
    int toolTipDuration = -1;
    bool tabletTracking = false;
    bool editFocus = false;
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
    // The title that the window of `w` shows: its window title with the marker for unsaved changes resolved.
    static QString shownTitle(const QWidget *w);
    // True if `w` or one of its ancestors in the window is not repainted.
    static bool updatesBlocked(const QWidget *w);
    static QWidget *focusWidgetOf(QWidget *top);
    static bool focusable(const QWidget *w);
    // The input method hints of `w` may be others now: the platform is told, if `w` has the focus.
    static void inputHintsChanged(const QWidget *w);
    static std::vector<QWidget *> tabChain(QWidget *top);
    static void setFocusWidget(QWidget *top, QWidget *w);
    static void clearReferences(QWidget *w);
    static void registerTopLevel(QWidget *w);
    static void unregisterTopLevel(QWidget *w);
    static void activateLayout(QWidget *w);
    // Makes `popup` the popup of the window of `w`, or removes it if it is null.
    static void setPopup(QWidget *w, QWidget *popup);
    // The widgets of the window `top` in tab order: those that take the focus by tabbing if `focusableOnly`, else all.
    static std::vector<QWidget *> focusChain(QWidget *top, bool focusableOnly);
    // Sends a QShortcutEvent to the widget that grabbed the shortcut `code` (a key and modifiers, as a QKeySequence
    // holds them) for key presses in `top`, unless `overridden` says the widget with the focus wants the key itself.
    // Returns true if a widget got the shortcut.
    static bool activateGrabbedShortcut(QWidget *top, int code, bool autoRepeat, int (*normalize)(int),
                                        const std::function<bool()> &overridden);
    // Opens the menu of the menu bar under `top` whose title has the mnemonic Alt and `key`; false if there is none.
    static bool activateMnemonic(QWidget *top, int key);
};

} // namespace QtPrivate
