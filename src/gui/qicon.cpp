// MIT License - see LICENSE
#include <QtGui/qicon.h>

#include <QtCore/qvariant.h>

#include "qicon_p.h"

#include <QtCore/qdir.h>
#include <QtCore/qdiriterator.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qhash.h>
#include <QtGui/qpainter.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <vector>

namespace {

std::atomic<QtPrivate::ScalableIconRenderer> g_renderer{nullptr};

bool scalable(const QString &fileName) {
    return fileName.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive) ||
           fileName.endsWith(QLatin1String(".svgz"), Qt::CaseInsensitive);
}

// A gray, half transparent copy of `pixmap`, for the Disabled mode.
QPixmap disabledOf(const QPixmap &pixmap) {
    QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const QRgb p = image.pixel(x, y);
            const int g = (qGray(p) + 2 * 160) / 3;
            image.setPixel(x, y, qRgba(g, g, g, qAlpha(p) / 2));
        }
    QPixmap out = QPixmap::fromImage(image);
    out.setDevicePixelRatio(pixmap.devicePixelRatio());
    return out;
}

// The settings of the icon themes, shared by every icon.
struct Themes {
    std::mutex mutex;
    QString theme;
    bool themeSet = false;
    QString fallbackTheme = QStringLiteral("hicolor");
    QStringList searchPaths;
    bool searchPathsSet = false;
    QStringList fallbackPaths;
    // name of a theme directory -> icon name -> (size in pixels, 0 if scalable; path)
    std::map<QString, QHash<QString, QList<std::pair<int, QString>>>> indexes;
};

Themes &themes() {
    static Themes *t = new Themes; // never destroyed: icons can be made during static destruction
    return *t;
}

QStringList defaultSearchPaths() {
    QStringList paths;
    paths << QDir::homePath() + QStringLiteral("/.icons");
    const QString dataHome = qEnvironmentVariable("XDG_DATA_HOME", QDir::homePath() + QStringLiteral("/.local/share"));
    paths << dataHome + QStringLiteral("/icons");
    const QString dataDirs = qEnvironmentVariable("XDG_DATA_DIRS", QStringLiteral("/usr/local/share:/usr/share"));
    for (const QString &dir : dataDirs.split(QLatin1Char(':'), Qt::SkipEmptyParts))
        paths << dir + QStringLiteral("/icons");
    paths << QStringLiteral(":/icons");
    return paths;
}

QStringList searchPathsLocked(Themes &t) { return t.searchPathsSet ? t.searchPaths : defaultSearchPaths(); }

// The theme that a desktop without a setting most likely has: the first of the common ones that is installed.
QString defaultTheme(Themes &t) {
    for (const char *name : {"Adwaita", "breeze", "Yaru", "Humanity", "oxygen", "gnome", "hicolor"})
        for (const QString &dir : searchPathsLocked(t))
            if (QFileInfo::exists(dir + QLatin1Char('/') + QLatin1String(name) + QStringLiteral("/index.theme")))
                return QString::fromLatin1(name);
    return QStringLiteral("hicolor");
}

// The size that a directory of a theme holds, from its name ("16x16", "32", "scalable"); -1 if it says nothing.
int sizeOfDirectory(const QString &path) {
    for (const QString &part : path.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        if (part == QLatin1String("scalable") || part == QLatin1String("symbolic"))
            return 0;
        const QString digits = part.section(QLatin1Char('@'), 0, 0).section(QLatin1Char('x'), 0, 0);
        bool ok = false;
        const int size = digits.toInt(&ok);
        if (ok && size > 0)
            return size;
    }
    return -1;
}

// The icons of the theme directory `dir`, indexed the first time it is asked for.
const QHash<QString, QList<std::pair<int, QString>>> &indexOf(Themes &t, const QString &dir) {
    auto it = t.indexes.find(dir);
    if (it != t.indexes.end())
        return it->second;
    QHash<QString, QList<std::pair<int, QString>>> index;
    QDirIterator files(dir, QStringList{QStringLiteral("*.png"), QStringLiteral("*.svg"), QStringLiteral("*.svgz")},
                       QDir::Files, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while (files.hasNext()) {
        const QString path = files.next();
        const QFileInfo info(path);
        const int size = sizeOfDirectory(info.path().mid(dir.size()));
        index[info.completeBaseName()].append({scalable(path) ? 0 : std::max(size, 0), path});
    }
    return t.indexes.emplace(dir, std::move(index)).first->second;
}

// The themes that `theme` inherits, from its index.theme.
QStringList inheritsOf(Themes &t, const QString &theme) {
    for (const QString &dir : searchPathsLocked(t)) {
        const QString file = dir + QLatin1Char('/') + theme + QStringLiteral("/index.theme");
        if (!QFileInfo::exists(file))
            continue;
        QFile f(file);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        for (const QByteArray &line : f.readAll().split('\n'))
            if (line.startsWith("Inherits="))
                return QString::fromUtf8(line.mid(9).trimmed()).split(QLatin1Char(','), Qt::SkipEmptyParts);
        return {};
    }
    return {};
}

// The files of the theme icon `name`: from the current theme, the themes it inherits, the fallback theme, and the
// fallback paths.
QList<std::pair<int, QString>> findThemeIcon(const QString &name) {
    Themes &t = themes();
    std::lock_guard<std::mutex> lock(t.mutex);
    QStringList order;
    QStringList pending{t.themeSet ? t.theme : defaultTheme(t)};
    while (!pending.isEmpty() && order.size() < 16) {
        const QString theme = pending.takeFirst().trimmed();
        if (theme.isEmpty() || order.contains(theme))
            continue;
        order << theme;
        pending << inheritsOf(t, theme);
    }
    if (!order.contains(t.fallbackTheme))
        order << t.fallbackTheme;
    if (!order.contains(QStringLiteral("hicolor")))
        order << QStringLiteral("hicolor");
    for (const QString &theme : order) {
        QList<std::pair<int, QString>> found;
        for (const QString &dir : searchPathsLocked(t)) {
            const QString themeDir = dir + QLatin1Char('/') + theme;
            if (!QFileInfo(themeDir).isDir())
                continue;
            found << indexOf(t, themeDir).value(name);
        }
        if (!found.isEmpty())
            return found;
    }
    QStringList fallback = t.fallbackPaths;
    fallback << QStringLiteral("/usr/share/pixmaps");
    for (const QString &dir : fallback)
        for (const char *ext : {".png", ".svg"}) {
            const QString path = dir + QLatin1Char('/') + name + QLatin1String(ext);
            if (QFileInfo::exists(path))
                return {{scalable(path) ? 0 : -1, path}};
        }
    return {};
}

} // namespace

void QtPrivate::setScalableIconRenderer(ScalableIconRenderer renderer) { g_renderer.store(renderer); }

struct QIcon::Data {
    struct Entry {
        QPixmap pixmap;   // loaded, or given to addPixmap()
        QString fileName; // empty for a pixmap that was added
        QSize size;       // the size the file holds; invalid if not known until it is loaded
        Mode mode;
        State state;
    };
    std::vector<Entry> entries;
    QString name; // the theme name of an icon from fromTheme()
    std::map<std::tuple<int, int, int, int>, QPixmap> cache; // width, height, mode, state -> pixmap
    qint64 serial = nextSerial();

    static qint64 nextSerial() {
        static std::atomic<qint64> counter{0};
        return ++counter;
    }

    // Loads the pixmap of a file entry of a fixed size.
    static const QPixmap &loaded(Entry &e) {
        if (e.pixmap.isNull() && !e.fileName.isEmpty() && !scalable(e.fileName)) {
            e.pixmap = QPixmap(e.fileName);
            if (!e.size.isValid())
                e.size = e.pixmap.size();
        }
        return e.pixmap;
    }
};

/*!
    \class QIcon
    \inmodule NtGui
    \since 6.0
    \brief The QIcon class provides scalable icons in different modes and states.

    An icon is a set of pixmaps, or of image files read when they are first needed, in several sizes, for the
    modes Normal, Disabled, Active and Selected and the states On and Off. pixmap() picks the best one for a size;
    a size that is missing is scaled down from a larger one, and a mode without its own pixmap is made from the
    Normal one, grayed for Disabled. Buttons, actions, menus, tool bars and item views show icons:

    \code
    QIcon open = QIcon::fromTheme("document-open", QIcon(":/icons/open.png"));
    button->setIcon(open);
    button->setIconSize(QSize(20, 20));
    \endcode

    fromTheme() finds icons of the freedesktop.org icon themes in themeSearchPaths(): the theme of themeName(), the
    themes it inherits, then \c hicolor. SVG files are drawn at the size asked for when the application links
    Nt SVG (\c Nt::Svg); without it they are left out.

    \differences
    There is no QIconEngine, so icons cannot be drawn by code of the application, and no \c addFile() of a file
    whose size is only known by loading it ahead of time. The theme is not asked of the desktop: without
    setThemeName() it is the first installed of Adwaita, breeze, Yaru, Humanity, oxygen, gnome and hicolor. The
    Active and Selected modes look like Normal unless they have their own pixmaps. There is no
    \c QIcon::fromTheme(QIcon::ThemeIcon) of Qt 6.7.
    \enddifferences

    \sa QPixmap, QAction::setIcon(), QAbstractButton::setIcon()
*/

/*!
    \enum QIcon::Mode

    This enum describes how an icon is shown.

    \value Normal The user can interact with what the icon belongs to.
    \value Disabled What the icon belongs to cannot be used.
    \value Active The mouse is over what the icon belongs to.
    \value Selected What the icon belongs to is selected.
*/

/*!
    \enum QIcon::State

    This enum describes the state an icon is for.

    \value On What the icon belongs to is on, such as a checked button.
    \value Off What the icon belongs to is off.
*/

/*!
    Constructs a null icon.
*/
QIcon::QIcon() noexcept = default;

/*!
    Constructs an icon from \a pixmap.
*/
QIcon::QIcon(const QPixmap &pixmap) {
    if (!pixmap.isNull())
        addPixmap(pixmap);
}

/*!
    Constructs an icon from the image file \a fileName, which is read when the icon is first drawn. A file in the
    resources is named with a \c{:/} prefix. An SVG file is drawn at the size asked for.
*/
QIcon::QIcon(const QString &fileName) { addFile(fileName); }

/*!
    Constructs a copy of \a other, which shares its pixmaps.
*/
QIcon::QIcon(const QIcon &other) = default;

/*!
    Moves \a other into this icon, which leaves \a other null.
*/
QIcon::QIcon(QIcon &&other) noexcept = default;

/*!
    Destroys the icon.
*/
QIcon::~QIcon() = default;

/*!
    Makes this icon a copy of \a other and returns a reference to it.
*/
QIcon &QIcon::operator=(const QIcon &other) = default;

/*!
    Moves \a other into this icon and returns a reference to it.
*/
QIcon &QIcon::operator=(QIcon &&other) noexcept = default;

/*!
    \fn void QIcon::swap(QIcon &other)

    Swaps this icon with \a other.
*/

void QIcon::detach() {
    if (!m_d)
        m_d = std::make_shared<Data>();
    else if (m_d.use_count() > 1)
        m_d = std::make_shared<Data>(*m_d);
    m_d->cache.clear();
    m_d->serial = Data::nextSerial();
}

/*!
    Returns \c true if the icon has no pixmaps and no files.
*/
bool QIcon::isNull() const { return !m_d || m_d->entries.empty(); }

/*!
    Returns the theme name of an icon from fromTheme(), or an empty string.
*/
QString QIcon::name() const { return m_d ? m_d->name : QString(); }

/*!
    Returns a number that identifies the icon: copies have the same key until one is changed.
*/
qint64 QIcon::cacheKey() const { return m_d ? m_d->serial : 0; }

/*!
    Returns a pixmap of the icon for \a mode and \a state that fits in \a size: the largest of its pixmaps that does,
    or a larger one scaled down to fit. A pixmap is never scaled up. Returns a null pixmap for a null icon.

    \sa actualSize(), paint()
*/
QPixmap QIcon::pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) const {
    if (isNull() || size.isEmpty())
        return QPixmap();
    const auto key = std::make_tuple(size.width(), size.height(), static_cast<int>(mode), static_cast<int>(state));
    auto cached = m_d->cache.find(key);
    if (cached != m_d->cache.end())
        return cached->second;
    // The entries for the mode and state, else those for the state in Normal mode, else any.
    std::vector<Data::Entry *> candidates;
    for (const auto &[wantMode, wantState] : {std::pair(mode, state), std::pair(Normal, state), std::pair(mode, Off),
                                              std::pair(Normal, Off), std::pair(Normal, On)}) {
        for (Data::Entry &e : m_d->entries)
            if (e.mode == wantMode && e.state == wantState)
                candidates.push_back(&e);
        if (!candidates.empty())
            break;
    }
    if (candidates.empty())
        for (Data::Entry &e : m_d->entries)
            candidates.push_back(&e);
    const bool ownMode = candidates.front()->mode == mode;
    QPixmap result;
    // A scalable file is drawn at the size; otherwise the smallest pixmap that covers the size, or the largest.
    for (Data::Entry *e : candidates) {
        if (!e->fileName.isEmpty() && scalable(e->fileName)) {
            if (QtPrivate::ScalableIconRenderer render = g_renderer.load()) {
                const QImage image = render(e->fileName, size);
                if (!image.isNull()) {
                    result = QPixmap::fromImage(image);
                    break;
                }
            }
        }
    }
    if (result.isNull()) {
        const Data::Entry *best = nullptr;
        for (Data::Entry *e : candidates) {
            const QPixmap &p = Data::loaded(*e);
            if (p.isNull())
                continue;
            const QSize s = p.size();
            const bool covers = s.width() >= size.width() || s.height() >= size.height();
            if (!best) {
                best = e;
                continue;
            }
            const QSize b = best->pixmap.size();
            const bool bestCovers = b.width() >= size.width() || b.height() >= size.height();
            // The smallest pixmap that covers the size, or else the largest one.
            const bool smaller = s.width() * s.height() < b.width() * b.height();
            const bool larger = s.width() * s.height() > b.width() * b.height();
            if ((covers && (!bestCovers || smaller)) || (!covers && !bestCovers && larger))
                best = e;
        }
        if (best) {
            result = best->pixmap;
            if (result.width() > size.width() || result.height() > size.height())
                result = result.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
    if (!result.isNull() && mode == Disabled && !ownMode)
        result = disabledOf(result);
    m_d->cache.emplace(key, result);
    return result;
}

/*!
    \overload

    Returns a pixmap of the icon that fits in \a size device-independent pixels at \a devicePixelRatio: it has
    that many more pixels and the ratio set, so it is drawn at \a size on a screen of that ratio.
*/
QPixmap QIcon::pixmap(const QSize &size, qreal devicePixelRatio, QIcon::Mode mode, QIcon::State state) const {
    const qreal ratio = devicePixelRatio > 0 ? devicePixelRatio : 1.0;
    QPixmap p = pixmap(QSize(qRound(size.width() * ratio), qRound(size.height() * ratio)), mode, state);
    if (!p.isNull() && ratio != 1.0) {
        p = p.copy();
        p.setDevicePixelRatio(ratio);
    }
    return p;
}

/*!
    \fn QPixmap QIcon::pixmap(int w, int h, QIcon::Mode mode, QIcon::State state) const
    \overload

    Returns a pixmap that fits in \a w by \a h pixels, for \a mode and \a state.
*/

/*!
    \fn QPixmap QIcon::pixmap(int extent, QIcon::Mode mode, QIcon::State state) const
    \overload

    Returns a pixmap that fits in \a extent by \a extent pixels, for \a mode and \a state.
*/

/*!
    Returns the size of the pixmap that pixmap() gives for \a size, \a mode and \a state.
*/
QSize QIcon::actualSize(const QSize &size, QIcon::Mode mode, QIcon::State state) const {
    return pixmap(size, mode, state).size();
}

/*!
    Returns the sizes of the pixmaps and fixed-size files of the icon for \a mode and \a state. Scalable files have
    no size and are not listed.
*/
QList<QSize> QIcon::availableSizes(QIcon::Mode mode, QIcon::State state) const {
    QList<QSize> sizes;
    if (!m_d)
        return sizes;
    for (Data::Entry &e : m_d->entries) {
        if (e.mode != mode || e.state != state || (!e.fileName.isEmpty() && scalable(e.fileName)))
            continue;
        const QSize s = e.size.isValid() ? e.size : Data::loaded(e).size();
        if (s.isValid() && !sizes.contains(s))
            sizes.append(s);
    }
    return sizes;
}

/*!
    Draws the icon for \a mode and \a state with \a painter in \a rect, as large as fits, placed by \a alignment.
    The pixmap has the device pixel ratio of the painter's device, so it is sharp on high-resolution screens.
*/
void QIcon::paint(QPainter *painter, const QRect &rect, Qt::Alignment alignment, QIcon::Mode mode,
                  QIcon::State state) const {
    if (!painter || isNull() || rect.isEmpty())
        return;
    const qreal ratio = painter->device() && painter->device()->devicePixelRatio() > 0
                            ? painter->device()->devicePixelRatio()
                            : 1.0;
    const QPixmap p = pixmap(rect.size(), ratio, mode, state);
    if (p.isNull())
        return;
    const QSize logical(qRound(p.width() / p.devicePixelRatio()), qRound(p.height() / p.devicePixelRatio()));
    int x = rect.x(), y = rect.y();
    if (alignment & Qt::AlignHCenter)
        x += (rect.width() - logical.width()) / 2;
    else if (alignment & Qt::AlignRight)
        x += rect.width() - logical.width();
    if (alignment & Qt::AlignVCenter)
        y += (rect.height() - logical.height()) / 2;
    else if (alignment & Qt::AlignBottom)
        y += rect.height() - logical.height();
    painter->drawPixmap(QRect(QPoint(x, y), logical), p);
}

/*!
    \fn void QIcon::paint(QPainter *painter, int x, int y, int w, int h, Qt::Alignment alignment, QIcon::Mode mode, QIcon::State state) const
    \overload

    Draws the icon with \a painter in the rectangle at (\a x, \a y) of \a w by \a h, placed by \a alignment, for
    \a mode and \a state.
*/

/*!
    Adds \a pixmap to the icon for \a mode and \a state.

    \sa addFile()
*/
void QIcon::addPixmap(const QPixmap &pixmap, QIcon::Mode mode, QIcon::State state) {
    if (pixmap.isNull())
        return;
    detach();
    m_d->entries.push_back({pixmap, QString(), pixmap.size(), mode, state});
}

/*!
    Adds the image file \a fileName to the icon for \a mode and \a state, as a pixmap of \a size, or of the size of
    the file if \a size is not valid. The file is read when it is first needed. An SVG file is drawn at the size
    that is asked for.

    \sa addPixmap()
*/
void QIcon::addFile(const QString &fileName, const QSize &size, QIcon::Mode mode, QIcon::State state) {
    if (fileName.isEmpty())
        return;
    if (!QFileInfo::exists(fileName))
        return;
    detach();
    m_d->entries.push_back({QPixmap(), fileName, size, mode, state});
}

/*!
    Returns the icon \a name of the icon theme, such as \c document-open, or a null icon if the theme and the
    themes it inherits have none.

    \sa hasThemeIcon(), themeName(), themeSearchPaths()
*/
QIcon QIcon::fromTheme(const QString &name) {
    QIcon icon;
    const QList<std::pair<int, QString>> files = findThemeIcon(name);
    if (files.isEmpty())
        return icon;
    icon.detach();
    for (const auto &[size, path] : files)
        icon.m_d->entries.push_back({QPixmap(), path, size > 0 ? QSize(size, size) : QSize(), Normal, Off});
    icon.m_d->name = name;
    return icon;
}

/*!
    \overload

    Returns the icon \a name of the icon theme, or \a fallback if the theme has none.
*/
QIcon QIcon::fromTheme(const QString &name, const QIcon &fallback) {
    QIcon icon = fromTheme(name);
    return icon.isNull() ? fallback : icon;
}

/*!
    Returns \c true if the icon theme, or a theme it inherits, has the icon \a name.
*/
bool QIcon::hasThemeIcon(const QString &name) { return !findThemeIcon(name).isEmpty(); }

/*!
    Returns the name of the icon theme that fromTheme() looks in.

    \sa setThemeName()
*/
QString QIcon::themeName() {
    Themes &t = themes();
    std::lock_guard<std::mutex> lock(t.mutex);
    return t.themeSet ? t.theme : defaultTheme(t);
}

/*!
    Sets the name of the icon theme that fromTheme() looks in to \a name, the name of a directory with an
    \c index.theme in one of the themeSearchPaths().
*/
void QIcon::setThemeName(const QString &name) {
    Themes &t = themes();
    std::lock_guard<std::mutex> lock(t.mutex);
    t.theme = name;
    t.themeSet = true;
}

/*!
    Returns the theme that fromTheme() looks in after the current one and the themes it inherits: \c hicolor by
    default.
*/
QString QIcon::fallbackThemeName() {
    Themes &t = themes();
    std::lock_guard<std::mutex> lock(t.mutex);
    return t.fallbackTheme;
}

/*!
    Sets the theme that fromTheme() looks in last to \a name.
*/
void QIcon::setFallbackThemeName(const QString &name) {
    Themes &t = themes();
    std::lock_guard<std::mutex> lock(t.mutex);
    t.fallbackTheme = name;
}

/*!
    Returns the directories that hold icon themes: \c{~/.icons}, the \c icons directories of the XDG data
    directories, and \c{:/icons} in the resources.

    \sa setThemeSearchPaths()
*/
QStringList QIcon::themeSearchPaths() {
    Themes &t = themes();
    std::lock_guard<std::mutex> lock(t.mutex);
    return searchPathsLocked(t);
}

/*!
    Sets the directories that hold icon themes to \a paths.
*/
void QIcon::setThemeSearchPaths(const QStringList &paths) {
    Themes &t = themes();
    std::lock_guard<std::mutex> lock(t.mutex);
    t.searchPaths = paths;
    t.searchPathsSet = true;
    t.indexes.clear();
}

/*!
    Returns the directories that fromTheme() looks in for icons that no theme has, by file name; empty by default,
    after which \c{/usr/share/pixmaps} is searched.
*/
QStringList QIcon::fallbackSearchPaths() {
    Themes &t = themes();
    std::lock_guard<std::mutex> lock(t.mutex);
    return t.fallbackPaths;
}

/*!
    Sets the directories that fromTheme() looks in for icons that no theme has to \a paths.
*/
void QIcon::setFallbackSearchPaths(const QStringList &paths) {
    Themes &t = themes();
    std::lock_guard<std::mutex> lock(t.mutex);
    t.fallbackPaths = paths;
}

/*!
    \since 6.0

    Returns the icon as a QVariant, so that it can be given where a QVariant is taken, as in
    \c{item->setData(Qt::UserRole, value)}.
*/
QIcon::operator QVariant() const { return QVariant::fromValue(*this); }
