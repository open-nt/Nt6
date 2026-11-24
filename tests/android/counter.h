// MIT License - see LICENSE
#pragma once

#include <QObject>

// A class with a signal and a slot, so that the build runs moc on a header of the application.
class Counter : public QObject {
    Q_OBJECT
public:
    explicit Counter(QObject *parent = nullptr) : QObject(parent) {}
    int value() const { return m_value; }

public slots:
    void increment() {
        ++m_value;
        emit changed(m_value);
    }

signals:
    // No body: ntmoc generates it.
    void changed(int value);

private:
    int m_value = 0;
};
