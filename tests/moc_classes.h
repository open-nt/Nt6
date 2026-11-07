#pragma once

#include <QObject>

#include <map>
#include <string>
#include <vector>

namespace outer {
namespace inner {

class Emitter : public QObject {
    Q_OBJECT
public:
    explicit Emitter(QObject *parent = nullptr) : QObject(parent) {}

    void fire() {
        emit noArgs();
        emit overloaded(1);
        emit overloaded(1, 2);
        emit overloaded(2.5);
    }

signals:
    void noArgs();
    void unnamed(int, const std::string &);
    void defaults(int a, QObject *obj = nullptr, int b = 3);
    void templated(const std::map<int, std::string> &m, std::vector<int> v);
    void unsignedArg(unsigned int, unsigned long long count);
    void overloaded(int a);
    void overloaded(int a, int b);
    void overloaded(double d);
    void withBody(int v) { emitSignal(&Emitter::withBody, v); }

public slots:
    void slotNotASignal(int);

private:
    int notSignal(int);
};

} // namespace inner

struct Plain : QObject {
    Q_OBJECT
signals:
    void changed(int v);
public:
    class Nested : public QObject {
        Q_OBJECT
    signals:
        void nestedSignal();
    };
};

} // namespace outer

// Signals in a class without Q_OBJECT are not generated (and a comment mentioning Q_OBJECT
// or `signals: void fake();` must be ignored).
class NoMacro {
public:
    void helper();
};
