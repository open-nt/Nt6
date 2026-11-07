// MIT License - see LICENSE
#include <QtCore/qsocketnotifier.h>

#include "dispatcher.h"

QSocketNotifier::QSocketNotifier(qintptr socket, Type type, QObject *parent)
    : QObject(parent), m_fd(socket), m_type(type) {
    m_id = QtPrivate::Dispatcher::instance().addNotifier(this, static_cast<int>(socket), static_cast<int>(type));
}

QSocketNotifier::~QSocketNotifier() { QtPrivate::Dispatcher::instance().removeNotifier(m_id); }

void QSocketNotifier::setEnabled(bool enable) {
    if (m_enabled == enable)
        return;
    m_enabled = enable;
    QtPrivate::Dispatcher::instance().setNotifierEnabled(m_id, enable);
}

bool QSocketNotifier::event(QEvent *e) {
    if (e->type() == QEvent::SockAct) {
        if (m_enabled)
            activated(QSocketDescriptor(static_cast<int>(m_fd)), m_type);
        return true;
    }
    return QObject::event(e);
}
