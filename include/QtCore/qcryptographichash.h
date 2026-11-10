// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qbytearrayview.h>

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
        Sha3_512
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

private:
    QCryptographicHash::Algorithm m_method;
    QByteArray m_outerPad;
    QByteArray m_innerPad;
    QCryptographicHash m_inner;
    mutable QByteArray m_resultCache;
};
