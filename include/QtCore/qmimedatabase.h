// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qmimetype.h>
#include <QtCore/qstring.h>

class QFileInfo;
class QIODevice;
class QUrl;

class QMimeDatabase {
public:
    /*!
        \enum QMimeDatabase::MatchMode

        How mimeTypeForFile() finds the type of a file.

        \value MatchDefault Use both the name and the contents of the file. The name decides, unless it fits no type or several; then the contents choose among those.
        \value MatchExtension Use the name of the file only.
        \value MatchContent Use the contents of the file only.
    */
    enum MatchMode { MatchDefault = 0x0, MatchExtension = 0x1, MatchContent = 0x2 };

    QMimeDatabase();
    /*!
        Destroys the database.
    */
    ~QMimeDatabase() = default;
    QMimeDatabase(const QMimeDatabase &) = delete;
    QMimeDatabase &operator=(const QMimeDatabase &) = delete;

    QMimeType mimeTypeForName(const QString &nameOrAlias) const;

    QMimeType mimeTypeForFile(const QString &fileName, MatchMode mode = MatchDefault) const;
    QMimeType mimeTypeForFile(const QFileInfo &fileInfo, MatchMode mode = MatchDefault) const;
    QList<QMimeType> mimeTypesForFileName(const QString &fileName) const;

    QMimeType mimeTypeForData(const QByteArray &data) const;
    QMimeType mimeTypeForData(QIODevice *device) const;

    QMimeType mimeTypeForUrl(const QUrl &url) const;
    QMimeType mimeTypeForFileNameAndData(const QString &fileName, QIODevice *device) const;
    QMimeType mimeTypeForFileNameAndData(const QString &fileName, const QByteArray &data) const;

    QString suffixForFileName(const QString &fileName) const;

    QList<QMimeType> allMimeTypes() const;
};
