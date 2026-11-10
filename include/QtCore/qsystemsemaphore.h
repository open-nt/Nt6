// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>

#include <memory>

class QSystemSemaphore {
public:
    enum AccessMode { Open, Create };
    enum SystemSemaphoreError {
        NoError,
        PermissionDenied,
        KeyError,
        AlreadyExists,
        NotFound,
        OutOfResources,
        UnknownError,
    };

    explicit QSystemSemaphore(const QString &key, int initialValue = 0, AccessMode mode = Open);
    ~QSystemSemaphore();
    QSystemSemaphore(const QSystemSemaphore &) = delete;
    QSystemSemaphore &operator=(const QSystemSemaphore &) = delete;

    void setKey(const QString &key, int initialValue = 0, AccessMode mode = Open);
    QString key() const;

    bool acquire();
    bool release(int n = 1);

    SystemSemaphoreError error() const;
    QString errorString() const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
