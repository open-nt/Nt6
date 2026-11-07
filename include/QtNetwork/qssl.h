// MIT License - see LICENSE
#pragma once

namespace QSsl {

enum KeyType { PrivateKey, PublicKey };
enum EncodingFormat { Pem, Der };
enum KeyAlgorithm { Opaque, Rsa, Dsa, Ec, Dh };
enum AlternativeNameEntryType { EmailEntry, DnsEntry, IpAddressEntry };
enum SslProtocol {
    TlsV1_2 = 3,
    AnyProtocol = 4,
    SecureProtocols = 5,
    TlsV1_2OrLater = 6,
    TlsV1_3 = 7,
    TlsV1_3OrLater = 8,
    UnknownProtocol = -1
};

} // namespace QSsl
