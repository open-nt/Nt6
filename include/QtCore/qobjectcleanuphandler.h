// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>

class QObjectCleanupHandler : public QObject {
    Q_OBJECT
public:
    QObjectCleanupHandler();
    ~QObjectCleanupHandler() override;

    QObject *add(QObject *object);
    void remove(QObject *object);
    bool isEmpty() const;
    void clear();

private:
    QList<QObject *> m_objects;
};
