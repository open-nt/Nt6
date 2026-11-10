// MIT License - see LICENSE
#pragma once

// Certificates and keys for the TLS tests, generated with OpenSSL when a test starts. Nothing is
// committed, so no private key is published and no certificate expires in the repository.
//
//   TestPki pki;                                 // a CA and certificates it signed
//   server.setSslConfiguration(pki.serverConfiguration());
//   client.setSslConfiguration(pki.clientConfiguration());
//
// Link the test with OpenSSL::SSL and OpenSSL::Crypto.

#include <QtNetwork/qsslcertificate.h>
#include <QtNetwork/qsslconfiguration.h>
#include <QtNetwork/qsslkey.h>

#include <openssl/pem.h>
#include <openssl/x509v3.h>

#include <memory>

// A certificate with its private key.
struct Identity {
    QSslCertificate certificate;
    QSslKey key;
    QByteArray keyPem;
    EVP_PKEY *pkey = nullptr;
    X509 *x509 = nullptr;
    std::shared_ptr<EVP_PKEY> ownedKey;
    std::shared_ptr<X509> ownedCertificate;
};

// Makes a P-256 key and a certificate for commonName, valid from fromDays to toDays relative to now.
// altNames is an OpenSSL subjectAltName value such as "DNS:localhost,IP:127.0.0.1", or nullptr. The
// certificate is signed by issuer, or self-signed if issuer is nullptr.
inline Identity makeIdentity(const char *commonName, const char *altNames, const Identity *issuer, long fromDays,
                             long toDays, bool ca = false) {
    Identity id;
    EVP_PKEY_CTX *kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    EVP_PKEY_keygen_init(kctx);
    EVP_PKEY_CTX_set_ec_paramgen_curve_nid(kctx, NID_X9_62_prime256v1);
    EVP_PKEY_keygen(kctx, &id.pkey);
    EVP_PKEY_CTX_free(kctx);

    X509 *x = X509_new();
    id.x509 = x;
    X509_set_version(x, 2);
    static long serial = 1;
    ASN1_INTEGER_set(X509_get_serialNumber(x), ++serial);
    X509_gmtime_adj(X509_getm_notBefore(x), fromDays * 86400);
    X509_gmtime_adj(X509_getm_notAfter(x), toDays * 86400);
    X509_set_pubkey(x, id.pkey);
    // The name is made apart and copied in: OpenSSL 4 returns the names of a certificate as const.
    X509_NAME *name = X509_NAME_new();
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, reinterpret_cast<const unsigned char *>("Nt Tests"), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char *>(commonName), -1, -1,
                               0);
    X509_set_subject_name(x, name);
    X509_set_issuer_name(x, issuer ? X509_get_subject_name(issuer->x509) : name);
    X509_NAME_free(name);

    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, issuer ? issuer->x509 : x, x, nullptr, nullptr, 0);
    const auto addExtension = [&](int nid, const char *value) {
        X509_EXTENSION *e = X509V3_EXT_conf_nid(nullptr, &ctx, nid, value);
        X509_add_ext(x, e, -1);
        X509_EXTENSION_free(e);
    };
    addExtension(NID_basic_constraints, ca ? "critical,CA:TRUE" : "CA:FALSE");
    if (altNames)
        addExtension(NID_subject_alt_name, altNames);
    X509_sign(x, issuer ? issuer->pkey : id.pkey, EVP_sha256());

    BIO *c = BIO_new(BIO_s_mem());
    PEM_write_bio_X509(c, x);
    char *data = nullptr;
    long n = BIO_get_mem_data(c, &data);
    id.certificate = QSslCertificate(QByteArray(data, n));
    BIO_free(c);
    BIO *k = BIO_new(BIO_s_mem());
    PEM_write_bio_PrivateKey(k, id.pkey, nullptr, nullptr, 0, nullptr, nullptr);
    n = BIO_get_mem_data(k, &data);
    id.keyPem = QByteArray(data, n);
    id.key = QSslKey(id.keyPem, QSsl::Ec);
    BIO_free(k);
    id.ownedKey.reset(id.pkey, EVP_PKEY_free);
    id.ownedCertificate.reset(id.x509, X509_free);
    return id;
}

// A test CA, a server certificate it signed for localhost and 127.0.0.1, and a self-signed one.
struct TestPki {
    Identity ca = makeIdentity("Nt Test CA", nullptr, nullptr, -1, 3650, true);
    Identity server = makeIdentity("localhost", "DNS:localhost,IP:127.0.0.1", &ca, -1, 365);
    Identity selfSigned = makeIdentity("localhost", "DNS:localhost,IP:127.0.0.1", nullptr, -1, 365);

    // What a server presents: the given identity's certificate and key.
    static QSslConfiguration serverConfiguration(const Identity &id) {
        QSslConfiguration c = QSslConfiguration::defaultConfiguration();
        c.setLocalCertificate(id.certificate);
        c.setPrivateKey(id.key);
        return c;
    }
    QSslConfiguration serverConfiguration() const { return serverConfiguration(server); }

    // What a client trusts: the test CA only.
    QSslConfiguration clientConfiguration() const {
        QSslConfiguration c = QSslConfiguration::defaultConfiguration();
        c.setCaCertificates({ca.certificate});
        return c;
    }
};
