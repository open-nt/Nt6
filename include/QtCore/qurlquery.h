// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qurl.h>

#include <initializer_list>
#include <utility>

class QUrlQuery {
public:
    QUrlQuery() = default;
    explicit QUrlQuery(const QUrl &url);
    explicit QUrlQuery(const QString &queryString) { setQuery(queryString); }
    QUrlQuery(std::initializer_list<std::pair<QString, QString>> list);

    bool isEmpty() const { return m_items.isEmpty(); }
    void clear() { m_items.clear(); }
    void setQuery(const QString &queryString);
    QString query(QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded) const;
    QString toString(QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded) const { return query(encoding); }
    void setQueryItems(const QList<std::pair<QString, QString>> &query) { m_items = query; }
    QList<std::pair<QString, QString>>
    queryItems(QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded) const;
    bool hasQueryItem(const QString &key) const;
    void addQueryItem(const QString &key, const QString &value) { m_items.append({key, value}); }
    void removeQueryItem(const QString &key);
    QString queryItemValue(const QString &key, QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded) const;
    QStringList allQueryItemValues(const QString &key,
                                   QUrl::ComponentFormattingOptions encoding = QUrl::PrettyDecoded) const;
    void removeAllQueryItems(const QString &key);

    void setQueryDelimiters(QChar valueDelimiter, QChar pairDelimiter);
    /*!
        Returns the character that separates a key from its value; \c{=}
        unless setQueryDelimiters() changed it.
    */
    QChar queryValueDelimiter() const { return m_valueDelimiter; }
    /*!
        Returns the character that separates the pairs; \c{&} unless
        setQueryDelimiters() changed it.
    */
    QChar queryPairDelimiter() const { return m_pairDelimiter; }
    /*!
        Returns the default value delimiter, \c{=}.
    */
    static constexpr char16_t defaultQueryValueDelimiter() noexcept { return u'='; }
    /*!
        Returns the default pair delimiter, \c{&}.
    */
    static constexpr char16_t defaultQueryPairDelimiter() noexcept { return u'&'; }
    /*!
        Swaps this query with \a other. This operation is very fast and never
        fails.
    */
    void swap(QUrlQuery &other) noexcept {
        m_items.swap(other.m_items);
        std::swap(m_valueDelimiter, other.m_valueDelimiter);
        std::swap(m_pairDelimiter, other.m_pairDelimiter);
    }

    friend bool operator==(const QUrlQuery &a, const QUrlQuery &b) {
        return a.m_items == b.m_items && a.m_valueDelimiter == b.m_valueDelimiter &&
               a.m_pairDelimiter == b.m_pairDelimiter;
    }
    friend bool operator!=(const QUrlQuery &a, const QUrlQuery &b) { return !(a == b); }

private:
    QString format(const QString &s, QUrl::ComponentFormattingOptions encoding) const;

    QList<std::pair<QString, QString>> m_items;
    QChar m_valueDelimiter = defaultQueryValueDelimiter();
    QChar m_pairDelimiter = defaultQueryPairDelimiter();
};
