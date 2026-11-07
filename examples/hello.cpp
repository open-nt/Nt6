#include <QCoreApplication>
#include <QObject>
#include <QTimer>

#include <iostream>

class Counter : public QObject {
    Q_OBJECT
public:
    explicit Counter(QObject *parent = nullptr) : QObject(parent) {}

    void setValue(int v) {
        if (v == m_value)
            return;
        m_value = v;
        emit valueChanged(v);
    }

signals:
    void valueChanged(int value) { emitSignal(&Counter::valueChanged, value); }

private:
    int m_value = 0;
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    Counter counter;
    QObject::connect(&counter, &Counter::valueChanged,
                     [](int v) { std::cout << "value = " << v << "\n"; });

    int ticks = 0;
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&] {
        counter.setValue(++ticks);
        if (ticks == 3)
            QCoreApplication::quit();
    });
    timer.start(100);

    return app.exec();
}
