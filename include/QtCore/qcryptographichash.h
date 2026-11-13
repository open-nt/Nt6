// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qbytearrayview.h>
#include <QtCore/qspan.h>

#include <cstddef>
#include <memory>
#include <utility>

class QIODevice;

class QCryptographicHash {
public:
    enum Algorithm {
        Md4,
        Md5,
        Sha1,
        Sha224,
        Sha256,
        Sha384,
        Sha512,
        Keccak_224 = 7,
        Keccak_256,
        Keccak_384,
        Keccak_512,
        Sha3_224 = 11,
        Sha3_256,
        Sha3_384,
        Sha3_512,
        Blake2b_160 = 15,
        Blake2b_256,
        Blake2b_384,
        Blake2b_512,
        Blake2s_128 = 19,
        Blake2s_160,
        Blake2s_224,
        Blake2s_256,
        NumAlgorithms
    };

    explicit QCryptographicHash(Algorithm method);
    QCryptographicHash(QCryptographicHash &&other) noexcept;
    ~QCryptographicHash();
    QCryptographicHash &operator=(QCryptographicHash &&other) noexcept;

    void reset() noexcept;
    /*!
        Returns the algorithm that the object was constructed with.
    */
    Algorithm algorithm() const noexcept { return m_method; }
    void addData(QByteArrayView data) noexcept;
    /*!
        \overload

        Adds the \a length bytes at \a data to the data being hashed.
    */
    void addData(const char *data, qsizetype length) noexcept { addData(QByteArrayView(data, length)); }
    bool addData(QIODevice *device);
    /*!
        \since 6.8
        Adds \a data to the hash. Nt processes the data immediately, so this is
        the same as addData().
    */
    void addDataWithoutCopy(QByteArrayView data) noexcept { addData(data); }
    /*!
        \since 5.10
        Swaps this hash with \a other.
    */
    void swap(QCryptographicHash &other) noexcept {
        std::swap(m_method, other.m_method);
        d.swap(other.d);
    }
    QByteArray result() const;
    QByteArrayView resultView() const noexcept;

    static QByteArray hash(QByteArrayView data, Algorithm method);
    static QByteArrayView hashInto(QSpan<char> buffer, QByteArrayView data, Algorithm method) noexcept;
    /*!
        \overload
        \since 6.8
        Writes the digest into \a buffer, a span of \c{unsigned char}; the
        result is computed over \a data with \a method.
    */
    static QByteArrayView hashInto(QSpan<unsigned char> buffer, QByteArrayView data, Algorithm method) noexcept {
        return hashInto(QSpan<char>(reinterpret_cast<char *>(buffer.data()), buffer.size()), data, method);
    }
    /*!
        \overload
        \since 6.8
        Writes the digest into \a buffer, a span of \c{std::byte}; the result
        is computed over \a data with \a method.
    */
    static QByteArrayView hashInto(QSpan<std::byte> buffer, QByteArrayView data, Algorithm method) noexcept {
        return hashInto(QSpan<char>(reinterpret_cast<char *>(buffer.data()), buffer.size()), data, method);
    }
    static int hashLength(Algorithm method);
    static bool supportsAlgorithm(Algorithm method);

private:
    struct State;
    Algorithm m_method;
    std::unique_ptr<State> d;
};

class QMessageAuthenticationCode {
public:
    explicit QMessageAuthenticationCode(QCryptographicHash::Algorithm method, QByteArrayView key = {});
    ~QMessageAuthenticationCode();
    /*!
        \since 6.6
        Exchanges this object with \a other.
    */
    void swap(QMessageAuthenticationCode &other) noexcept {
        std::swap(m_method, other.m_method);
        m_outerPad.swap(other.m_outerPad);
        m_innerPad.swap(other.m_innerPad);
        m_inner.swap(other.m_inner);
        m_resultCache.swap(other.m_resultCache);
    }

    void reset();
    void setKey(QByteArrayView key);
    void addData(QByteArrayView data);
    /*!
        \overload

        Adds the \a length bytes at \a data to the message.
    */
    void addData(const char *data, qsizetype length) { addData(QByteArrayView(data, length)); }
    bool addData(QIODevice *device);
    QByteArray result() const;
    QByteArrayView resultView() const;

    static QByteArray hash(QByteArrayView message, QByteArrayView key, QCryptographicHash::Algorithm method);
    static QByteArrayView hashInto(QSpan<char> buffer, QByteArrayView message, QByteArrayView key,
                                   QCryptographicHash::Algorithm method) noexcept;

private:
    QCryptographicHash::Algorithm m_method;
    QByteArray m_outerPad;
    QByteArray m_innerPad;
    QCryptographicHash m_inner;
    mutable QByteArray m_resultCache;
};
