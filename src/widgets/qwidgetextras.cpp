// MIT License - see LICENSE
#include <QtWidgets/qwidget.h>

#include "qwidget_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdatastream.h>
#include <QtCore/qdebug.h>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qscreen.h>
#include <QtWidgets/qapplication.h>

#include <algorithm>

using QtPrivate::WidgetAccess;

namespace {

// A key that QWidget::grabShortcut() grabbed.
struct GrabbedShortcut {
    int id = 0;
    QPointer<QWidget> widget;
    QKeySequence key;
    Qt::ShortcutContext context = Qt::WindowShortcut;
    bool enabled = true;
    bool autoRepeat = true;
};

std::vector<GrabbedShortcut> &grabbedShortcuts() {
    static auto *list = new std::vector<GrabbedShortcut>;
    return *list;
}

int g_nextShortcutId = 1;

GrabbedShortcut *findGrab(const QWidget *w, int id) {
    for (GrabbedShortcut &g : grabbedShortcuts())
        if (g.id == id && g.widget == w)
            return &g;
    return nullptr;
}

void collectFocusChain(QWidget *w, bool focusableOnly, std::vector<QWidget *> &out) {
    if (focusableOnly) {
        if (!WidgetAccess::shownSelf(w))
            return;
        if (WidgetAccess::focusable(w))
            out.push_back(w);
    } else {
        out.push_back(w);
    }
    for (QWidget *child : WidgetAccess::childWidgets(w))
        collectFocusChain(child, focusableOnly, out);
}

// Sends `type` to `w` and the widgets under it that do not set the property themselves (`ownValue`).
template <typename OwnValue>
void sendToFollowers(QWidget *w, QEvent::Type type, OwnValue ownValue) {
    QEvent change(type);
    QCoreApplication::sendEvent(w, &change);
    for (QWidget *child : WidgetAccess::childWidgets(w))
        if (!ownValue(child))
            sendToFollowers(child, type, ownValue);
}

constexpr quint32 GeometryMagic = 0x1D9D0CB;
constexpr quint16 GeometryMajorVersion = 3;

} // namespace

namespace QtPrivate {

std::vector<QWidget *> WidgetAccess::focusChain(QWidget *top, bool focusableOnly) {
    std::vector<QWidget *> chain;
    collectFocusChain(top, focusableOnly, chain);
    auto &order = top->m_d->tabOrder;
    order.erase(std::remove_if(order.begin(), order.end(), [](const auto &pair) { return !pair.first || !pair.second; }),
                order.end());
    for (const auto &[first, second] : order) {
        auto at = std::find(chain.begin(), chain.end(), second.data());
        if (at == chain.end() || std::find(chain.begin(), chain.end(), first.data()) == chain.end())
            continue;
        chain.erase(at);
        chain.insert(std::find(chain.begin(), chain.end(), first.data()) + 1, second.data());
    }
    return chain;
}

bool WidgetAccess::activateGrabbedShortcut(QWidget *top, int code, bool autoRepeat, int (*normalize)(int),
                                           const std::function<bool()> &overridden) {
    auto &all = grabbedShortcuts();
    all.erase(std::remove_if(all.begin(), all.end(), [](const GrabbedShortcut &g) { return !g.widget; }), all.end());
    const QWidget *focus = focusWidgetOf(top);
    std::vector<GrabbedShortcut> matches;
    for (const GrabbedShortcut &g : all) {
        QWidget *w = g.widget;
        if (!g.enabled || (autoRepeat && !g.autoRepeat) || g.key.isEmpty() || normalize(g.key[0]) != code)
            continue;
        if (!w->isVisible() || !w->isEnabled())
            continue;
        bool active = false;
        switch (g.context) {
        case Qt::WidgetShortcut:
            active = focus == w;
            break;
        case Qt::WidgetWithChildrenShortcut:
            active = focus && (focus == w || w->isAncestorOf(focus));
            break;
        case Qt::WindowShortcut:
            active = w->window() == top;
            break;
        case Qt::ApplicationShortcut:
            active = true;
            break;
        }
        if (active)
            matches.push_back(g);
    }
    if (matches.empty() || overridden())
        return false;
    QShortcutEvent event(matches.front().key, matches.front().id, matches.size() > 1);
    QCoreApplication::sendEvent(matches.front().widget.data(), &event);
    return true;
}

} // namespace QtPrivate

/*!
    \since 6.0

    Returns the geometry of the window including its frame. Nt does not know
    the frame that the window system draws, so this is geometry().

    \sa geometry(), frameSize()
*/
QRect QWidget::frameGeometry() const { return geometry(); }

/*!
    \fn QSize QWidget::frameSize() const
    \since 6.0

    Returns the size of the window including its frame.

    \sa frameGeometry()
*/

/*!
    \since 6.0

    Returns the geometry the window has when it is neither maximized nor full
    screen. A child widget returns an empty rectangle.

    \sa showNormal(), saveGeometry()
*/
QRect QWidget::normalGeometry() const {
    if (!isWindow())
        return QRect();
    if (!(windowState() & (Qt::WindowMaximized | Qt::WindowFullScreen)) || !m_d->normalGeometry.isValid())
        return geometry();
    return m_d->normalGeometry;
}

/*!
    \since 6.0

    Returns the bounding rectangle of the visible children of the widget,
    without the child windows.

    \sa childrenRegion()
*/
QRect QWidget::childrenRect() const {
    QRect r;
    for (QWidget *child : WidgetAccess::childWidgets(this))
        if (!child->isHidden())
            r = r.united(child->geometry());
    return r;
}

/*!
    \since 6.0

    Returns the region that the visible children of the widget cover.

    \sa childrenRect()
*/
QRegion QWidget::childrenRegion() const {
    QRegion r;
    for (QWidget *child : WidgetAccess::childWidgets(this))
        if (!child->isHidden())
            r += child->geometry();
    return r;
}

/*!
    \since 6.0

    Returns the part of the widget that is not clipped by its ancestors, in
    its own coordinates; empty if the widget is not visible. Siblings that
    cover the widget are not taken out.
*/
QRegion QWidget::visibleRegion() const {
    if (!isVisible())
        return QRegion();
    return QRegion(WidgetAccess::visibleRect(this).translated(-WidgetAccess::windowOffset(this)));
}

/*!
    \since 6.0

    Returns the base size of the widget, from which sizeIncrement() steps.

    \sa setBaseSize()
*/
QSize QWidget::baseSize() const { return m_d->baseSize; }

/*!
    \since 6.0

    Sets the base size of the widget to \a size.

    \sa baseSize()
*/
void QWidget::setBaseSize(const QSize &size) { m_d->baseSize = size; }

/*!
    \fn void QWidget::setBaseSize(int basew, int baseh)
    \since 6.0
    \overload

    Sets the base size to \a basew by \a baseh pixels.
*/

/*!
    \since 6.0

    Returns the step in which the user resizes the window.

    \sa setSizeIncrement()
*/
QSize QWidget::sizeIncrement() const { return m_d->sizeIncrement; }

/*!
    \since 6.0

    Sets the step in which the user resizes the window to \a size. Nt keeps
    the value for the application; the window system is not told.

    \sa sizeIncrement()
*/
void QWidget::setSizeIncrement(const QSize &size) { m_d->sizeIncrement = size; }

/*!
    \fn void QWidget::setSizeIncrement(int w, int h)
    \since 6.0
    \overload

    Sets the size increment to \a w by \a h pixels.
*/

/*!
    \since 6.0

    Returns \c true if the preferred height of the widget depends on its
    width, as its size policy says.

    \sa heightForWidth()
*/
bool QWidget::hasHeightForWidth() const { return m_d->sizePolicy.hasHeightForWidth(); }

/*!
    \since 6.0

    Returns the preferred height of the widget at the width \a w, or -1 if it
    does not depend on the width. The default returns -1.

    \sa hasHeightForWidth()
*/
int QWidget::heightForWidth(int w) const {
    (void)w;
    return -1;
}

/*!
    \since 6.0

    Returns the geometry and state of the window, for restoreGeometry() to
    restore, in the format of Qt 6.

    \code
    settings.setValue("geometry", window.saveGeometry());
    // next time:
    window.restoreGeometry(settings.value("geometry").toByteArray());
    \endcode

    \sa restoreGeometry(), QMainWindow::saveState()
*/
QByteArray QWidget::saveGeometry() const {
    QByteArray array;
    QDataStream stream(&array, QIODeviceBase::WriteOnly);
    stream.setVersion(QDataStream::Qt_4_0);
    const QList<QScreen *> screens = QGuiApplication::screens();
    QScreen *current = screen();
    const auto screenNumber = static_cast<qint32>(std::max<qsizetype>(screens.indexOf(current), 0));
    const qint32 screenWidth = current ? current->geometry().width() : 0;
    const Qt::WindowStates state = windowState();
    stream << GeometryMagic << GeometryMajorVersion << quint16(0) << frameGeometry() << normalGeometry()
           << screenNumber << quint8(state.testFlag(Qt::WindowMaximized) ? 1 : 0)
           << quint8(state.testFlag(Qt::WindowFullScreen) ? 1 : 0) << screenWidth << geometry();
    return array;
}

/*!
    \since 6.0

    Restores the geometry and state of the window from \a geometry, which
    saveGeometry() made. Returns \c false, changing nothing, if \a geometry
    is not in that format.

    \sa saveGeometry()
*/
bool QWidget::restoreGeometry(const QByteArray &geometry) {
    if (geometry.size() < 4)
        return false;
    QDataStream stream(geometry);
    stream.setVersion(QDataStream::Qt_4_0);
    quint32 magic = 0;
    quint16 major = 0;
    quint16 minor = 0;
    stream >> magic >> major >> minor;
    if (magic != GeometryMagic || major < 1 || major > GeometryMajorVersion)
        return false;
    QRect frame;
    QRect normal;
    qint32 screenNumber = 0;
    quint8 maximized = 0;
    quint8 fullScreen = 0;
    stream >> frame >> normal >> screenNumber >> maximized >> fullScreen;
    QRect restored = normal;
    if (major > 1) {
        qint32 screenWidth = 0;
        stream >> screenWidth;
    }
    if (major > 2) {
        QRect client;
        stream >> client;
        if (client.isValid() && !maximized && !fullScreen)
            restored = client;
    }
    if (stream.status() != QDataStream::Ok)
        return false;
    if (restored.isValid())
        setGeometry(restored);
    m_d->normalGeometry = normal;
    if (fullScreen)
        setWindowState(Qt::WindowFullScreen);
    else if (maximized)
        setWindowState(Qt::WindowMaximized);
    return true;
}

/*!
    \since 6.0

    Scrolls the widget by \a dx pixels to the right and \a dy down: its
    children move with it and it is repainted.
*/
void QWidget::scroll(int dx, int dy) {
    if (dx == 0 && dy == 0)
        return;
    for (QWidget *child : WidgetAccess::childWidgets(this))
        child->move(child->pos() + QPoint(dx, dy));
    update();
}

/*!
    \since 6.0
    \overload

    Scrolls the part \a r of the widget by \a dx and \a dy; the children do
    not move. The part is repainted.
*/
void QWidget::scroll(int dx, int dy, const QRect &r) {
    if (dx != 0 || dy != 0)
        update(r);
}

/*!
    \since 6.0

    Returns the mask set with setMask(), or an empty region if there is none.
*/
QRegion QWidget::mask() const { return m_d->mask; }

/*!
    \since 6.0

    Sets the mask of the widget to \a region: only the part of the widget in
    it is meant to be shown. An empty region clears the mask.

    \sa clearMask()
*/
void QWidget::setMask(const QRegion &region) {
    m_d->mask = region;
    update();
}

/*!
    \since 6.0

    Removes the mask that setMask() set.
*/
void QWidget::clearMask() { setMask(QRegion()); }

/*!
    \since 6.0

    Returns the window of the widget, which is the widget that has the native
    window, or \c nullptr if the widget is a window.
*/
QWidget *QWidget::nativeParentWidget() const { return isWindow() ? nullptr : window(); }

/*!
    \since 6.0

    Returns the QGraphicsProxyWidget that shows the widget in a scene, or
    \c nullptr.
*/
QGraphicsProxyWidget *QWidget::graphicsProxyWidget() const { return m_d->proxy; }

/*!
    \since 6.0

    Places the widget just below \a w among its siblings. \a w must be a
    sibling of the widget.

    \sa raise(), lower()
*/
void QWidget::stackUnder(QWidget *w) {
    QWidget *p = WidgetAccess::up(this);
    if (!w || w == this || !p || WidgetAccess::up(w) != p)
        return;
    const QObjectList siblings = p->children();
    const auto at = siblings.indexOf(w);
    QObject::setParent(nullptr);
    QObject::setParent(p);
    for (qsizetype i = at; i < siblings.size(); ++i) {
        QObject *c = siblings.at(i);
        if (c != this && dynamic_cast<QWidget *>(c)) {
            c->setParent(nullptr);
            c->setParent(p);
        }
    }
    update();
}

/*!
    \since 6.0

    Sets the window flags of the widget to \a flags without telling the
    window system.

    \sa setWindowFlags()
*/
void QWidget::overrideWindowFlags(Qt::WindowFlags flags) { m_d->windowFlags = flags; }

/*!
    \since 6.0

    Sets the state of the window to \a state, as setWindowState() does.
*/
void QWidget::overrideWindowState(Qt::WindowStates state) { setWindowState(state); }

/*!
    \since 6.0

    Returns the native identifier of the window that contains the widget.

    \sa winId()
*/
WId QWidget::effectiveWinId() const { return window()->winId(); }

/*!
    \since 6.0

    Returns the native identifier of the widget if it is a window that has
    one; otherwise returns 0.
*/
WId QWidget::internalWinId() const { return isWindow() && m_d->window ? winId() : 0; }

/*!
    \since 6.0

    Makes the widget, a window, get a native window, as winId() does.
*/
void QWidget::createWinId() {
    if (isWindow())
        (void)winId();
}

/*!
    \since 6.0

    Returns the window whose native identifier is \a id, or \c nullptr.
*/
QWidget *QWidget::find(WId id) {
    if (id == 0)
        return nullptr;
    for (QWidget *top : QApplication::topLevelWidgets())
        if (top->internalWinId() == id)
            return top;
    return nullptr;
}

/*!
    \since 6.0

    Returns \c nullptr: Nt widgets paint through their window, not a backing
    store of their own.
*/
QBackingStore *QWidget::backingStore() const { return nullptr; }

/*!
    \since 6.0

    Returns \c nullptr: Nt widgets have no painter shared between them.
*/
QPainter *QWidget::sharedPainter() const { return nullptr; }

/*!
    \since 6.0

    Returns the opacity of the window, from 0.0 (transparent) to 1.0 (opaque).

    \sa setWindowOpacity()
*/
qreal QWidget::windowOpacity() const { return window()->m_d->windowOpacity; }

/*!
    \since 6.0

    Sets the opacity of the window to \a level, kept between 0.0 and 1.0. Nt
    keeps the value; the window system does not blend the window.
*/
void QWidget::setWindowOpacity(qreal level) { window()->m_d->windowOpacity = std::clamp(level, 0.0, 1.0); }

/*!
    \since 6.0

    Returns the path of the file that the window shows.

    \sa setWindowFilePath()
*/
QString QWidget::windowFilePath() const { return m_d->windowFilePath; }

/*!
    \since 6.0

    Sets the path of the file that the window shows to \a filePath. While the
    window has no title, its title is the file name of the path.

    \sa windowFilePath()
*/
void QWidget::setWindowFilePath(const QString &filePath) {
    m_d->windowFilePath = filePath;
    if (isWindow() && windowTitle().isEmpty() && !filePath.isEmpty()) {
        const auto slash = filePath.lastIndexOf(QLatin1Char('/'));
        const QString name = filePath.mid(slash + 1);
        if (m_d->window)
            m_d->window->setTitle(name + QStringLiteral("[*]"));
    }
}

/*!
    \since 6.0

    Returns the text that the window system shows with the icon of the
    minimized window.

    \sa setWindowIconText()
*/
QString QWidget::windowIconText() const { return m_d->windowIconText; }

/*!
    \since 6.0

    Sets the icon text of the window to \a text, and emits
    windowIconTextChanged().
*/
void QWidget::setWindowIconText(const QString &text) {
    if (m_d->windowIconText == text)
        return;
    m_d->windowIconText = text;
    QEvent change(QEvent::IconTextChange);
    QCoreApplication::sendEvent(this, &change);
    emit windowIconTextChanged(text);
}

/*!
    \since 6.0

    Returns the role of the window, which X11 window managers use.
*/
QString QWidget::windowRole() const { return m_d->windowRole; }

/*!
    \since 6.0

    Sets the role of the window to \a role.
*/
void QWidget::setWindowRole(const QString &role) { m_d->windowRole = role; }

/*!
    \since 6.0

    Returns \c true if the widget is enabled as far as \a ancestor goes: if
    neither it nor a widget between it and \a ancestor was disabled. With a
    null \a ancestor, this is isEnabled().
*/
bool QWidget::isEnabledTo(const QWidget *ancestor) const {
    for (const QWidget *w = this; w && w != ancestor; w = WidgetAccess::up(w))
        if (w->m_d->disabled)
            return false;
    return true;
}

/*!
    \since 6.0

    Returns the layout direction of the widget: the one set with
    setLayoutDirection(), or else that of its parent, or of the application.

    \sa QGuiApplication::layoutDirection()
*/
Qt::LayoutDirection QWidget::layoutDirection() const {
    for (const QWidget *w = this; w; w = w->parentWidget())
        if (w->m_d->layoutDirection)
            return *w->m_d->layoutDirection;
    return QGuiApplication::layoutDirection();
}

/*!
    \since 6.0

    Sets the layout direction of the widget and the children that have none
    of their own to \a direction; they receive a QEvent::LayoutDirectionChange
    event.

    \sa unsetLayoutDirection()
*/
void QWidget::setLayoutDirection(Qt::LayoutDirection direction) {
    const Qt::LayoutDirection old = layoutDirection();
    m_d->layoutDirection = direction;
    if (old != direction)
        sendToFollowers(this, QEvent::LayoutDirectionChange,
                        [](const QWidget *w) { return w->m_d->layoutDirection.has_value(); });
}

/*!
    \since 6.0

    Makes the widget follow the layout direction of its parent again.

    \sa setLayoutDirection()
*/
void QWidget::unsetLayoutDirection() {
    const Qt::LayoutDirection old = layoutDirection();
    m_d->layoutDirection.reset();
    if (old != layoutDirection())
        sendToFollowers(this, QEvent::LayoutDirectionChange,
                        [](const QWidget *w) { return w->m_d->layoutDirection.has_value(); });
}

/*!
    \fn bool QWidget::isLeftToRight() const
    \since 6.0

    Returns \c true if the layout direction of the widget is Qt::LeftToRight.
*/

/*!
    \fn bool QWidget::isRightToLeft() const
    \since 6.0

    Returns \c true if the layout direction of the widget is Qt::RightToLeft.
*/

/*!
    \since 6.0

    Returns the locale of the widget: the one set with setLocale(), or else
    that of its parent, or the default locale.
*/
QLocale QWidget::locale() const {
    for (const QWidget *w = this; w; w = w->parentWidget())
        if (w->m_d->locale)
            return *w->m_d->locale;
    return QLocale();
}

/*!
    \since 6.0

    Sets the locale of the widget and the children that have none of their
    own to \a locale; they receive a QEvent::LocaleChange event.

    \sa unsetLocale()
*/
void QWidget::setLocale(const QLocale &locale) {
    m_d->locale = locale;
    sendToFollowers(this, QEvent::LocaleChange, [](const QWidget *w) { return w->m_d->locale.has_value(); });
}

/*!
    \since 6.0

    Makes the widget follow the locale of its parent again.
*/
void QWidget::unsetLocale() {
    if (!m_d->locale)
        return;
    m_d->locale.reset();
    sendToFollowers(this, QEvent::LocaleChange, [](const QWidget *w) { return w->m_d->locale.has_value(); });
}

/*!
    \since 6.0

    Returns the palette role that the background of the widget is painted
    with: the one set with setBackgroundRole(), or QPalette::Window.
*/
QPalette::ColorRole QWidget::backgroundRole() const {
    return m_d->backgroundRole != QPalette::NoRole ? m_d->backgroundRole : QPalette::Window;
}

/*!
    \since 6.0

    Sets the palette role of the background of the widget to \a role, which
    autoFillBackground() fills.
*/
void QWidget::setBackgroundRole(QPalette::ColorRole role) {
    m_d->backgroundRole = role;
    update();
}

/*!
    \since 6.0

    Returns the palette role of the foreground of the widget: the one set
    with setForegroundRole(), or the role that goes with backgroundRole().
*/
QPalette::ColorRole QWidget::foregroundRole() const {
    if (m_d->foregroundRole != QPalette::NoRole)
        return m_d->foregroundRole;
    switch (backgroundRole()) {
    case QPalette::Button:
        return QPalette::ButtonText;
    case QPalette::Base:
    case QPalette::AlternateBase:
        return QPalette::Text;
    case QPalette::Dark:
    case QPalette::Shadow:
        return QPalette::Light;
    case QPalette::Highlight:
        return QPalette::HighlightedText;
    case QPalette::ToolTipBase:
        return QPalette::ToolTipText;
    default:
        return QPalette::WindowText;
    }
}

/*!
    \since 6.0

    Sets the palette role of the foreground of the widget to \a role.
*/
void QWidget::setForegroundRole(QPalette::ColorRole role) {
    m_d->foregroundRole = role;
    update();
}

/*!
    \since 6.0

    Returns the widget that takes the focus for this one, or \c nullptr.

    \sa setFocusProxy()
*/
QWidget *QWidget::focusProxy() const { return m_d->focusProxy.data(); }

/*!
    \since 6.0

    Makes \a w take the focus whenever this widget is given it, as a line
    edit does for the spin box around it. A null \a w removes the proxy.
*/
void QWidget::setFocusProxy(QWidget *w) {
    for (const QWidget *p = w; p; p = p->m_d->focusProxy.data()) {
        if (p == this) {
            qWarning("QWidget::setFocusProxy: %s would be a proxy of itself", qPrintable(objectName()));
            return;
        }
    }
    m_d->focusProxy = w;
}

/*!
    \since 6.0

    Puts \a second right after \a first in the tab order of their window.
    Both must be in the same window.

    \code
    QWidget::setTabOrder(nameEdit, emailEdit);
    QWidget::setTabOrder(emailEdit, sendButton);
    \endcode

    \sa nextInFocusChain()
*/
void QWidget::setTabOrder(QWidget *first, QWidget *second) {
    if (!first || !second || first == second)
        return;
    while (QWidget *proxy = first->focusProxy())
        first = proxy;
    while (QWidget *proxy = second->focusProxy())
        second = proxy;
    QWidget *top = first->window();
    if (second->window() != top) {
        qWarning("QWidget::setTabOrder: the widgets are in different windows");
        return;
    }
    auto &order = top->m_d->tabOrder;
    order.erase(std::remove_if(order.begin(), order.end(), [second](const auto &pair) { return pair.second == second; }),
                order.end());
    order.emplace_back(first, second);
}

/*!
    \since 6.0

    Returns the widget after this one in the focus chain of its window: the
    order in which tabbing visits the widgets, counting those that do not
    take the focus. The chain wraps around.

    \sa previousInFocusChain(), setTabOrder()
*/
QWidget *QWidget::nextInFocusChain() const {
    const std::vector<QWidget *> chain = WidgetAccess::focusChain(window(), false);
    const auto at = std::find(chain.begin(), chain.end(), this);
    if (at == chain.end())
        return const_cast<QWidget *>(this); // NOLINT(cppcoreguidelines-pro-type-const-cast): Qt's signature
    return std::next(at) == chain.end() ? chain.front() : *std::next(at);
}

/*!
    \since 6.0

    Returns the widget before this one in the focus chain of its window.

    \sa nextInFocusChain()
*/
QWidget *QWidget::previousInFocusChain() const {
    const std::vector<QWidget *> chain = WidgetAccess::focusChain(window(), false);
    const auto at = std::find(chain.begin(), chain.end(), this);
    if (at == chain.end())
        return const_cast<QWidget *>(this); // NOLINT(cppcoreguidelines-pro-type-const-cast): Qt's signature
    return at == chain.begin() ? chain.back() : *std::prev(at);
}

/*!
    \since 6.0

    Returns \c true if the widget has the edit focus, which keypad navigation
    gives.
*/
bool QWidget::hasEditFocus() const { return m_d->editFocus && hasFocus(); }

/*!
    \since 6.0

    Gives the widget the edit focus if \a enable is \c true, and the focus
    with it.
*/
void QWidget::setEditFocus(bool enable) {
    if (enable && !hasFocus())
        setFocus();
    m_d->editFocus = enable;
}

/*!
    \since 6.0

    Returns \c true if tablet tracking is on.
*/
bool QWidget::hasTabletTracking() const { return m_d->tabletTracking; }

/*!
    \since 6.0

    Turns tablet tracking on if \a enable is \c true. Nt has no tablet events,
    so the value is only kept.
*/
void QWidget::setTabletTracking(bool enable) { m_d->tabletTracking = enable; }

/*!
    \since 6.0

    Adds a shortcut for \a key, active in \a context, and returns its id. When
    the key is pressed, the widget receives a QShortcutEvent with that id.
    QShortcut and QAction::setShortcut() are simpler to use.

    \sa releaseShortcut(), setShortcutEnabled()
*/
int QWidget::grabShortcut(const QKeySequence &key, Qt::ShortcutContext context) {
    if (key.isEmpty())
        return 0;
    GrabbedShortcut grab;
    grab.id = g_nextShortcutId++;
    grab.widget = this;
    grab.key = key;
    grab.context = context;
    grabbedShortcuts().push_back(grab);
    return grab.id;
}

/*!
    \since 6.0

    Removes the shortcut \a id that grabShortcut() added.
*/
void QWidget::releaseShortcut(int id) {
    auto &all = grabbedShortcuts();
    all.erase(std::remove_if(all.begin(), all.end(),
                             [&](const GrabbedShortcut &g) { return g.id == id && g.widget == this; }),
              all.end());
}

/*!
    \since 6.0

    Enables the shortcut \a id if \a enable is \c true, and disables it otherwise.
*/
void QWidget::setShortcutEnabled(int id, bool enable) {
    if (GrabbedShortcut *g = findGrab(this, id))
        g->enabled = enable;
}

/*!
    \since 6.0

    Makes the shortcut \a id fire again while its key is held down if
    \a enable is \c true.
*/
void QWidget::setShortcutAutoRepeat(int id, bool enable) {
    if (GrabbedShortcut *g = findGrab(this, id))
        g->autoRepeat = enable;
}

/*!
    \since 6.0

    Returns the value of the input method property \a query of the widget.
    The default answers Qt::ImEnabled and Qt::ImHints; an editor reimplements
    it.
*/
QVariant QWidget::inputMethodQuery(Qt::InputMethodQuery query) const {
    switch (query) {
    case Qt::ImEnabled:
        return QVariant(testAttribute(Qt::WA_InputMethodEnabled));
    case Qt::ImHints:
        return QVariant(static_cast<int>(inputMethodHints().toInt()));
    default:
        return QVariant();
    }
}

/*!
    \since 6.0

    Tells the input method that the input method property \a query of the
    widget, such as where the cursor is, changed. Nt has no input method
    that follows it, so this does nothing.
*/
void QWidget::updateMicroFocus(Qt::InputMethodQuery query) { (void)query; }

/*!
    \since 6.0

    Handles a native event of the window system of type \a eventType, at
    \a message, and returns \c true to stop it from being handled further,
    with the result in \a result. Nt does not deliver native events to
    widgets; the default returns \c false.
*/
bool QWidget::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
    (void)eventType;
    (void)message;
    (void)result;
    return false;
}

/*!
    \since 6.0

    Returns the identifier of the widget for accessibility tools.
*/
QString QWidget::accessibleIdentifier() const { return m_d->accessibleIdentifier; }

/*!
    \since 6.0

    Sets the identifier of the widget for accessibility tools to \a identifier.
*/
void QWidget::setAccessibleIdentifier(const QString &identifier) { m_d->accessibleIdentifier = identifier; }

/*!
    \since 6.0

    Returns how long the tool tip of the widget stays, in milliseconds; -1,
    the default, lets its length decide.
*/
int QWidget::toolTipDuration() const { return m_d->toolTipDuration; }

/*!
    \since 6.0

    Sets how long the tool tip of the widget stays to \a msec milliseconds.
*/
void QWidget::setToolTipDuration(int msec) { m_d->toolTipDuration = msec; }

/*!
    \fn QWidget *QWidget::topLevelWidget() const
    \since 6.0

    Returns window(); kept for code written for older Qt.
*/

/*!
    \fn QFontInfo QWidget::fontInfo() const
    \since 6.0

    Returns the font information of the font of the widget.
*/

/*!
    \fn void QWidget::setHidden(bool hidden)
    \since 6.0

    Hides the widget if \a hidden is \c true, and shows it otherwise.
*/
