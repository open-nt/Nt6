// MIT License - see LICENSE
#include <QtCore/qcryptographichash.h>
#include <QtCore/qiodevice.h>

#include <cmath>
#include <cstring>

namespace {

constexpr quint64 kSha512K[80] = {
    0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc, 0x3956c25bf348b538,
    0x59f111f1b605d019, 0x923f82a4af194f9b, 0xab1c5ed5da6d8118, 0xd807aa98a3030242, 0x12835b0145706fbe,
    0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2, 0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235,
    0xc19bf174cf692694, 0xe49b69c19ef14ad2, 0xefbe4786384f25e3, 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65,
    0x2de92c6f592b0275, 0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5, 0x983e5152ee66dfab,
    0xa831c66d2db43210, 0xb00327c898fb213f, 0xbf597fc7beef0ee4, 0xc6e00bf33da88fc2, 0xd5a79147930aa725,
    0x06ca6351e003826f, 0x142929670a0e6e70, 0x27b70a8546d22ffc, 0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed,
    0x53380d139d95b3df, 0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6, 0x92722c851482353b,
    0xa2bfe8a14cf10364, 0xa81a664bbc423001, 0xc24b8b70d0f89791, 0xc76c51a30654be30, 0xd192e819d6ef5218,
    0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8, 0x19a4c116b8d2d0c8, 0x1e376c085141ab53,
    0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8, 0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb, 0x5b9cca4f7763e373,
    0x682e6ff3d6b2b8a3, 0x748f82ee5defb2fc, 0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
    0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915, 0xc67178f2e372532b, 0xca273eceea26619c,
    0xd186b8c721c0c207, 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178, 0x06f067aa72176fba, 0x0a637dc5a2c898a6,
    0x113f9804bef90dae, 0x1b710b35131c471b, 0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc,
    0x431d67c49c100d4c, 0x4cc5d4becb3e42b6, 0x597f299cfc657e2a, 0x5fcb6fab3ad6faec, 0x6c44198c4a475817};

constexpr quint64 kSha512Init[8] = {0x6a09e667f3bcc908, 0xbb67ae8584caa73b, 0x3c6ef372fe94f82b, 0xa54ff53a5f1d36f1,
                                    0x510e527fade682d1, 0x9b05688c2b3e6c1f, 0x1f83d9abfb41bd6b, 0x5be0cd19137e2179};
constexpr quint64 kSha384Init[8] = {0xcbbb9d5dc1059ed8, 0x629a292a367cd507, 0x9159015a3070dd17, 0x152fecd8f70e5939,
                                    0x67332667ffc00b31, 0x8eb44a8768581511, 0xdb0c2e0d64f98fa7, 0x47b5481dbefa4fa4};
constexpr quint64 kKeccakRc[24] = {
    0x0000000000000001, 0x0000000000008082, 0x800000000000808a, 0x8000000080008000, 0x000000000000808b,
    0x0000000080000001, 0x8000000080008081, 0x8000000000008009, 0x000000000000008a, 0x0000000000000088,
    0x0000000080008009, 0x000000008000000a, 0x000000008000808b, 0x800000000000008b, 0x8000000000008089,
    0x8000000000008003, 0x8000000000008002, 0x8000000000000080, 0x000000000000800a, 0x800000008000000a,
    0x8000000080008081, 0x8000000000008080, 0x0000000080000001, 0x8000000080008008};

inline quint32 rol32(quint32 x, int n) { return (x << n) | (x >> (32 - n)); }
inline quint32 ror32(quint32 x, int n) { return (x >> n) | (x << (32 - n)); }
inline quint64 ror64(quint64 x, int n) { return (x >> n) | (x << (64 - n)); }
inline quint64 rol64(quint64 x, int n) { return n == 0 ? x : (x << n) | (x >> (64 - n)); }
inline quint32 be32(const uchar *p) { return quint32(p[0]) << 24 | quint32(p[1]) << 16 | quint32(p[2]) << 8 | p[3]; }
inline quint32 le32(const uchar *p) { return quint32(p[3]) << 24 | quint32(p[2]) << 16 | quint32(p[1]) << 8 | p[0]; }
inline quint64 be64(const uchar *p) { return quint64(be32(p)) << 32 | be32(p + 4); }

int digestSize(QCryptographicHash::Algorithm m) {
    using H = QCryptographicHash;
    switch (m) {
    case H::Md4:
    case H::Md5: return 16;
    case H::Sha1: return 20;
    case H::Sha224:
    case H::Sha3_224:
    case H::Keccak_224: return 28;
    case H::Sha256:
    case H::Sha3_256:
    case H::Keccak_256: return 32;
    case H::Sha384:
    case H::Sha3_384:
    case H::Keccak_384: return 48;
    case H::Sha512:
    case H::Sha3_512:
    case H::Keccak_512: return 64;
    }
    return 0;
}

bool isKeccakFamily(QCryptographicHash::Algorithm m) { return m >= QCryptographicHash::Keccak_224; }
bool isSha3(QCryptographicHash::Algorithm m) { return m >= QCryptographicHash::Sha3_224; }

int blockSize(QCryptographicHash::Algorithm m) {
    if (isKeccakFamily(m))
        return 200 - 2 * digestSize(m);
    return m == QCryptographicHash::Sha384 || m == QCryptographicHash::Sha512 ? 128 : 64;
}

} // namespace

struct QCryptographicHash::State {
    Algorithm m;
    int block;
    quint32 h32[8];
    quint64 h64[25]; // SHA-384/512 state, or the Keccak lanes
    uchar buf[200];
    int used;
    quint64 total;

    explicit State(Algorithm method) : m(method), block(blockSize(method)) { init(); }

    void init() {
        used = 0;
        total = 0;
        std::memset(h64, 0, sizeof h64);
        switch (m) {
        case Md4:
        case Md5:
        case Sha1: {
            static const quint32 iv[5] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};
            std::memcpy(h32, iv, sizeof iv);
            break;
        }
        case Sha256:
            for (int i = 0; i < 8; ++i)
                h32[i] = static_cast<quint32>(kSha512Init[i] >> 32);
            break;
        case Sha224:
            for (int i = 0; i < 8; ++i)
                h32[i] = static_cast<quint32>(kSha384Init[i]);
            break;
        case Sha384:
            std::memcpy(h64, kSha384Init, sizeof kSha384Init);
            break;
        case Sha512:
            std::memcpy(h64, kSha512Init, sizeof kSha512Init);
            break;
        default:
            break;
        }
    }

    void md4(const uchar *p) {
        quint32 x[16];
        for (int i = 0; i < 16; ++i)
            x[i] = le32(p + i * 4);
        quint32 a = h32[0], b = h32[1], c = h32[2], d = h32[3];
        for (int i = 0; i < 16; ++i) {
            static const int s[4] = {3, 7, 11, 19};
            const quint32 t = rol32(a + ((b & c) | (~b & d)) + x[i], s[i % 4]);
            a = d; d = c; c = b; b = t;
        }
        for (int i = 0; i < 16; ++i) {
            static const int s[4] = {3, 5, 9, 13};
            const int k = (i % 4) * 4 + i / 4;
            const quint32 t = rol32(a + ((b & c) | (b & d) | (c & d)) + x[k] + 0x5a827999, s[i % 4]);
            a = d; d = c; c = b; b = t;
        }
        static const int order[16] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};
        for (int i = 0; i < 16; ++i) {
            static const int s[4] = {3, 9, 11, 15};
            const quint32 t = rol32(a + (b ^ c ^ d) + x[order[i]] + 0x6ed9eba1, s[i % 4]);
            a = d; d = c; c = b; b = t;
        }
        h32[0] += a; h32[1] += b; h32[2] += c; h32[3] += d;
    }

    void md5(const uchar *p) {
        static const int s[16] = {7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21};
        quint32 x[16];
        for (int i = 0; i < 16; ++i)
            x[i] = le32(p + i * 4);
        quint32 a = h32[0], b = h32[1], c = h32[2], d = h32[3];
        for (int i = 0; i < 64; ++i) {
            quint32 f;
            int g;
            switch (i / 16) {
            case 0: f = (b & c) | (~b & d); g = i; break;
            case 1: f = (d & b) | (~d & c); g = (5 * i + 1) % 16; break;
            case 2: f = b ^ c ^ d; g = (3 * i + 5) % 16; break;
            default: f = c ^ (b | ~d); g = (7 * i) % 16; break;
            }
            const quint32 k = static_cast<quint32>(std::floor(4294967296.0 * std::fabs(std::sin(i + 1.0))));
            const quint32 t = d;
            d = c;
            c = b;
            b += rol32(a + f + k + x[g], s[(i / 16) * 4 + i % 4]);
            a = t;
        }
        h32[0] += a; h32[1] += b; h32[2] += c; h32[3] += d;
    }

    void sha1(const uchar *p) {
        quint32 w[80];
        for (int i = 0; i < 16; ++i)
            w[i] = be32(p + i * 4);
        for (int i = 16; i < 80; ++i)
            w[i] = rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        quint32 a = h32[0], b = h32[1], c = h32[2], d = h32[3], e = h32[4];
        for (int i = 0; i < 80; ++i) {
            quint32 f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5a827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ed9eba1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdc; }
            else { f = b ^ c ^ d; k = 0xca62c1d6; }
            const quint32 t = rol32(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rol32(b, 30); b = a; a = t;
        }
        h32[0] += a; h32[1] += b; h32[2] += c; h32[3] += d; h32[4] += e;
    }

    void sha256(const uchar *p) {
        quint32 w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = be32(p + i * 4);
        for (int i = 16; i < 64; ++i) {
            const quint32 s0 = ror32(w[i - 15], 7) ^ ror32(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const quint32 s1 = ror32(w[i - 2], 17) ^ ror32(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        quint32 v[8];
        std::memcpy(v, h32, sizeof v);
        for (int i = 0; i < 64; ++i) {
            const quint32 k = static_cast<quint32>(kSha512K[i] >> 32);
            const quint32 s1 = ror32(v[4], 6) ^ ror32(v[4], 11) ^ ror32(v[4], 25);
            const quint32 ch = (v[4] & v[5]) ^ (~v[4] & v[6]);
            const quint32 t1 = v[7] + s1 + ch + k + w[i];
            const quint32 s0 = ror32(v[0], 2) ^ ror32(v[0], 13) ^ ror32(v[0], 22);
            const quint32 maj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
            for (int j = 7; j > 0; --j)
                v[j] = v[j - 1];
            v[4] += t1;
            v[0] = t1 + s0 + maj;
        }
        for (int i = 0; i < 8; ++i)
            h32[i] += v[i];
    }

    void sha512(const uchar *p) {
        quint64 w[80];
        for (int i = 0; i < 16; ++i)
            w[i] = be64(p + i * 8);
        for (int i = 16; i < 80; ++i) {
            const quint64 s0 = ror64(w[i - 15], 1) ^ ror64(w[i - 15], 8) ^ (w[i - 15] >> 7);
            const quint64 s1 = ror64(w[i - 2], 19) ^ ror64(w[i - 2], 61) ^ (w[i - 2] >> 6);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        quint64 v[8];
        std::memcpy(v, h64, sizeof v);
        for (int i = 0; i < 80; ++i) {
            const quint64 s1 = ror64(v[4], 14) ^ ror64(v[4], 18) ^ ror64(v[4], 41);
            const quint64 ch = (v[4] & v[5]) ^ (~v[4] & v[6]);
            const quint64 t1 = v[7] + s1 + ch + kSha512K[i] + w[i];
            const quint64 s0 = ror64(v[0], 28) ^ ror64(v[0], 34) ^ ror64(v[0], 39);
            const quint64 maj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
            for (int j = 7; j > 0; --j)
                v[j] = v[j - 1];
            v[4] += t1;
            v[0] = t1 + s0 + maj;
        }
        for (int i = 0; i < 8; ++i)
            h64[i] += v[i];
    }

    void keccakF() {
        quint64 *a = h64;
        for (quint64 rc : kKeccakRc) {
            quint64 c[5], d;
            for (int x = 0; x < 5; ++x)
                c[x] = a[x] ^ a[x + 5] ^ a[x + 10] ^ a[x + 15] ^ a[x + 20];
            for (int x = 0; x < 5; ++x) {
                d = c[(x + 4) % 5] ^ rol64(c[(x + 1) % 5], 1);
                for (int y = 0; y < 25; y += 5)
                    a[y + x] ^= d;
            }
            // Rho and pi: walk the 24 lanes, with triangular rotation amounts.
            quint64 last = a[1];
            int x = 1, y = 0;
            for (int t = 0; t < 24; ++t) {
                const int nx = y, ny = (2 * x + 3 * y) % 5;
                const int rot = ((t + 1) * (t + 2) / 2) % 64;
                const quint64 tmp = a[nx + 5 * ny];
                a[nx + 5 * ny] = rol64(last, rot);
                last = tmp;
                x = nx;
                y = ny;
            }
            for (int r = 0; r < 25; r += 5) {
                quint64 row[5];
                std::memcpy(row, a + r, sizeof row);
                for (int i = 0; i < 5; ++i)
                    a[r + i] = row[i] ^ (~row[(i + 1) % 5] & row[(i + 2) % 5]);
            }
            a[0] ^= rc;
        }
    }

    void absorb(const uchar *p) {
        for (int i = 0; i < block / 8; ++i) {
            quint64 lane = 0;
            for (int b = 7; b >= 0; --b)
                lane = (lane << 8) | p[i * 8 + b];
            h64[i] ^= lane;
        }
        keccakF();
    }

    void compress(const uchar *p) {
        switch (m) {
        case Md4: md4(p); break;
        case Md5: md5(p); break;
        case Sha1: sha1(p); break;
        case Sha224:
        case Sha256: sha256(p); break;
        case Sha384:
        case Sha512: sha512(p); break;
        default: absorb(p); break;
        }
    }

    void add(const uchar *p, std::size_t n) {
        total += n;
        while (n > 0) {
            if (used == 0 && n >= static_cast<std::size_t>(block)) {
                compress(p);
                p += block;
                n -= static_cast<std::size_t>(block);
                continue;
            }
            const std::size_t take = std::min(n, static_cast<std::size_t>(block - used));
            std::memcpy(buf + used, p, take);
            used += static_cast<int>(take);
            p += take;
            n -= take;
            if (used == block) {
                compress(buf);
                used = 0;
            }
        }
    }

    QByteArray finish() {
        if (isKeccakFamily(m)) {
            std::memset(buf + used, 0, static_cast<std::size_t>(block - used));
            buf[used] ^= isSha3(m) ? 0x06 : 0x01;
            buf[block - 1] ^= 0x80;
            absorb(buf);
            QByteArray out(digestSize(m), '\0');
            for (int i = 0; i < out.size(); ++i)
                out[i] = static_cast<char>(h64[i / 8] >> (8 * (i % 8)));
            return out;
        }

        const quint64 bits = total * 8;
        const int lengthBytes = block == 128 ? 16 : 8;
        buf[used++] = 0x80;
        if (used > block - lengthBytes) {
            std::memset(buf + used, 0, static_cast<std::size_t>(block - used));
            compress(buf);
            used = 0;
        }
        std::memset(buf + used, 0, static_cast<std::size_t>(block - used));
        for (int i = 0; i < 8; ++i) {
            const uchar byte = static_cast<uchar>(bits >> (8 * i));
            if (m == Md4 || m == Md5)
                buf[block - 8 + i] = byte;
            else
                buf[block - 1 - i] = byte;
        }
        compress(buf);

        QByteArray out(digestSize(m), '\0');
        for (int i = 0; i < out.size(); ++i) {
            if (m == Md4 || m == Md5)
                out[i] = static_cast<char>(h32[i / 4] >> (8 * (i % 4)));
            else if (block == 128)
                out[i] = static_cast<char>(h64[i / 8] >> (56 - 8 * (i % 8)));
            else
                out[i] = static_cast<char>(h32[i / 4] >> (24 - 8 * (i % 4)));
        }
        return out;
    }
};

QCryptographicHash::QCryptographicHash(Algorithm method) : m_method(method), d(std::make_unique<State>(method)) {}
QCryptographicHash::QCryptographicHash(QCryptographicHash &&other) noexcept = default;
QCryptographicHash::~QCryptographicHash() = default;
QCryptographicHash &QCryptographicHash::operator=(QCryptographicHash &&other) noexcept = default;

void QCryptographicHash::reset() noexcept { d->init(); }

void QCryptographicHash::addData(QByteArrayView data) noexcept {
    d->add(reinterpret_cast<const uchar *>(data.data()), static_cast<std::size_t>(data.size()));
}

bool QCryptographicHash::addData(QIODevice *device) {
    if (!device || !device->isReadable() || !device->isOpen())
        return false;
    char buffer[4096];
    qint64 n;
    while ((n = device->read(buffer, sizeof buffer)) > 0)
        addData(QByteArrayView(buffer, n));
    return device->atEnd();
}

QByteArray QCryptographicHash::result() const {
    State copy = *d; // more data may follow
    return copy.finish();
}

QByteArray QCryptographicHash::hash(QByteArrayView data, Algorithm method) {
    QCryptographicHash h(method);
    h.addData(data);
    return h.result();
}

int QCryptographicHash::hashLength(Algorithm method) { return digestSize(method); }

bool QCryptographicHash::supportsAlgorithm(Algorithm method) { return digestSize(method) != 0; }

QMessageAuthenticationCode::QMessageAuthenticationCode(QCryptographicHash::Algorithm method, QByteArrayView key)
    : m_method(method), m_inner(method) {
    setKey(key);
}

QMessageAuthenticationCode::~QMessageAuthenticationCode() = default;

void QMessageAuthenticationCode::setKey(QByteArrayView key) {
    const int block = blockSize(m_method);
    QByteArray k = key.size() > block ? QCryptographicHash::hash(key, m_method) : key.toByteArray();
    k.resize(block);
    m_innerPad = k;
    m_outerPad = k;
    for (int i = 0; i < block; ++i) {
        m_innerPad[i] = static_cast<char>(m_innerPad[i] ^ 0x36);
        m_outerPad[i] = static_cast<char>(m_outerPad[i] ^ 0x5c);
    }
    reset();
}

void QMessageAuthenticationCode::reset() {
    m_inner.reset();
    m_inner.addData(m_innerPad);
}

void QMessageAuthenticationCode::addData(QByteArrayView data) { m_inner.addData(data); }

bool QMessageAuthenticationCode::addData(QIODevice *device) { return m_inner.addData(device); }

QByteArray QMessageAuthenticationCode::result() const {
    QCryptographicHash outer(m_method);
    outer.addData(m_outerPad);
    outer.addData(m_inner.result());
    return outer.result();
}

QByteArray QMessageAuthenticationCode::hash(QByteArrayView message, QByteArrayView key, QCryptographicHash::Algorithm method) {
    QMessageAuthenticationCode mac(method, key);
    mac.addData(message);
    return mac.result();
}
