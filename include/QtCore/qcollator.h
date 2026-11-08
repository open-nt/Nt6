// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlocale.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>

#include <utility>

class QCollator;

/*!
    \class QCollatorSortKey
    \since 5.2
    \brief The QCollatorSortKey class can be used to speed up string collation.

    A sort key is obtained from QCollator::sortKey(). Comparing two keys with
    compare() gives the same result as comparing their strings with the
    collator, but is faster when each string is compared many times, as when
    sorting.

    \differences
    The key is an Nt-specific byte sequence; keys from different collator
    settings must not be compared.
    \enddifferences

    \sa QCollator
*/
class QCollatorSortKey {
public:
    /*!
        Constructs a copy of \a other.
    */
    QCollatorSortKey(const QCollatorSortKey &other) = default;
    /*!
        Move-constructs a key from \a other.
    */
    QCollatorSortKey(QCollatorSortKey &&other) noexcept = default;
    /*!
        Destroys the key.
    */
    ~QCollatorSortKey() = default;
    /*!
        Assigns \a other to this key and returns a reference to it.
    */
    QCollatorSortKey &operator=(const QCollatorSortKey &other) = default;
    /*!
        Move-assigns \a other to this key and returns a reference to it.
    */
    QCollatorSortKey &operator=(QCollatorSortKey &&other) noexcept = default;
    /*!
        Swaps this key with \a other.
    */
    void swap(QCollatorSortKey &other) noexcept { std::swap(m_key, other.m_key); }
    /*!
        Returns a negative number, zero or a positive number if this key sorts
        before, equal to or after \a otherKey.
    */
    int compare(const QCollatorSortKey &otherKey) const {
        const int r = m_key.compare(otherKey.m_key);
        return r < 0 ? -1 : (r > 0 ? 1 : 0);
    }
    friend bool operator<(const QCollatorSortKey &a, const QCollatorSortKey &b) { return a.compare(b) < 0; }

private:
    friend class QCollator;
    explicit QCollatorSortKey(QByteArray key) : m_key(std::move(key)) {}
    QByteArray m_key;
};

/*!
    \class QCollator
    \since 5.2
    \brief The QCollator class compares strings according to a localized collation algorithm.

    A collator orders strings as people of a locale expect, unlike
    QString::compare(), which compares code units: letters are compared
    before accents, accents before case, and with numericMode() digits are
    compared by value.

    \code
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(files.begin(), files.end(), collator);   // "file2" before "file10"
    \endcode

    \differences
    Nt implements its own simplified collation instead of using ICU: letters
    are compared by their base letter (with Latin accents removed), then by
    accent, then by case (lowercase first). Letters of other scripts sort
    after Latin by code point. Tailorings exist for the Nordic languages
    (å, ä, æ, ö, ø after z), Spanish (ñ), Polish and Czech; other languages
    use the default order. Expansions are limited to ß, æ, œ and similar
    letters, and there are no contractions such as Czech "ch".
    \enddifferences

    \sa QCollatorSortKey, QLocale
*/
class QCollator {
public:
    QCollator();
    explicit QCollator(const QLocale &locale);
    /*!
        Constructs a copy of \a other.
    */
    QCollator(const QCollator &other) = default;
    /*!
        Move-constructs a collator from \a other.
    */
    QCollator(QCollator &&other) noexcept = default;
    /*!
        Destroys the collator.
    */
    ~QCollator() = default;
    /*!
        Assigns \a other to this collator and returns a reference to it.
    */
    QCollator &operator=(const QCollator &other) = default;
    /*!
        Move-assigns \a other to this collator and returns a reference to it.
    */
    QCollator &operator=(QCollator &&other) noexcept = default;
    /*!
        Swaps this collator with \a other.
    */
    void swap(QCollator &other) noexcept {
        m_locale.swap(other.m_locale);
        std::swap(m_cs, other.m_cs);
        std::swap(m_numeric, other.m_numeric);
        std::swap(m_ignorePunctuation, other.m_ignorePunctuation);
    }

    /*!
        Sets the locale of the collator to \a locale.

        \sa locale()
    */
    void setLocale(const QLocale &locale) { m_locale = locale; }
    /*!
        Returns the locale of the collator.

        \sa setLocale()
    */
    QLocale locale() const { return m_locale; }
    /*!
        Returns the case sensitivity of the collator. The default is
        Qt::CaseSensitive.

        \sa setCaseSensitivity()
    */
    Qt::CaseSensitivity caseSensitivity() const { return m_cs; }
    /*!
        Sets the case sensitivity of the collator to \a cs. With
        Qt::CaseInsensitive, strings that differ only in case compare equal.

        \sa caseSensitivity()
    */
    void setCaseSensitivity(Qt::CaseSensitivity cs) { m_cs = cs; }
    /*!
        Sets numeric mode to \a on. In numeric mode, runs of digits are
        compared by their numeric value, so "2" sorts before "10".

        \sa numericMode()
    */
    void setNumericMode(bool on) { m_numeric = on; }
    /*!
        Returns \c true if numeric mode is on; otherwise returns \c false.

        \sa setNumericMode()
    */
    bool numericMode() const { return m_numeric; }
    /*!
        Sets whether punctuation, symbols and white space are ignored to
        \a on.

        \sa ignorePunctuation()
    */
    void setIgnorePunctuation(bool on) { m_ignorePunctuation = on; }
    /*!
        Returns \c true if punctuation, symbols and white space are ignored;
        otherwise returns \c false.

        \sa setIgnorePunctuation()
    */
    bool ignorePunctuation() const { return m_ignorePunctuation; }

    int compare(QStringView s1, QStringView s2) const;
    /*!
        \overload
        Compares \a s1 with \a s2 and returns a negative number, zero or a
        positive number if \a s1 sorts before, equal to or after \a s2.
    */
    int compare(const QString &s1, const QString &s2) const { return compare(QStringView(s1), QStringView(s2)); }
    /*!
        \overload
        Compares the \a len1 characters at \a s1 with the \a len2 characters
        at \a s2.
    */
    int compare(const QChar *s1, qsizetype len1, const QChar *s2, qsizetype len2) const {
        return compare(QStringView(s1, len1), QStringView(s2, len2));
    }
    /*!
        Returns \c true if \a s1 sorts before \a s2. This makes a collator
        usable as the comparison function of \c std::sort.
    */
    bool operator()(QStringView s1, QStringView s2) const { return compare(s1, s2) < 0; }
    /*!
        \overload
        Returns \c true if \a s1 sorts before \a s2.
    */
    bool operator()(const QString &s1, const QString &s2) const { return compare(s1, s2) < 0; }
    QCollatorSortKey sortKey(const QString &string) const;

    static int defaultCompare(QStringView s1, QStringView s2);
    static QCollatorSortKey defaultSortKey(QStringView key);

private:
    QByteArray key(QStringView s) const;
    QLocale m_locale;
    Qt::CaseSensitivity m_cs = Qt::CaseSensitive;
    bool m_numeric = false;
    bool m_ignorePunctuation = false;
};
