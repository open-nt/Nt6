// MIT License - see LICENSE
#include "qdbusconnection_p.h"
#include "qdbusmarshaller_p.h"

#include <QtDBus/qdbusmetatype.h>
#include <QtDBus/qdbuspendingcall.h>
#include <QtDBus/qdbuspendingreply.h>
#include <QtDBus/qdbusreply.h>

/*!
    \class QDBusPendingCall
    \brief The QDBusPendingCall class refers to one pending asynchronous call.

    QDBusConnection::asyncCall() and QDBusAbstractInterface::asyncCall() return one. Wait for
    it with waitForFinished(), or watch it with a QDBusPendingCallWatcher. Copies refer to the
    same call.

    \differences
    None.
    \enddifferences

    \sa QDBusPendingReply, QDBusPendingCallWatcher
*/

/*!
    \class QDBusPendingCallWatcher
    \brief The QDBusPendingCallWatcher class provides a convenient way of waiting for
    asynchronous replies.

    The watcher emits finished() in its own thread when the call has finished:

    \code
    auto *watcher = new QDBusPendingCallWatcher(iface.asyncCall("Now"), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [](QDBusPendingCallWatcher *w) {
        QDBusPendingReply<QString> reply = *w;
        if (!reply.isError())
            qDebug() << reply.value();
        w->deleteLater();
    });
    \endcode

    If the call has already finished, finished() is still emitted from the event loop.

    \differences
    None.
    \enddifferences

    \sa QDBusPendingCall, QDBusPendingReply
*/

/*!
    \class QDBusPendingReplyBase
    \internal
*/

/*!
    \class QDBusPendingReply
    \brief The QDBusPendingReply class contains the reply to an asynchronous method call.

    The template arguments are the types of the reply's arguments:

    \code
    QDBusPendingReply<QString, int> reply = iface.asyncCall("Status");
    reply.waitForFinished();
    if (reply.isValid()) {
        QString state = reply.argumentAt<0>();
        int load = reply.argumentAt<1>();
    }
    \endcode

    When the call finishes, the reply's signature is checked against the types; a reply that
    does not match becomes an \c InvalidSignature error.

    \differences
    It is a variadic template, so it accepts any number of types. There is no
    \c QDBusPendingReplyData.
    \enddifferences

    \sa QDBusPendingCall, QDBusReply
*/

/*!
    \class QDBusReply
    \brief The QDBusReply class stores the reply for a method call to a remote object.

    It holds either the first argument of the reply, converted to \c T, or the error:

    \code
    QDBusReply<int> reply = iface.call("Count");
    if (reply.isValid())
        qDebug() << reply.value();
    else
        qWarning() << reply.error().message();
    \endcode

    \c QDBusReply<void> holds only an error. \c QDBusReply<QVariant> takes the value out of the
    D-Bus variant that carries it.

    \differences
    None.
    \enddifferences

    \sa QDBusMessage, QDBusPendingReply
*/

/*!
    Constructs an object that refers to no call. It is finished, with an error.
*/
QDBusPendingCall::QDBusPendingCall() = default;

QDBusPendingCall::QDBusPendingCall(std::shared_ptr<QDBusPendingCallPrivate> p) : d(std::move(p)) {}

/*!
    Constructs a copy of \a other, which refers to the same call.
*/
QDBusPendingCall::QDBusPendingCall(const QDBusPendingCall &other) = default;

/*!
    Constructs an object that takes the place of \a other.
*/
QDBusPendingCall::QDBusPendingCall(QDBusPendingCall &&other) noexcept = default;

/*!
    Makes this object refer to the call of \a other and returns a reference to it.
*/
QDBusPendingCall &QDBusPendingCall::operator=(const QDBusPendingCall &other) = default;

/*!
    Moves \a other into this object and returns a reference to it.
*/
QDBusPendingCall &QDBusPendingCall::operator=(QDBusPendingCall &&other) noexcept = default;

/*!
    Destroys the object. The call goes on.
*/
QDBusPendingCall::~QDBusPendingCall() = default;

/*!
    Swaps this object with \a other.
*/
void QDBusPendingCall::swap(QDBusPendingCall &other) noexcept { d.swap(other.d); }

/*!
    Returns \c true if the reply has arrived, or the call failed.
*/
bool QDBusPendingCall::isFinished() const { return !d || d->isFinished(); }

/*!
    Blocks until the call has finished, without running the event loop.
*/
void QDBusPendingCall::waitForFinished() {
    if (d)
        d->waitForFinished();
}

/*!
    Returns \c true if the call finished with an error.
*/
bool QDBusPendingCall::isError() const {
    return !d || (d->isFinished() && d->reply().type() == QDBusMessage::ErrorMessage);
}

/*!
    Returns \c true if the call finished with a reply.
*/
bool QDBusPendingCall::isValid() const {
    return d && d->isFinished() && d->reply().type() == QDBusMessage::ReplyMessage;
}

/*!
    Returns the error of the call, which is invalid unless it finished with an error.
*/
QDBusError QDBusPendingCall::error() const {
    if (!d)
        return QDBusError(QDBusError::Disconnected, QStringLiteral("Not connected to D-Bus server"));
    return d->isFinished() ? QDBusError(d->reply()) : QDBusError();
}

/*!
    Returns the reply, or an invalid message while the call has not finished.
*/
QDBusMessage QDBusPendingCall::reply() const { return d ? d->reply() : QDBusMessage(); }

/*!
    Returns a call that has finished with \a error.
*/
QDBusPendingCall QDBusPendingCall::fromError(const QDBusError &error) {
    return fromCompletedCall(QDBusMessage::createError(error));
}

/*!
    Returns a call that has finished with \a message, which should be a reply or an error.
*/
QDBusPendingCall QDBusPendingCall::fromCompletedCall(const QDBusMessage &message) {
    auto p = std::make_shared<QDBusPendingCallPrivate>();
    if (message.type() == QDBusMessage::ReplyMessage || message.type() == QDBusMessage::ErrorMessage)
        p->complete(message);
    else
        p->complete(QDBusMessage::createError(QDBusError::InvalidArgs, QStringLiteral("Not a reply")));
    return QDBusPendingCall(p);
}

/*!
    Constructs a watcher for \a call with the given \a parent.
*/
QDBusPendingCallWatcher::QDBusPendingCallWatcher(const QDBusPendingCall &call, QObject *parent)
    : QObject(parent), QDBusPendingCall(call) {
    if (!d)
        d = QDBusPendingCall::fromError(QDBusError(QDBusError::Disconnected, QString())).d;
    const auto guard = QDBusTargetGuard::create(this);
    d->onFinished([guard] {
        guard->post([guard] {
            if (auto *self = dynamic_cast<QDBusPendingCallWatcher *>(guard->target()))
                self->finished(self);
        });
    });
}

/*!
    Destroys the watcher. The call goes on.
*/
QDBusPendingCallWatcher::~QDBusPendingCallWatcher() = default;

/*!
    Blocks until the call has finished. finished() is still emitted from the event loop.
*/
void QDBusPendingCallWatcher::waitForFinished() { d->waitForFinished(); }

void QDBusPendingReplyBase::assign(const QDBusPendingCall &call, const QList<QMetaType> &types) {
    d = call.d;
    if (!d)
        d = QDBusPendingCall::fromError(QDBusError(QDBusError::Disconnected, QString())).d;
    std::lock_guard<std::mutex> lock(d->mutex);
    d->expectedTypes = types;
    d->checkTypes = true;
    if (d->finished)
        d->replyMessage = QDBusPendingCallPrivate::checked(d->replyMessage, types);
}

void QDBusPendingReplyBase::assign(const QDBusMessage &message, const QList<QMetaType> &types) {
    assign(QDBusPendingCall::fromCompletedCall(message), types);
}

QVariant QDBusPendingReplyBase::argumentAt(int index) const {
    if (!d)
        return QVariant();
    const QList<QVariant> args = d->reply().arguments();
    return index >= 0 && index < args.size() ? args.at(index) : QVariant();
}

void qDBusReplyFill(const QDBusMessage &reply, QDBusError &error, QVariant &data) {
    error = QDBusError(reply);
    if (error.isValid())
        return;
    if (reply.type() != QDBusMessage::ReplyMessage) {
        error = QDBusError(QDBusError::NoReply, QStringLiteral("No reply"));
        return;
    }
    const QList<QVariant> args = reply.arguments();
    const char *expected = QDBusMetaType::typeToSignature(data.metaType());
    if (args.isEmpty()) {
        error = QDBusError(QDBusError::InvalidSignature,
                           QStringLiteral("Unexpected reply signature: got \"\", expected \"%1\"")
                               .arg(QString::fromLatin1(expected ? expected : "")));
        return;
    }
    const QVariant &first = args.first();
    if (first.metaType() == data.metaType()) {
        data = first;
        return;
    }
    const QByteArray got = QDBusWriter::signatureOf(first);
    if (expected && got == expected) {
        bool ok = false;
        const QVariant v = qDBusAdaptArgument(first, data.metaType(), &ok);
        if (ok) {
            data = v;
            return;
        }
    }
    error = QDBusError(QDBusError::InvalidSignature,
                       QStringLiteral("Unexpected reply signature: got \"%1\", expected \"%2\"")
                           .arg(QString::fromLatin1(got), QString::fromLatin1(expected ? expected : "")));
}

#include "moc_qdbuspendingcall.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
