// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <limits>
#include <random>
#include <type_traits>

class QRandomGenerator {
public:
    /*!
        The type of the values that the generator returns, \c quint32.
    */
    using result_type = quint32;

    /*!
        Constructs a generator seeded with \a seedValue. The same seed always
        produces the same sequence.

        \sa seed(), securelySeeded()
    */
    QRandomGenerator(quint32 seedValue = 1) : m_engine(seedValue) {}
    QRandomGenerator(const quint32 *seedBuffer, qsizetype len);
    /*!
        \overload

        Constructs a generator seeded from all \c N values of \a seedBuffer.
    */
    template <std::size_t N>
    QRandomGenerator(const quint32 (&seedBuffer)[N]) : QRandomGenerator(seedBuffer, static_cast<qsizetype>(N)) {}

    quint32 generate();
    /*!
        Returns a random 64-bit value, built from two calls to generate().

        \sa generate(), bounded()
    */
    quint64 generate64() { const quint64 hi = generate(); return (hi << 32) | generate(); }
    /*!
        Returns a random \c double that is greater than or equal to 0 and less
        than 1, with 53 bits of randomness.

        \sa bounded()
    */
    double generateDouble() {
        const quint64 x = generate64() >> 11;
        return static_cast<double>(x) / static_cast<double>(1ULL << 53);
    }
    /*!
        \overload

        Returns a random \c double that is greater than or equal to 0 and less
        than \a highest.
    */
    double bounded(double highest) { return generateDouble() * highest; }
    /*!
        \overload

        Returns a random \c double that is greater than or equal to \a lowest
        and less than \a highest.
    */
    double bounded(double lowest, double highest) { return lowest + generateDouble() * (highest - lowest); }
    quint32 bounded(quint32 highest);
    /*!
        \overload

        Returns a random value that is greater than or equal to \a lowest and
        less than \a highest.
    */
    quint32 bounded(quint32 lowest, quint32 highest) { return bounded(highest - lowest) + lowest; }
    /*!
        \overload

        Returns a random \c int that is greater than or equal to 0 and less
        than \a highest.
    */
    int bounded(int highest) { return static_cast<int>(bounded(static_cast<quint32>(highest))); }
    /*!
        \overload

        Returns a random \c int that is greater than or equal to \a lowest and
        less than \a highest.
    */
    int bounded(int lowest, int highest) {
        // Unsigned arithmetic: the span or the sum may not fit in an int.
        return static_cast<int>(bounded(static_cast<quint32>(highest) - static_cast<quint32>(lowest)) + static_cast<quint32>(lowest));
    }
    quint64 bounded(quint64 highest);
    /*!
        \overload

        Returns a random \c qint64 that is greater than or equal to 0 and less
        than \a highest.
    */
    qint64 bounded(qint64 highest) { return static_cast<qint64>(bounded(static_cast<quint64>(highest))); }
    /*!
        \overload

        Returns a random \c qint64 that is greater than or equal to \a lowest
        and less than \a highest.
    */
    qint64 bounded(qint64 lowest, qint64 highest) {
        return static_cast<qint64>(bounded(static_cast<quint64>(highest) - static_cast<quint64>(lowest)) + static_cast<quint64>(lowest));
    }
    /*!
        \overload

        Stores a random 32-bit value in each element of the range from
        \a first to \a last.
    */
    template <typename It>
    void generate(It first, It last) {
        for (; first != last; ++first)
            *first = static_cast<typename std::iterator_traits<It>::value_type>(generate());
    }
    /*!
        Fills \a buffer with \a count random 32-bit values.

        \sa generate()
    */
    void fillRange(quint32 *buffer, qsizetype count) { generate(buffer, buffer + count); }
    /*!
        \overload

        Fills \a buffer with \a count random 64-bit values.
    */
    void fillRange(quint64 *buffer, qsizetype count) {
        for (qsizetype i = 0; i < count; ++i)
            buffer[i] = generate64();
    }
    /*!
        \overload

        Fills the array \a buffer with random values of its element type, which
        must be an unsigned integer type of at most 64 bits.
    */
    template <typename UInt, std::size_t N, typename = std::enable_if_t<std::is_unsigned_v<UInt> && (sizeof(UInt) <= 8)>>
    void fillRange(UInt (&buffer)[N]) {
        for (UInt &v : buffer)
            v = static_cast<UInt>(sizeof(UInt) == 8 ? generate64() : generate());
    }
    /*!
        Returns a random 32-bit value, the same as generate(). This lets the
        generator be used with the C++ standard library's distributions.
    */
    result_type operator()() { return generate(); }
    /*!
        Reseeds the generator with \a s, which makes it a deterministic
        generator.

        \sa discard()
    */
    void seed(quint32 s = 1) { m_system = false; m_engine.seed(s); }
    /*!
        Discards the next \a z values of the sequence.
    */
    void discard(unsigned long long z) { for (; z; --z) generate(); }

    /*!
        Returns the smallest value that generate() can return, 0.

        \sa max()
    */
    static constexpr result_type min() { return 0; }
    /*!
        Returns the largest value that generate() can return, 0xffffffff.

        \sa min()
    */
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
