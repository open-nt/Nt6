// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

/*!
    \class QStringList
    \brief The QStringList class provides a list of strings.

    QStringList is a QList<QString> with convenience functions for working
    with strings: join(), filter(), replaceInStrings(), removeDuplicates() and
    sort(). Lists are usually produced by QString::split() and combined back
    with join():

    \code
    QStringList fields = QString("a,b,c").split(',');
    fields << "d";
    QString csv = fields.join(',');   // "a,b,c,d"
    \endcode

    \differences
    QStringList derives from QList<QString> as in Qt 6. contains(),
    indexOf(), filter() and replaceInStrings() take plain QString arguments
    only: there are no QStringView, QLatin1StringView or QRegularExpression
    overloads. \c lastIndexOf() is the inherited QList one and has no case
    sensitivity argument. join() accepts a QString or QChar separator only.
    \enddifferences

    \sa QString, QList
*/
class QStringList : public QList<QString> {
public:
    /*!
        Constructs an empty list.
    */
    QStringList() = default;
    /*!
        \overload

        Constructs a list containing the single string \a s.
    */
    QStringList(const QString &s) { append(s); }
    /*!
        \overload

        Constructs a list containing the strings in \a args.
    */
    QStringList(std::initializer_list<QString> args) : QList<QString>(args) {}
    /*!
        \overload

        Constructs a copy of the QList<QString> \a l.
    */
    QStringList(const QList<QString> &l) : QList<QString>(l) {}
    /*!
        \overload

        Constructs a list by moving the contents of the QList<QString> \a l.
    */
    QStringList(QList<QString> &&l) : QList<QString>(std::move(l)) {}
    /*!
        \overload

        Constructs a list containing the strings in the range from \a first
        up to, but not including, \a last.
    */
    template <typename It, typename = typename std::iterator_traits<It>::iterator_category>
    QStringList(It first, It last) : QList<QString>(first, last) {}

    /*!
        Returns the strings in the list joined into one string, with \a sep
        inserted between consecutive strings. An empty list gives an empty
        string.

        \sa QString::split()
    */
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
    /*!
        \overload

        Joins the strings in the list using the character \a sep as the
        separator.
    */
    QString join(QChar sep) const { return join(QString(sep)); }

    /*!
        Returns \c true if the list contains the string \a s, using the case
        sensitivity \a cs; otherwise returns \c false.

        \sa indexOf()
    */
    bool contains(const QString &s, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        return indexOf(s, 0, cs) != -1;
    }
    /*!
        Returns the index position of the first string in the list equal to
        \a s, searching forward from index position \a from, or -1 if there is
        none. \a cs selects the case sensitivity.

        \sa contains()
    */
    qsizetype indexOf(const QString &s, qsizetype from = 0, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        for (qsizetype i = from < 0 ? 0 : from; i < size(); ++i)
            if (at(i).compare(s, cs) == 0)
                return i;
        return -1;
    }
    /*!
        Returns a list of the strings that contain the substring \a needle,
        using the case sensitivity \a cs.

        \sa QString::contains()
    */
    QStringList filter(const QString &needle, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        QStringList r;
        for (const QString &s : *this)
            if (s.contains(needle, cs))
                r.append(s);
        return r;
    }
    /*!
        Replaces every occurrence of the string \a before with \a after in
        each string of the list, using the case sensitivity \a cs, and returns
        a reference to the list.

        \sa QString::replace()
    */
    QStringList &replaceInStrings(const QString &before, const QString &after, Qt::CaseSensitivity cs = Qt::CaseSensitive) {
        for (QString &s : *this)
            s.replace(before, after, cs);
        return *this;
    }
    /*!
        Removes duplicate strings from the list, keeping the first occurrence
        of each, and returns the number of strings removed. Strings are
        compared case sensitively.
    */
    qsizetype removeDuplicates() {
        QStringList r;
        for (const QString &s : *this)
            if (!r.QList<QString>::contains(s))
                r.append(s);
        const qsizetype removed = size() - r.size();
        *this = r;
        return removed;
    }
    /*!
        Sorts the list in ascending order, comparing strings with
        QString::compare() and the case sensitivity \a cs. The sort is not
        guaranteed to be stable.
    */
    void sort(Qt::CaseSensitivity cs = Qt::CaseSensitive) {
        std::sort(begin(), end(), [cs](const QString &a, const QString &b) { return a.compare(b, cs) < 0; });
    }
    /*!
        Appends the string \a s to the list and returns a reference to the
        list.
    */
    QStringList &operator<<(const QString &s) { append(s); return *this; }
    /*!
        \overload

        Appends the strings of the list \a l to this list.
    */
    QStringList &operator<<(const QStringList &l) { append(l); return *this; }
};
