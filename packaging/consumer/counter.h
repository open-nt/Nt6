// MIT License - see LICENSE
#pragma once

#include <QObject>

class Counter : public QObject {
    Q_OBJECT
public:
    explicit Counter(QObject *parent = nullptr) : QObject(parent) {}

    int value() const { return m_value; }
    void setValue(int value) {
        if (value == m_value)
            return;
        m_value = value;
        emit valueChanged(value);
    }

signals:
    // No body: ntmoc generates it.
    void valueChanged(int value);

private:
    int m_value = 0;
};
