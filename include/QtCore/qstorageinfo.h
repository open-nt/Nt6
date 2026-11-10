// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qdir.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

class QStorageInfo {
public:
    QStorageInfo();
    explicit QStorageInfo(const QString &path);
    explicit QStorageInfo(const QDir &dir);

    /*!
        Exchanges this storage with \a other.
    */
    void swap(QStorageInfo &other) noexcept { std::swap(d, other.d); }

    void setPath(const QString &path);

    QString rootPath() const;
    QByteArray device() const;
    QByteArray subvolume() const;
    QByteArray fileSystemType() const;
    QString name() const;
    QString displayName() const;

    qint64 bytesTotal() const;
    qint64 bytesFree() const;
    qint64 bytesAvailable() const;
    int blockSize() const;

    bool isRoot() const;
    bool isReadOnly() const;
    bool isReady() const;
    bool isValid() const;

    void refresh();

    static QList<QStorageInfo> mountedVolumes();
    static QStorageInfo root();

    /*!
        Returns \c true if \a first and \a second are the same mounted volume: they have the same
        root path and the same device.
    */
    friend bool operator==(const QStorageInfo &first, const QStorageInfo &second) {
        return first.d.rootPath == second.d.rootPath && first.d.device == second.d.device;
    }
    /*!
        Returns \c true if \a first and \a second are different volumes.
    */
    friend bool operator!=(const QStorageInfo &first, const QStorageInfo &second) { return !(first == second); }

private:
    struct Data {
        QString rootPath;
        QByteArray device;
        QByteArray subvolume;
        QByteArray fileSystemType;
        QString name;
        qint64 bytesTotal = -1;
        qint64 bytesFree = -1;
        qint64 bytesAvailable = -1;
        int blockSize = -1;
        bool readOnly = false;
        bool ready = false;
        bool valid = false;
    };
    Data d;
};
