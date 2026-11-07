// MIT License - see LICENSE
#pragma once

#include <QtCore/qmetatype.h>

#include <memory>

class QDBusUnixFileDescriptor {
public:
    QDBusUnixFileDescriptor();
    explicit QDBusUnixFileDescriptor(int fileDescriptor);
    QDBusUnixFileDescriptor(const QDBusUnixFileDescriptor &other);
    QDBusUnixFileDescriptor(QDBusUnixFileDescriptor &&other) noexcept;
    QDBusUnixFileDescriptor &operator=(const QDBusUnixFileDescriptor &other);
    QDBusUnixFileDescriptor &operator=(QDBusUnixFileDescriptor &&other) noexcept;
    ~QDBusUnixFileDescriptor();

    bool isValid() const;
    int fileDescriptor() const;
    void setFileDescriptor(int fileDescriptor);
    void giveFileDescriptor(int fileDescriptor);
    int takeFileDescriptor();
    void swap(QDBusUnixFileDescriptor &other) noexcept;
    static bool isSupported();

private:
    struct Handle;
    std::shared_ptr<Handle> d;
};

Q_DECLARE_METATYPE(QDBusUnixFileDescriptor)
