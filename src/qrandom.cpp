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

QRandomGenerator::QRandomGenerator(const quint32 *seedBuffer, qsizetype len) {
    std::seed_seq seq(seedBuffer, seedBuffer + len);
    m_engine.seed(seq);
}

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

QRandomGenerator *QRandomGenerator::system() {
    static QRandomGenerator generator{SystemTag{}};
    return &generator;
}

QRandomGenerator *QRandomGenerator::global() {
    static QRandomGenerator *generator = [] {
        static QRandomGenerator g = securelySeeded();
        g.m_locked = true;
        return &g;
    }();
    return generator;
}

QRandomGenerator QRandomGenerator::securelySeeded() {
    quint32 seed[8];
    systemBytes(seed, sizeof seed);
    return QRandomGenerator(seed);
}
