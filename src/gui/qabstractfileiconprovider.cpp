// MIT License - see LICENSE
#include <QtGui/qabstractfileiconprovider.h>

namespace {

// The icon of the theme for `name`, or for the first of the names after it that the theme has.
QIcon themeIcon(std::initializer_list<const char *> names) {
    for (const char *name : names) {
        const QIcon icon = QIcon::fromTheme(QString::fromLatin1(name));
        if (!icon.isNull())
            return icon;
    }
    return QIcon();
}

} // namespace

/*!
    \class QAbstractFileIconProvider
    \inmodule NtGui
    \since 6.0
    \brief The QAbstractFileIconProvider class gives the icons and the type names of files.

    QFileSystemModel asks its icon provider for the icon and the type of each file. The icons come from the icon
    theme (QIcon::fromTheme()): \c folder, \c text-x-generic and the others of the freedesktop naming
    specification, and for a file the icon of its MIME type when the theme has one. QFileIconProvider adds the
    icons of the style where the theme has none.

    \differences
    The MIME type of a file is guessed from its suffix, not from its contents.
    \enddifferences

    \sa QFileIconProvider
*/

/*!
    \enum QAbstractFileIconProvider::IconType

    The kinds of icons there are.

    \value Computer The computer.
    \value Desktop The desktop.
    \value Trashcan The trash.
    \value Network The network.
    \value Drive A drive.
    \value Folder A folder.
    \value File A file.
*/

/*!
    \enum QAbstractFileIconProvider::Option

    \value DontUseCustomDirectoryIcons The icon of a folder is always the folder icon. Nt has no others.
*/

/*!
    Constructs an icon provider.
*/
QAbstractFileIconProvider::QAbstractFileIconProvider() = default;

/*!
    Destroys the icon provider.
*/
QAbstractFileIconProvider::~QAbstractFileIconProvider() = default;

/*!
    Returns the icon of the theme for \a type, or a null icon if the theme has none.
*/
QIcon QAbstractFileIconProvider::icon(IconType type) const {
    switch (type) {
    case Computer:
        return themeIcon({"computer"});
    case Desktop:
        return themeIcon({"user-desktop", "desktop"});
    case Trashcan:
        return themeIcon({"user-trash"});
    case Network:
        return themeIcon({"network-workgroup", "network-server"});
    case Drive:
        return themeIcon({"drive-harddisk"});
    case Folder:
        return themeIcon({"folder"});
    case File:
        return themeIcon({"text-x-generic", "unknown"});
    }
    return QIcon();
}

/*!
    Returns the icon of the file \a info: the folder icon for a folder, and for a file the icon of the theme for its
    kind, such as \c image-x-generic for a picture, or the file icon.
*/
QIcon QAbstractFileIconProvider::icon(const QFileInfo &info) const {
    if (info.isDir())
        return icon((info.isDir() && info.absoluteFilePath() == QStringLiteral("/")) ? Drive : Folder);
    const QString suffix = info.suffix().toLower();
    static const struct {
        const char *suffixes;
        const char *icon;
    } kinds[] = {
        {"png jpg jpeg gif bmp svg webp tif tiff ico", "image-x-generic"},
        {"mp3 wav ogg flac opus m4a", "audio-x-generic"},
        {"mp4 mkv avi mov webm", "video-x-generic"},
        {"zip tar gz bz2 xz 7z tgz", "package-x-generic"},
        {"html htm", "text-html"},
        {"pdf", "application-pdf"},
        {"sh py pl rb js", "text-x-script"},
        {"c cpp cc h hpp", "text-x-csrc"},
    };
    for (const auto &kind : kinds) {
        if (!suffix.isEmpty() && QString::fromLatin1(kind.suffixes).split(QChar(' ')).contains(suffix)) {
            const QIcon themed = QIcon::fromTheme(QString::fromLatin1(kind.icon));
            if (!themed.isNull())
                return themed;
        }
    }
    if (info.isExecutable() && !info.isDir()) {
        const QIcon themed = QIcon::fromTheme(QStringLiteral("application-x-executable"));
        if (!themed.isNull())
            return themed;
    }
    return icon(File);
}

/*!
    Returns the name of the type of the file \a info, as a file manager shows it: "Folder", "Drive", or for a file
    its suffix in upper case and "File", such as "PNG File", or "File" for one without a suffix.
*/
QString QAbstractFileIconProvider::type(const QFileInfo &info) const {
    if ((info.isDir() && info.absoluteFilePath() == QStringLiteral("/")))
        return QStringLiteral("Drive");
    if (info.isDir())
        return QStringLiteral("Folder");
    if (info.isSymLink())
        return QStringLiteral("Shortcut");
    const QString suffix = info.suffix();
    return suffix.isEmpty() ? QStringLiteral("File") : suffix.toUpper() + QStringLiteral(" File");
}

/*!
    \fn void QAbstractFileIconProvider::setOptions(Options options)

    Sets the options of the provider to \a options.
*/

/*!
    \fn QAbstractFileIconProvider::Options QAbstractFileIconProvider::options() const

    Returns the options of the provider.
*/
