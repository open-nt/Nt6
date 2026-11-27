// MIT License - see LICENSE
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

// Macros of the program's own that declare parts of the meta-object, as moc expands them.
#define TEST_PROPERTY(type, name)                                                                                      \
    Q_PROPERTY(type name READ name WRITE set_##name NOTIFY name##Changed)                                              \
public:                                                                                                                \
    type name() const { return m_##name; }                                                                             \
    void set_##name(type value) {                                                                                      \
        m_##name = value;                                                                                              \
        emit name##Changed();                                                                                          \
    }                                                                                                                  \
    Q_SIGNAL void name##Changed();                                                                                     \
                                                                                                                       \
private:                                                                                                               \
    type m_##name{};
#define TEST_SIGNAL(name) void name(int value);
#define TEST_SECTION signals
#define TEST_OUTER(name) TEST_SIGNAL(name)
#define TEST_VALUE(x) ((x) + 1)

class MacroEmitter : public QObject {
    Q_OBJECT
    TEST_PROPERTY(int, width)
    TEST_PROPERTY(std::string, label)
public:
    int next() const { return TEST_VALUE(m_width); }
    TEST_SECTION : TEST_SIGNAL(moved) TEST_OUTER(resized) void after();
};

// Signals in a class without Q_OBJECT are not generated (and a comment mentioning Q_OBJECT
// or `signals: void fake();` must be ignored).
class NoMacro {
public:
    void helper();
};
