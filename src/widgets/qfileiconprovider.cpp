// MIT License - see LICENSE
#include <QtWidgets/qfileiconprovider.h>

#include <QtWidgets/qapplication.h>
#include <QtWidgets/qstyle.h>

/*!
    \class QFileIconProvider
    \inmodule NtWidgets
    \since 6.0
    \brief The QFileIconProvider class gives the icons of files, from the icon theme or the style.

    It is the icon provider of QFileSystemModel and QFileDialog: the icons of the theme, and the standard icons of the
    style (QStyle::SP_DirIcon, QStyle::SP_FileIcon and the others) where the theme has none, so that there is
    always an icon.

    \differences
    None beyond those of QAbstractFileIconProvider.
    \enddifferences

    \sa QAbstractFileIconProvider, QFileSystemModel
*/

/*!
    Constructs an icon provider.
*/
QFileIconProvider::QFileIconProvider() = default;

/*!
    Destroys the icon provider.
*/
QFileIconProvider::~QFileIconProvider() = default;

/*!
    \reimp

    Returns the icon of the theme for \a type, or the standard icon of the style.
*/
QIcon QFileIconProvider::icon(IconType type) const {
    QIcon themed = QAbstractFileIconProvider::icon(type);
    if (!themed.isNull() || !qApp)
        return themed;
    QStyle::StandardPixmap standard = QStyle::SP_FileIcon;
    switch (type) {
    case Computer:
        standard = QStyle::SP_ComputerIcon;
        break;
    case Desktop:
        standard = QStyle::SP_DesktopIcon;
        break;
    case Trashcan:
        standard = QStyle::SP_TrashIcon;
        break;
    case Network:
        standard = QStyle::SP_DriveNetIcon;
        break;
    case Drive:
        standard = QStyle::SP_DriveHDIcon;
        break;
    case Folder:
        standard = QStyle::SP_DirIcon;
        break;
    case File:
        standard = QStyle::SP_FileIcon;
        break;
    }
    return QApplication::style()->standardIcon(standard);
}

/*!
    \reimp

    Returns the icon of the theme for the file \a info, or the folder or file icon of the style.
*/
QIcon QFileIconProvider::icon(const QFileInfo &info) const {
    QIcon themed = QAbstractFileIconProvider::icon(info);
    if (!themed.isNull())
        return themed;
    return icon(info.isDir() ? Folder : File);
}
