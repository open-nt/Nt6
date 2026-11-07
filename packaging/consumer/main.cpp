// MIT License - see LICENSE

#include "counter.h"

#include <QHostAddress>
#include <QString>

#include <iostream>

int main() {
    int seen = 0;
    Counter counter;
    QObject::connect(&counter, &Counter::valueChanged, [&](int value) { seen = value; });
    counter.setValue(42);
    if (seen != 42) {
        std::cerr << "signal was not delivered\n";
        return 1;
    }

    QHostAddress address;
    if (!address.setAddress(QString("192.168.1.10")) || !address.isEqual(QHostAddress("192.168.1.10"))) {
        std::cerr << "QHostAddress failed\n";
        return 1;
    }

    std::cout << "Nt consumer ok\n";
    return 0;
}
