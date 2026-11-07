// MIT License - see LICENSE
#include <QtCore/qrandom.h>

#include <sys/random.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace {

void systemBytes(void *out, std::size_t size) {
    auto *p = static_cast<unsigned char *>(out);
    while (size > 0) {
        const ssize_t n = getrandom(p, size, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            std::perror("QRandomGenerator: getrandom");
            std::abort(); // never fall back to predictable output
        }
        p += n;
        size -= static_cast<std::size_t>(n);
    }
}

std::mutex g_globalLock;

} // namespace

/*!
    \class QRandomGenerator
    \brief The QRandomGenerator class allows one to obtain random values from a high-quality Random Number Generator.

    A QRandomGenerator produces 32-bit and 64-bit random numbers. Generators
    that are constructed or seeded explicitly are deterministic: the same seed
    always gives the same sequence, which is useful for tests. The generators
    returned by system() and global() are not.

    \code
    int roll = QRandomGenerator::global()->bounded(1, 7);
    \endcode

    The class meets the requirements of a C++ \c UniformRandomBitGenerator, so
    it can be passed to the standard \c <random> distributions and to
    \c std::shuffle.

    A seeded QRandomGenerator is not thread-safe. The objects returned by
    system() and global() are.

    \differences
    There is no \c QRandomGenerator64, no \c operator==, \c operator<< or
    \c operator>> on the generator, no constructor taking a
    \c std::seed_seq, and no \c bounded() overload with two \c quint64 limits.
    A seeded generator uses \c std::mt19937. system() reads from
    \c getrandom(2), and global() is guarded by a mutex rather than being
    thread-local.
    \enddifferences

    \sa QUuid
*/

/*!
    Constructs a generator seeded from the \a len values at \a seedBuffer. The
    seed values are mixed with \c std::seed_seq, so the sequence is fully
    determined by them.

    \sa seed(), securelySeeded()
*/
QRandomGenerator::QRandomGenerator(const quint32 *seedBuffer, qsizetype len) {
    std::seed_seq seq(seedBuffer, seedBuffer + len);
    m_engine.seed(seq);
}

/*!
    Returns a random 32-bit value from this generator.

    \sa generate64(), bounded()
*/
quint32 QRandomGenerator::generate() {
    if (m_system) {
        quint32 v;
        systemBytes(&v, sizeof v);
        return v;
    }
    if (m_locked) {
        std::lock_guard<std::mutex> lock(g_globalLock);
        return m_engine();
    }
    return m_engine();
}

// Rejection sampling, so every value in the range is equally likely.
/*!
    Returns a random value that is greater than or equal to 0 and less than
    \a highest. Returns 0 if \a highest is 0 or 1. The values are uniformly
    distributed.

    \sa generate(), generateDouble()
*/
quint32 QRandomGenerator::bounded(quint32 highest) {
    if (highest <= 1)
        return 0;
    const quint32 limit = -highest % highest;
    quint32 v;
    do {
        v = generate();
    } while (v < limit);
    return v % highest;
}

/*!
    \overload

    Returns a 64-bit random value that is greater than or equal to 0 and less
    than \a highest.
*/
quint64 QRandomGenerator::bounded(quint64 highest) {
    if (highest <= 1)
        return 0;
    const quint64 limit = -highest % highest;
    quint64 v;
    do {
        v = generate64();
    } while (v < limit);
    return v % highest;
}

/*!
    Returns the process-wide generator that reads from the operating system's
    random number source. It is cryptographically secure and thread-safe, but
    slower than global().

    \sa global(), securelySeeded()
*/
QRandomGenerator *QRandomGenerator::system() {
    static QRandomGenerator generator{SystemTag{}};
    return &generator;
}

/*!
    Returns the process-wide pseudo-random generator, which is seeded once
    from system(). It is thread-safe but not suitable for cryptographic use.

    \sa system(), securelySeeded()
*/
QRandomGenerator *QRandomGenerator::global() {
    static QRandomGenerator *generator = [] {
        static QRandomGenerator g = securelySeeded();
        g.m_locked = true;
        return &g;
    }();
    return generator;
}

/*!
    Returns a new generator that is seeded with random data from system().

    \sa system(), global()
*/
QRandomGenerator QRandomGenerator::securelySeeded() {
    quint32 seed[8];
    systemBytes(seed, sizeof seed);
    return QRandomGenerator(seed);
}
