// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qbytearrayview.h>

#include <memory>

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
    Algorithm algorithm() const noexcept { return m_method; }
    void addData(QByteArrayView data) noexcept;
    bool addData(QIODevice *device);
    QByteArray result() const;

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
    bool addData(QIODevice *device);
    QByteArray result() const;

    static QByteArray hash(QByteArrayView message, QByteArrayView key, QCryptographicHash::Algorithm method);

private:
    QCryptographicHash::Algorithm m_method;
    QByteArray m_outerPad;
    QByteArray m_innerPad;
    QCryptographicHash m_inner;
};
