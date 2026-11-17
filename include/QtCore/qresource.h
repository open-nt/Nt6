// MIT License - see LICENSE
#pragma once

#include <QtCore/qtresource.h>

#include <QtCore/qbytearray.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qglobal.h>
#include <QtCore/qlocale.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

#include <memory>

class QResource {
public:
    /*!
        \enum QResource::Compression
        \since 5.13

        The compression algorithm of a resource's data.

        \value NoCompression The data is stored as is.
        \value ZlibCompression The data is compressed with zlib. Nt never
               produces it.
        \value ZstdCompression The data is compressed with Zstandard. Nt never
               produces it.

        \sa compressionAlgorithm()
    */
    enum Compression { NoCompression, ZlibCompression, ZstdCompression };

    explicit QResource(const QString &file = QString(), const QLocale &locale = QLocale());
    ~QResource();
    QResource(const QResource &) = delete;
    QResource &operator=(const QResource &) = delete;

    void setFileName(const QString &file);
    QString fileName() const;
    QString absoluteFilePath() const;

    void setLocale(const QLocale &locale);
    QLocale locale() const;

    bool isValid() const;

    Compression compressionAlgorithm() const;
    qint64 size() const;
    const uchar *data() const;
    qint64 uncompressedSize() const;
    QByteArray uncompressedData() const;
    QDateTime lastModified() const;

    static bool registerResource(const QString &rccFileName, const QString &resourceRoot = QString());
    static bool unregisterResource(const QString &rccFileName, const QString &resourceRoot = QString());
    static bool registerResource(const uchar *rccData, const QString &resourceRoot = QString());
    static bool unregisterResource(const uchar *rccData, const QString &resourceRoot = QString());

protected:
    bool isDir() const;
    bool isFile() const;
    QStringList children() const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

// Q_INIT_RESOURCE() and Q_CLEANUP_RESOURCE() are in qtresource.h, which qglobal.h includes.
