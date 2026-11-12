// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtQml/qjsvalue.h>

class QJSValueIterator {
public:
    explicit QJSValueIterator(const QJSValue &object);
    ~QJSValueIterator();
    QJSValueIterator(const QJSValueIterator &) = delete;
    QJSValueIterator &operator=(const QJSValueIterator &) = delete;

    bool hasNext() const;
    bool next();
    QString name() const;
    QJSValue value() const;
    QJSValueIterator &operator=(QJSValue &object);

private:
    QJSValue m_object;
    QStringList m_keys;
    qsizetype m_index = -1;
};
