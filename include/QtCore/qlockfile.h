// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>

#include <chrono>
#include <memory>

class QLockFile {
public:
    /*!
        \enum QLockFile::LockError

        This enum describes the result of the last call to lock() or
        tryLock().

        \value NoError The lock was acquired successfully.
        \value LockFailedError The lock could not be acquired because another process holds it.
        \value PermissionError The lock file could not be created, for lack of permissions in the parent directory.
        \value UnknownError Another error happened, for instance a full partition prevented writing out the lock file, or the directory of the lock file does not exist.
    */
    enum LockError { NoError = 0, LockFailedError = 1, PermissionError = 2, UnknownError = 3 };

    explicit QLockFile(const QString &fileName);
    ~QLockFile();
    QLockFile(const QLockFile &) = delete;
    QLockFile &operator=(const QLockFile &) = delete;

    bool lock();
    bool tryLock(int timeout = 0);
    bool tryLock(std::chrono::milliseconds timeout);
    void unlock();
    void setStaleLockTime(int staleLockTime);
    int staleLockTime() const;
    void setStaleLockTime(std::chrono::milliseconds value);
    std::chrono::milliseconds staleLockTimeAsDuration() const;
    QString fileName() const;
    bool isLocked() const;
    bool getLockInfo(qint64 *pid, QString *hostname, QString *appname) const;
    bool removeStaleLockFile();
    LockError error() const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
