// MIT License - see LICENSE
#include <QtCore/qsocketnotifier.h>

#include "dispatcher.h"

/*!
    \class QSocketNotifier
    \brief The QSocketNotifier class provides a way to watch activity on a file descriptor.

    A QSocketNotifier emits activated() from the event loop when a file
    descriptor becomes readable, writable or has an exceptional condition.
    The notifier does not own the descriptor and does not read or write it;
    the slot must do so, otherwise activated() fires again on the next pass.

    \code
    QSocketNotifier notifier(fd, QSocketNotifier::Read);
    QObject::connect(&notifier, &QSocketNotifier::activated, [](QSocketDescriptor fd) {
        drain(fd);
    });
    \endcode

    \differences
    Descriptors are watched with \c poll(), so only POSIX descriptors are
    supported, and the \c Exception type watches for \c POLLPRI. There is
    no constructor taking only a type, no \c setSocket(), and the type of
    the \c activated() signal's first argument is always QSocketDescriptor.
    \enddifferences

    \sa QSocketDescriptor, QEventLoop
*/

/*!
    Constructs a notifier that watches \a socket for the condition \a type.
    The notifier is enabled and has the given \a parent.
*/
QSocketNotifier::QSocketNotifier(qintptr socket, Type type, QObject *parent)
    : QObject(parent), m_fd(socket), m_type(type) {
    m_id = QtPrivate::Dispatcher::instance().addNotifier(this, static_cast<int>(socket), static_cast<int>(type));
}

/*!
    Destroys the notifier and stops watching the descriptor. The descriptor
    is not closed.
*/
QSocketNotifier::~QSocketNotifier() { QtPrivate::Dispatcher::instance().removeNotifier(m_id); }

/*!
    Enables the notifier if \a enable is \c true; otherwise disables it. A
    disabled notifier does not emit activated().

    \sa isEnabled()
*/
void QSocketNotifier::setEnabled(bool enable) {
    if (m_enabled == enable)
        return;
    m_enabled = enable;
    QtPrivate::Dispatcher::instance().setNotifierEnabled(m_id, enable);
}

/*!
    \reimp
*/
bool QSocketNotifier::event(QEvent *e) {
    if (e->type() == QEvent::SockAct) {
        if (m_enabled)
            activated(QSocketDescriptor(static_cast<int>(m_fd)), m_type);
        return true;
    }
    return QObject::event(e);
}
