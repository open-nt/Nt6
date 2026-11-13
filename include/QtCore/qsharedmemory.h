// MIT License - see LICENSE
#pragma once

#include <QtCore/qnativeipckey.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <memory>

class QSharedMemory : public QObject {
    Q_OBJECT
public:
    enum AccessMode { ReadOnly, ReadWrite };
    enum SharedMemoryError {
        NoError,
        PermissionDenied,
        InvalidSize,
        KeyError,
        AlreadyExists,
        NotFound,
        LockError,
        OutOfResources,
        UnknownError,
    };

    explicit QSharedMemory(QObject *parent = nullptr);
    explicit QSharedMemory(const QString &key, QObject *parent = nullptr);
    explicit QSharedMemory(const QNativeIpcKey &key, QObject *parent = nullptr);
    ~QSharedMemory() override;

    void setKey(const QString &key);
    QString key() const;
    void setNativeKey(const QString &key);
    void setNativeKey(const QNativeIpcKey &key);
    QString nativeKey() const;
    QNativeIpcKey nativeIpcKey() const;
    static QNativeIpcKey platformSafeKey(const QString &key,
                                         QNativeIpcKey::Type type = QNativeIpcKey::DefaultTypeForOs);
    static QNativeIpcKey legacyNativeKey(const QString &key,
                                         QNativeIpcKey::Type type = QNativeIpcKey::legacyDefaultTypeForOs());
    /*!
        \since 6.6
        Returns \c true if keys of the type \a type work on this system; only \c Type::PosixRealtime does in Nt.
    */
    static bool isKeyTypeSupported(QNativeIpcKey::Type type) { return type == QNativeIpcKey::Type::PosixRealtime; }

    bool create(qsizetype size, AccessMode mode = ReadWrite);
    qsizetype size() const;

    bool attach(AccessMode mode = ReadWrite);
    bool isAttached() const;
    bool detach();

    void *data();
    const void *constData() const;
    const void *data() const;

    bool lock();
    bool unlock();

    SharedMemoryError error() const;
    QString errorString() const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
