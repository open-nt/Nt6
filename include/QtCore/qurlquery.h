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
    QList<std::pair<QString, QString>> queryItems() const { return m_items; }
    bool hasQueryItem(const QString &key) const;
    void addQueryItem(const QString &key, const QString &value) { m_items.append({key, value}); }
    void removeQueryItem(const QString &key);
    QString queryItemValue(const QString &key) const;
    QStringList allQueryItemValues(const QString &key) const;
    void removeAllQueryItems(const QString &key);

    friend bool operator==(const QUrlQuery &a, const QUrlQuery &b) { return a.m_items == b.m_items; }
    friend bool operator!=(const QUrlQuery &a, const QUrlQuery &b) { return !(a == b); }

private:
    QList<std::pair<QString, QString>> m_items;
};
