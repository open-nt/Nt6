// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtDBus/qdbusmacros.h>

class QDBusAbstractAdaptor : public QObject {
    Q_OBJECT
protected:
    explicit QDBusAbstractAdaptor(QObject *parent);

public:
    ~QDBusAbstractAdaptor() override;

protected:
    void setAutoRelaySignals(bool enable);
    bool autoRelaySignals() const;

private:
    QList<QMetaObject::Connection> m_relays;
    bool m_autoRelay = false;
};
