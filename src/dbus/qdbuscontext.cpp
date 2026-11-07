// MIT License - see LICENSE
#include "qdbuscontext_p.h"

#include <QtCore/qobject.h>
#include <QtDBus/qdbuscontext.h>

#include <algorithm>
#include <mutex>
#include <vector>

/*!
    \class QDBusContext
    \brief The QDBusContext class allows slots to determine the D-Bus context of the calls.

    Inherit QDBusContext in an exported object or adaptor, next to QObject. While a slot runs
    because of a D-Bus call, message() is the call and connection() the connection it came on.
    A slot can then reply later, or with an error:

    \code
    class Store : public QObject, protected QDBusContext {
        Q_OBJECT
    public slots:
        QString Get(const QString &key) {
            if (!m_values.contains(key))
                sendErrorReply(QDBusError::InvalidArgs, "no such key");
            return m_values.value(key);
        }
    };
    \endcode

    \differences
    The destructor is virtual, and QDBusContext cannot be copied. The context is also set on the
    object that owns an adaptor while the adaptor's slot runs.
    \enddifferences

    \sa QDBusConnection::registerObject()
*/

namespace {

// Every QDBusContext, so that the one of an object can be found even if it is a protected base,
// which dynamic_cast cannot reach: both cast to the address of the complete object.
std::mutex &registryMutex() {
    static std::mutex m;
    return m;
}

std::vector<QDBusContext *> &registry() {
    static std::vector<QDBusContext *> r;
    return r;
}

} // namespace

QDBusContextPrivate *QDBusContextPrivate::set(QObject *object, QDBusContextPrivate *context) {
    if (!object)
        return nullptr;
    const void *complete = dynamic_cast<const void *>(object);
    QDBusContext *c = nullptr;
    {
        std::lock_guard<std::mutex> lock(registryMutex());
        for (QDBusContext *candidate : registry())
            if (dynamic_cast<const void *>(candidate) == complete) {
                c = candidate;
                break;
            }
    }
    if (!c)
        return nullptr;
    QDBusContextPrivate *old = c->d_ptr;
    c->d_ptr = context;
    return old;
}

/*!
    Constructs a context, which is empty until a D-Bus call arrives.
*/
QDBusContext::QDBusContext() {
    std::lock_guard<std::mutex> lock(registryMutex());
    registry().push_back(this);
}

/*!
    Destroys the context.
*/
QDBusContext::~QDBusContext() {
    std::lock_guard<std::mutex> lock(registryMutex());
    auto &r = registry();
    r.erase(std::remove(r.begin(), r.end(), this), r.end());
}

/*!
    Returns \c true if the slot that is running was called over D-Bus.
*/
bool QDBusContext::calledFromDBus() const { return d_ptr != nullptr; }

/*!
    Returns the connection on which the current call arrived. Valid only while
    calledFromDBus() is \c true.
*/
QDBusConnection QDBusContext::connection() const { return d_ptr ? d_ptr->connection : QDBusConnection(QString()); }

/*!
    Returns the message of the current call. Valid only while calledFromDBus() is \c true.
*/
const QDBusMessage &QDBusContext::message() const {
    static const QDBusMessage none;
    return d_ptr ? d_ptr->message : none;
}

/*!
    Returns \c true if the reply to the current call will be sent later.
*/
bool QDBusContext::isDelayedReply() const { return d_ptr && d_ptr->message.isDelayedReply(); }

/*!
    Sets whether the reply to the current call is sent later to \a enable. The slot's return
    value is then ignored; keep message() and send the reply with
    \c{connection().send(message().createReply(...))}.
*/
void QDBusContext::setDelayedReply(bool enable) const {
    if (d_ptr)
        d_ptr->message.setDelayedReply(enable);
}

/*!
    Sends the error \a name with the text \a message as the reply to the current call. The
    slot's return value is ignored.
*/
void QDBusContext::sendErrorReply(const QString &name, const QString &message) const {
    if (!d_ptr)
        return;
    d_ptr->message.setDelayedReply(true);
    if (d_ptr->message.isReplyRequired())
        d_ptr->connection.send(d_ptr->message.createErrorReply(name, message));
}

/*!
    \overload
    Sends an error of the type \a type with the text \a message as the reply.
*/
void QDBusContext::sendErrorReply(QDBusError::ErrorType type, const QString &message) const {
    sendErrorReply(QDBusError::errorString(type), message);
}
