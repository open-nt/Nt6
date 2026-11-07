// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qflags.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

class QUrl {
public:
    enum ParsingMode { TolerantMode, StrictMode, DecodedMode };
    enum UrlFormattingOption {
        None = 0x0,
        RemoveScheme = 0x1,
        RemovePassword = 0x2,
        RemoveUserInfo = RemovePassword | 0x4,
        RemovePort = 0x8,
        RemoveAuthority = RemoveUserInfo | RemovePort | 0x10,
        RemovePath = 0x20,
        RemoveQuery = 0x40,
        RemoveFragment = 0x80,
        StripTrailingSlash = 0x400,
        NormalizePathSegments = 0x1000
    };
    Q_DECLARE_FLAGS(FormattingOptions, UrlFormattingOption)
    enum ComponentFormattingOption { PrettyDecoded = 0x0, FullyEncoded = 0x100000, FullyDecoded = 0x200000 };
    Q_DECLARE_FLAGS(ComponentFormattingOptions, ComponentFormattingOption)

    QUrl() = default;
    QUrl(const QString &url, ParsingMode mode = TolerantMode);

    void setUrl(const QString &url, ParsingMode mode = TolerantMode);
    QString url(FormattingOptions options = None) const { return toString(options); }
    QString toString(FormattingOptions options = None) const;
    QString toDisplayString(FormattingOptions options = RemovePassword) const { return toString(options); }
    QByteArray toEncoded(FormattingOptions options = None) const;
    QUrl adjusted(FormattingOptions options) const;

    bool isValid() const { return m_valid && !isEmpty(); }
    bool isEmpty() const;
    bool isRelative() const { return m_scheme.isEmpty(); }
    bool isLocalFile() const { return m_scheme == QString("file"); }
    void clear() { *this = QUrl(); }
    QString errorString() const { return m_error; }

    void setScheme(const QString &scheme);
    QString scheme() const { return m_scheme; }
    void setAuthority(const QString &authority, ParsingMode mode = TolerantMode);
    QString authority(ComponentFormattingOptions options = PrettyDecoded) const;
    void setUserInfo(const QString &userInfo, ParsingMode mode = TolerantMode);
    QString userInfo(ComponentFormattingOptions options = PrettyDecoded) const;
    void setUserName(const QString &userName, ParsingMode mode = DecodedMode);
    QString userName(ComponentFormattingOptions options = FullyDecoded) const;
    void setPassword(const QString &password, ParsingMode mode = DecodedMode);
    QString password(ComponentFormattingOptions options = FullyDecoded) const;
    void setHost(const QString &host, ParsingMode mode = DecodedMode);
    QString host(ComponentFormattingOptions options = FullyDecoded) const;
    void setPort(int port);
    int port(int defaultPort = -1) const { return m_port < 0 ? defaultPort : m_port; }
    void setPath(const QString &path, ParsingMode mode = DecodedMode);
    QString path(ComponentFormattingOptions options = FullyDecoded) const;
    QString fileName(ComponentFormattingOptions options = FullyDecoded) const;
    bool hasQuery() const { return m_hasQuery; }
    void setQuery(const QString &query, ParsingMode mode = TolerantMode);
    QString query(ComponentFormattingOptions options = PrettyDecoded) const;
    bool hasFragment() const { return m_hasFragment; }
    void setFragment(const QString &fragment, ParsingMode mode = TolerantMode);
    QString fragment(ComponentFormattingOptions options = PrettyDecoded) const;

    QUrl resolved(const QUrl &relative) const;
    bool isParentOf(const QUrl &url) const;
    QString toLocalFile() const;

    static QUrl fromLocalFile(const QString &localFile);
    static QUrl fromEncoded(const QByteArray &input, ParsingMode mode = TolerantMode);
    static QByteArray toPercentEncoding(const QString &input, const QByteArray &exclude = QByteArray(), const QByteArray &include = QByteArray());
    static QString fromPercentEncoding(const QByteArray &input);

    friend bool operator==(const QUrl &a, const QUrl &b) { return a.toEncoded() == b.toEncoded(); }
    friend bool operator!=(const QUrl &a, const QUrl &b) { return !(a == b); }
    friend bool operator<(const QUrl &a, const QUrl &b) { return a.toEncoded() < b.toEncoded(); }

private:
    void fail(const QString &message);
    void parse(const QString &text, ParsingMode mode);
    QByteArray encodedAuthority() const;

    // Components are held percent-encoded, except the scheme and host.
    QString m_scheme;
    QByteArray m_user;
    QByteArray m_password;
    QString m_host;
    int m_port = -1;
    QByteArray m_path;
    QByteArray m_query;
    QByteArray m_fragment;
    bool m_hasUser = false;
    bool m_hasPassword = false;
    bool m_hasHost = false;
    bool m_hasQuery = false;
    bool m_hasFragment = false;
    bool m_valid = true;
    QString m_error;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QUrl::FormattingOptions)
Q_DECLARE_OPERATORS_FOR_FLAGS(QUrl::ComponentFormattingOptions)

std::size_t qHash(const QUrl &url, std::size_t seed = 0) noexcept;
