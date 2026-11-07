// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

class QStringList : public QList<QString> {
public:
    QStringList() = default;
    QStringList(const QString &s) { append(s); }
    QStringList(std::initializer_list<QString> args) : QList<QString>(args) {}
    QStringList(const QList<QString> &l) : QList<QString>(l) {}
    QStringList(QList<QString> &&l) : QList<QString>(std::move(l)) {}
    template <typename It, typename = typename std::iterator_traits<It>::iterator_category>
    QStringList(It first, It last) : QList<QString>(first, last) {}

    QString join(const QString &sep) const {
        QString r;
        bool first = true;
        for (const QString &s : *this) {
            if (!first)
                r += sep;
            r += s;
            first = false;
        }
        return r;
    }
    QString join(QChar sep) const { return join(QString(sep)); }

    bool contains(const QString &s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        return indexOf(s, 0, cs) != -1;
    }
    qsizetype indexOf(const QString &s, qsizetype from = 0, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        for (qsizetype i = from < 0 ? 0 : from; i < size(); ++i)
            if (at(i).compare(s, cs) == 0)
                return i;
        return -1;
    }
    QStringList filter(const QString &needle, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        QStringList r;
        for (const QString &s : *this)
            if (s.contains(needle, cs))
                r.append(s);
        return r;
    }
    QStringList &replaceInStrings(const QString &before, const QString &after, Qt::CaseSensitivity cs = Qt::CaseSensitive) {
        for (QString &s : *this)
            s.replace(before, after, cs);
        return *this;
    }
    qsizetype removeDuplicates() {
        QStringList r;
        for (const QString &s : *this)
            if (!r.QList<QString>::contains(s))
                r.append(s);
        const qsizetype removed = size() - r.size();
        *this = r;
        return removed;
    }
    void sort(Qt::CaseSensitivity cs = Qt::CaseSensitive) {
        std::sort(begin(), end(), [cs](const QString &a, const QString &b) { return a.compare(b, cs) < 0; });
    }
    QStringList &operator<<(const QString &s) { append(s); return *this; }
    QStringList &operator<<(const QStringList &l) { append(l); return *this; }
};
