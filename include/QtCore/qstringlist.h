// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

#include <type_traits>

class QStringMatcher;
class QStringView;

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

    QStringList is another name of QList<QString>, so everything that QList
    has is there too, and a function that takes one takes the other.

    contains(), indexOf() and lastIndexOf() are those of QList when they
    are called without a case sensitivity, and compare case sensitively; the
    functions here are the ones that take a Qt::CaseSensitivity.

    \differences
    The QStringView and
    QLatin1StringView overloads of join(), contains(), indexOf(),
    lastIndexOf(), filter() and replaceInStrings() are function templates that
    accept exactly those two types, so that a string literal selects the
    QString overload; they convert the view to a QString once per call.
    \enddifferences

    \sa QString, QList
*/
template <> struct QListSpecialMethods<QString> {
    /*!
        Returns the strings in the list joined into one string, with \a sep
        inserted between consecutive strings. An empty list gives an empty
        string.

        \sa QString::split()
    */
    QString join(const QString &sep) const {
        QString r;
        bool first = true;
        for (const QString &s : self()) {
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
    bool contains(const QString &s, Qt::CaseSensitivity cs) const { return indexOf(s, 0, cs) != -1; }
    /*!
        Returns the index position of the first string in the list equal to
        \a s, searching forward from index position \a from, or -1 if there is
        none. \a cs selects the case sensitivity.

        \sa contains()
    */
    qsizetype indexOf(const QString &s, qsizetype from, Qt::CaseSensitivity cs) const {
        for (qsizetype i = from < 0 ? 0 : from; i < self().size(); ++i)
            if (self().at(i).compare(s, cs) == 0)
                return i;
        return -1;
    }
    /*!
        Returns a list of the strings that contain the substring \a needle,
        using the case sensitivity \a cs.

        \sa QString::contains()
    */
    QList<QString> filter(const QString &needle, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        QList<QString> r;
        for (const QString &s : self())
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
    QList<QString> &replaceInStrings(const QString &before, const QString &after,
                                     Qt::CaseSensitivity cs = Qt::CaseSensitive) {
        for (QString &s : self())
            s.replace(before, after, cs);
        return self();
    }
    /*!
        \since 6.7
        Returns the index of the last string equal to \a s, searching backward
        from index position \a from and using the case sensitivity \a cs, or
        -1 if there is none. A negative \a from counts from the end of the
        list, so the default of -1 starts at the last string.

        \sa indexOf(), contains()
    */
    qsizetype lastIndexOf(const QString &s, qsizetype from, Qt::CaseSensitivity cs) const {
        for (qsizetype i = from < 0 ? self().size() + from : qMin(from, self().size() - 1); i >= 0; --i)
            if (self().at(i).compare(s, cs) == 0)
                return i;
        return -1;
    }

private:
    // The view overloads below accept these two types only: a string literal converts to QString
    // and to the views alike, and must keep selecting the QString overloads.
    template <typename S>
    using if_view = std::enable_if_t<std::is_same_v<S, QStringView> || std::is_same_v<S, QLatin1StringView>, int>;
    template <typename B, typename A>
    using if_views = std::enable_if_t<(std::is_same_v<B, QStringView> || std::is_same_v<A, QStringView>) &&
                                          (std::is_same_v<B, QStringView> || std::is_same_v<B, QString>) &&
                                          (std::is_same_v<A, QStringView> || std::is_same_v<A, QString>),
                                      int>;
    static const QString &text(const QString &s) { return s; }
    template <typename S> static QString text(S view) { return view.toString(); }

public:
    /*!
        \overload
        \since 5.14

        Joins the strings with the separator \a sep, a QStringView or a
        QLatin1StringView.
    */
    template <typename S, if_view<S> = 0> QString join(S sep) const { return join(text(sep)); }
    /*!
        \overload
        \since 5.12

        Returns \c true if the list contains the string \a s, a QStringView or
        a QLatin1StringView, using the case sensitivity \a cs.
    */
    template <typename S, if_view<S> = 0> bool contains(S s, Qt::CaseSensitivity cs) const {
        return indexOf(text(s), 0, cs) != -1;
    }
    /*!
        \overload
        \since 5.13

        Returns the index of the first string equal to \a s, a QStringView or
        a QLatin1StringView, searching forward from \a from and using the case
        sensitivity \a cs, or -1.
    */
    template <typename S, if_view<S> = 0> qsizetype indexOf(S s, qsizetype from, Qt::CaseSensitivity cs) const {
        return indexOf(text(s), from, cs);
    }
    /*!
        \overload
        \since 5.13

        Returns the index of the last string equal to \a s, a QStringView or a
        QLatin1StringView, searching backward from \a from and using the case
        sensitivity \a cs, or -1.
    */
    template <typename S, if_view<S> = 0> qsizetype lastIndexOf(S s, qsizetype from, Qt::CaseSensitivity cs) const {
        return lastIndexOf(text(s), from, cs);
    }
    /*!
        \overload
        \since 5.14

        Returns the strings that contain \a needle, a QStringView or a
        QLatin1StringView, using the case sensitivity \a cs.
    */
    template <typename S, if_view<S> = 0>
    QList<QString> filter(S needle, Qt::CaseSensitivity cs = Qt::CaseSensitive) const {
        return filter(text(needle), cs);
    }
    /*!
        \overload
        \since 5.14

        Replaces every occurrence of \a before with \a after in each string,
        using the case sensitivity \a cs. At least one of \a before and
        \a after is a QStringView and the other may be a QString.
    */
    template <typename B, typename A, if_views<B, A> = 0>
    QList<QString> &replaceInStrings(const B &before, const A &after, Qt::CaseSensitivity cs = Qt::CaseSensitive) {
        return replaceInStrings(text(before), text(after), cs);
    }
    /*!
        \overload
        \since 6.7

        Returns the strings in which \a matcher finds its pattern, with the case
        sensitivity of the matcher. This is faster than the QString overload for
        a long list, because the matcher prepares the pattern once.

        This function is defined in \c{<QStringMatcher>}.
    */
    QList<QString> filter(const QStringMatcher &matcher) const;
    /*!
        \since 5.0
        Returns the strings of the list that \a re matches somewhere.

        \sa QString::contains()
    */
    QList<QString> filter(const QRegularExpression &re) const;
    /*!
        \overload
        \since 5.0
        Returns the index of the first string in the list, searching forward from
        \a from, that \a re matches exactly (the whole string), or -1.
    */
    qsizetype indexOf(const QRegularExpression &re, qsizetype from = 0) const;
    /*!
        \overload
        \since 5.0
        Returns the index of the last string in the list, searching backward from
        \a from, that \a re matches exactly, or -1. A negative \a from counts from
        the end of the list.
    */
    qsizetype lastIndexOf(const QRegularExpression &re, qsizetype from = -1) const;
    /*!
        \overload
        \since 5.0
        Replaces every match of \a re with \a after in each string of the list,
        as QString::replace() does, and returns a reference to the list.
    */
    QList<QString> &replaceInStrings(const QRegularExpression &re, const QString &after);
    /*!
        Removes duplicate strings from the list, keeping the first occurrence
        of each, and returns the number of strings removed. Strings are
        compared case sensitively.
    */
    qsizetype removeDuplicates() {
        QList<QString> r;
        for (const QString &s : self())
            if (!r.contains(s))
                r.append(s);
        const qsizetype removed = self().size() - r.size();
        self() = r;
        return removed;
    }
    /*!
        Sorts the list in ascending order, comparing strings with
        QString::compare() and the case sensitivity \a cs. The sort is not
        guaranteed to be stable.
    */
    void sort(Qt::CaseSensitivity cs = Qt::CaseSensitive) {
        std::sort(self().begin(), self().end(),
                  [cs](const QString &a, const QString &b) { return a.compare(b, cs) < 0; });
    }

protected:
    /*! \internal */
    ~QListSpecialMethods() = default;

private:
    QList<QString> &self() { return *static_cast<QList<QString> *>(this); }
    const QList<QString> &self() const { return *static_cast<const QList<QString> *>(this); }
};
