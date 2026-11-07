// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <limits>
#include <random>
#include <type_traits>

class QRandomGenerator {
public:
    using result_type = quint32;

    QRandomGenerator(quint32 seedValue = 1) : m_engine(seedValue) {}
    QRandomGenerator(const quint32 *seedBuffer, qsizetype len);
    template <qsizetype N>
    QRandomGenerator(const quint32 (&seedBuffer)[N]) : QRandomGenerator(seedBuffer, N) {}

    quint32 generate();
    quint64 generate64() { const quint64 hi = generate(); return (hi << 32) | generate(); }
    double generateDouble() {
        const quint64 x = generate64() >> 11;
        return static_cast<double>(x) / static_cast<double>(1ULL << 53);
    }
    double bounded(double highest) { return generateDouble() * highest; }
    quint32 bounded(quint32 highest);
    quint32 bounded(quint32 lowest, quint32 highest) { return bounded(highest - lowest) + lowest; }
    int bounded(int highest) { return static_cast<int>(bounded(static_cast<quint32>(highest))); }
    int bounded(int lowest, int highest) {
        return static_cast<int>(bounded(static_cast<quint32>(highest) - static_cast<quint32>(lowest))) + lowest;
    }
    quint64 bounded(quint64 highest);
    qint64 bounded(qint64 highest) { return static_cast<qint64>(bounded(static_cast<quint64>(highest))); }
    qint64 bounded(qint64 lowest, qint64 highest) {
        return static_cast<qint64>(bounded(static_cast<quint64>(highest) - static_cast<quint64>(lowest))) + lowest;
    }
    template <typename It>
    void generate(It first, It last) {
        for (; first != last; ++first)
            *first = static_cast<typename std::iterator_traits<It>::value_type>(generate());
    }
    void fillRange(quint32 *buffer, qsizetype count) { generate(buffer, buffer + count); }
    void fillRange(quint64 *buffer, qsizetype count) {
        for (qsizetype i = 0; i < count; ++i)
            buffer[i] = generate64();
    }
    result_type operator()() { return generate(); }
    void seed(quint32 s = 1) { m_system = false; m_engine.seed(s); }
    void discard(unsigned long long z) { for (; z; --z) generate(); }

    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }

    // Cryptographically secure and thread-safe; backed by the operating system.
    static QRandomGenerator *system();
    // Seeded once from system(); thread-safe.
    static QRandomGenerator *global();
    static QRandomGenerator securelySeeded();

private:
    struct SystemTag {};
    explicit QRandomGenerator(SystemTag) : m_system(true) {}

    bool m_system = false;
    bool m_locked = false;
    std::mt19937 m_engine;
};
