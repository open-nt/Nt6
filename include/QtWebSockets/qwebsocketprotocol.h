// MIT License - see LICENSE
#pragma once

namespace QWebSocketProtocol {

enum Version {
    VersionUnknown = -1,
    Version0 = 0,
    Version4 = 4,
    Version5 = 5,
    Version6 = 6,
    Version7 = 7,
    Version8 = 8,
    Version13 = 13,
    VersionLatest = Version13
};

// A fixed underlying type, so that application codes (3000-4999) are valid values too.
enum CloseCode : int {
    CloseCodeNormal = 1000,
    CloseCodeGoingAway = 1001,
    CloseCodeProtocolError = 1002,
    CloseCodeDatatypeNotSupported = 1003,
    CloseCodeReserved1004 = 1004,
    CloseCodeMissingStatusCode = 1005,
    CloseCodeAbnormalDisconnection = 1006,
    CloseCodeWrongDatatype = 1007,
    CloseCodePolicyViolated = 1008,
    CloseCodeTooMuchData = 1009,
    CloseCodeMissingExtension = 1010,
    CloseCodeBadOperation = 1011,
    CloseCodeTlsHandshakeFailed = 1015
};

} // namespace QWebSocketProtocol
