// MIT License - see LICENSE
#include "moc_classes.h"

#include <QCoreApplication>

#include <cstdio>
#include <map>
#include <string>
#include <vector>

static int g_failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

void outer::inner::Emitter::slotNotASignal(int) {}
int outer::inner::Emitter::notSignal(int v) { return v; }

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    using outer::inner::Emitter;

    Emitter e;
    int noArgs = 0, unnamedInt = 0, defaultsA = 0, defaultsB = 0, o1 = 0, o2 = 0, od = 0;
    std::string unnamedStr;
    QObject *obj = &app;

    QObject::connect(&e, &Emitter::noArgs, [&] { ++noArgs; });
    QObject::connect(&e, &Emitter::unnamed, [&](int i, const std::string &s) { unnamedInt = i; unnamedStr = s; });
    QObject::connect(&e, &Emitter::defaults, [&](int a, QObject *o, int b) { defaultsA = a; obj = o; defaultsB = b; });
    QObject::connect(&e, qOverload<int>(&Emitter::overloaded), [&](int a) { o1 = a; });
    QObject::connect(&e, qOverload<int, int>(&Emitter::overloaded), [&](int a, int b) { o2 = a + b; });
    QObject::connect(&e, qOverload<double>(&Emitter::overloaded), [&](double) { ++od; });

    e.fire();
    CHECK(noArgs == 1);
    CHECK(o1 == 1 && o2 == 3 && od == 1);

    emit e.unnamed(7, "seven");
    CHECK(unnamedInt == 7 && unnamedStr == "seven");

    emit e.defaults(5);
    CHECK(defaultsA == 5 && defaultsB == 3 && obj == nullptr);

    int total = 0;
    QObject::connect(&e, &Emitter::templated, [&](const std::map<int, std::string> &m, std::vector<int> v) {
        total = static_cast<int>(m.size() + v.size());
    });
    emit e.templated({{1, "a"}, {2, "b"}}, {1, 2, 3});
    CHECK(total == 5);

    unsigned long long big = 0;
    QObject::connect(&e, &Emitter::unsignedArg, [&](unsigned int, unsigned long long c) { big = c; });
    emit e.unsignedArg(1u, 1ull << 40);
    CHECK(big == (1ull << 40));

    int withBody = 0;
    QObject::connect(&e, &Emitter::withBody, [&](int v) { withBody = v; });
    emit e.withBody(9);
    CHECK(withBody == 9);

    outer::Plain plain;
    int changed = 0;
    QObject::connect(&plain, &outer::Plain::changed, [&](int v) { changed = v; });
    emit plain.changed(4);
    CHECK(changed == 4);

    outer::Plain::Nested nested;
    int nestedHits = 0;
    QObject::connect(&nested, &outer::Plain::Nested::nestedSignal, [&] { ++nestedHits; });
    emit nested.nestedSignal();
    CHECK(nestedHits == 1);

    if (g_failures == 0)
        std::puts("all moc tests passed");
    return g_failures == 0 ? 0 : 1;
}
