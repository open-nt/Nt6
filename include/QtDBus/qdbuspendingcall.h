// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtDBus/qdbuserror.h>
#include <QtDBus/qdbusmessage.h>

#include <memory>

struct QDBusPendingCallPrivate;

class QDBusPendingCall {
public:
    QDBusPendingCall(const QDBusPendingCall &other);
    QDBusPendingCall(QDBusPendingCall &&other) noexcept;
    QDBusPendingCall &operator=(const QDBusPendingCall &other);
    QDBusPendingCall &operator=(QDBusPendingCall &&other) noexcept;
    ~QDBusPendingCall();
    void swap(QDBusPendingCall &other) noexcept;

    bool isFinished() const;
    void waitForFinished();
    bool isError() const;
    bool isValid() const;
    QDBusError error() const;
    QDBusMessage reply() const;

    static QDBusPendingCall fromError(const QDBusError &error);
    static QDBusPendingCall fromCompletedCall(const QDBusMessage &message);

protected:
    QDBusPendingCall();

private:
    friend class QDBusConnection;
    friend class QDBusConnectionPrivate;
    friend class QDBusPendingReplyBase;
    friend class QDBusPendingCallWatcher;
    explicit QDBusPendingCall(std::shared_ptr<QDBusPendingCallPrivate> d);

    std::shared_ptr<QDBusPendingCallPrivate> d;
};

class QDBusPendingCallWatcher : public QObject, public QDBusPendingCall {
    Q_OBJECT
public:
    explicit QDBusPendingCallWatcher(const QDBusPendingCall &call, QObject *parent = nullptr);
    ~QDBusPendingCallWatcher() override;

    void waitForFinished();

signals:
    /*!
        This signal is emitted when the call has finished, with this object as \a self.
    */
    void finished(QDBusPendingCallWatcher *self = nullptr) { emitSignal(&QDBusPendingCallWatcher::finished, self); }
};
