// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>

#include <string>

/*!
    \class QStringMatcher
    \brief The QStringMatcher class holds a sequence of characters that can be quickly matched in a Unicode string.

    This class is useful when you have a sequence of QChars that you want to
    repeatedly match against some strings (perhaps in a loop), or when you
    want to search for the same pattern in many strings. It uses the
    Boyer-Moore-Horspool algorithm, which builds a table of the pattern once
    so that every search can skip characters of the text.

    \code
    QStringMatcher matcher("needle", Qt::CaseInsensitive);
    qsizetype i = matcher.indexIn("Hay NEEDLE hay");   // 4
    \endcode

    \differences
    Case-insensitive matching compares characters folded with
    QChar::toCaseFolded(), which in Nt covers ASCII, Latin-1, Latin Extended-A,
    Greek and Cyrillic. A match is reported at a code unit index and may begin
    or end inside a surrogate pair only if the pattern does. There is no
    QByteArrayMatcher.
    \enddifferences

    \sa QString::indexOf(), QStringView::indexOf()
*/
class QStringMatcher {
public:
    /*!
        Constructs an empty matcher. It matches the empty pattern.
    */
    QStringMatcher() = default;
    QStringMatcher(const QString &pattern, Qt::CaseSensitivity cs = Qt::CaseSensitive);
    QStringMatcher(const QChar *uc, qsizetype len, Qt::CaseSensitivity cs = Qt::CaseSensitive);
    QStringMatcher(QStringView pattern, Qt::CaseSensitivity cs = Qt::CaseSensitive);

    void setPattern(const QString &pattern);
    void setCaseSensitivity(Qt::CaseSensitivity cs);
    qsizetype indexIn(const QString &str, qsizetype from = 0) const;
    qsizetype indexIn(const QChar *str, qsizetype length, qsizetype from = 0) const;
    qsizetype indexIn(QStringView str, qsizetype from = 0) const;
    /*!
        Returns the pattern that was set.

        \sa setPattern()
    */
    QString pattern() const { return m_pattern; }
    /*!
        Returns the case sensitivity of the matcher.

        \sa setCaseSensitivity()
    */
    Qt::CaseSensitivity caseSensitivity() const noexcept { return m_cs; }

private:
    void build();

    QString m_pattern;
    std::u16string m_key;
    Qt::CaseSensitivity m_cs = Qt::CaseSensitive;
    qsizetype m_skip[256] = {};
};
