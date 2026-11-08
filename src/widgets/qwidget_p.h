// MIT License - see LICENSE
#pragma once

#include "qwidgetwindow_p.h"

#include <QtCore/qpointer.h>
#include <QtGui/qpalette.h>
#include <QtWidgets/qlayout.h>
#include <QtWidgets/qwidget.h>

#include <memory>
#include <vector>

struct QWidget::Data {
    QRect geometry;
    bool geometrySet = false;
    QSize minimumSize{0, 0};
    QSize maximumSize{QLAYOUTSIZE_MAX, QLAYOUTSIZE_MAX};
    QSizePolicy sizePolicy;
    bool hiddenExplicitly = false; // hide() was called
    bool pendingShow = false;      // created below a visible parent, not shown yet
    bool disabled = false;         // setEnabled(false) on this widget
    QString title;
    QPalette palette;
    bool paletteSet = false;
    QFont font;
    bool fontSet = false;
    QStyle *style = nullptr;
    bool autoFill = false;
    bool mouseTracking = false;
    bool underMouse = false;
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

    // The widgets that are children of `w`, bottom to top.
    static std::vector<QWidget *> childWidgets(const QWidget *w);
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
};

} // namespace QtPrivate
