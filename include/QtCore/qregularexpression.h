// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qstringview.h>

#include <memory>

class QRegularExpressionMatch;
class QRegularExpressionMatchIterator;
struct QRegularExpressionPrivate;
struct QRegularExpressionMatchPrivate;
struct QRegularExpressionMatchIteratorPrivate;

/*!
    \class QRegularExpression
    \since 5.0
    \brief The QRegularExpression class provides pattern matching using regular expressions.

    A QRegularExpression holds a Perl-compatible pattern. match() looks for
    the first match in a string and returns a QRegularExpressionMatch with
    the captured groups; globalMatch() iterates over all matches:

    \code
    QRegularExpression re(u"(\\d+)-(?<month>\\d+)"_s);
    QRegularExpressionMatch m = re.match(u"due 2024-06"_s);
    if (m.hasMatch()) {
        QString year = m.captured(1);       // "2024"
        QString month = m.captured("month"); // "06"
    }

    for (const QRegularExpressionMatch &word : QRegularExpression(u"\\w+"_s).globalMatch(text))
        words << word.captured();
    \endcode

    The pattern syntax follows PCRE2: literals and escapes, character classes
    (including POSIX classes and \c{\p{...}} properties), the anchors \c{^},
    \c{$}, \c{\A}, \c{\z}, \c{\Z}, \c{\b}, \c{\B} and \c{\G}, greedy, lazy and
    possessive quantifiers, capturing, named, non-capturing and atomic groups,
    lookahead and lookbehind, back references, inline option settings such as
    \c{(?i)}, \c{\Q...\E} quoting, \c{\K} and comments.

    \differences
    Nt has its own backtracking engine instead of PCRE2, so there is no JIT,
    and optimize() does nothing. Recursion and subroutine calls
    (\c{(?R)}, \c{(?1)}), conditional groups, branch reset groups, callouts
    and backtracking control verbs are not supported and make the pattern
    invalid. Case-insensitive matching folds characters with
    QChar::toCaseFolded(). Without UseUnicodePropertiesOption, \c{\w},
    \c{\d}, \c{\s} and \c{\b} are ASCII-only as in PCRE2; \c{\p{...}}
    supports the general categories and \c{Any}, but not scripts. A match
    that needs too deep a recursion fails as if there were no match. Error
    messages are in English and follow PCRE2's wording.
    \enddifferences

    \sa QRegularExpressionMatch, QRegularExpressionMatchIterator, QString::indexOf()
*/
class QRegularExpression {
public:
    /*!
        \enum QRegularExpression::PatternOption

        \value NoPatternOption No option is set.
        \value CaseInsensitiveOption Letters match regardless of case.
        \value DotMatchesEverythingOption The dot also matches newlines.
        \value MultilineOption \c{^} and \c{$} also match at the start and end
               of each line.
        \value ExtendedPatternSyntaxOption Whitespace in the pattern is
               ignored and \c{#} starts a comment, outside character classes.
        \value InvertedGreedinessOption Quantifiers are lazy by default and
               become greedy when followed by \c{?}.
        \value DontCaptureOption Plain groups do not capture; named groups
               still do.
        \value UseUnicodePropertiesOption \c{\w}, \c{\d}, \c{\s}, \c{\b} and
               the POSIX classes use Unicode properties instead of ASCII.
    */
    enum PatternOption {
        NoPatternOption = 0x0000,
        CaseInsensitiveOption = 0x0001,
        DotMatchesEverythingOption = 0x0002,
        MultilineOption = 0x0004,
        ExtendedPatternSyntaxOption = 0x0008,
        InvertedGreedinessOption = 0x0010,
        DontCaptureOption = 0x0020,
        UseUnicodePropertiesOption = 0x0040
    };
    Q_DECLARE_FLAGS(PatternOptions, PatternOption)

    /*!
        \enum QRegularExpression::MatchType

        \value NormalMatch A normal match.
        \value PartialPreferCompleteMatch A complete match is preferred; a
               partial match (the subject ended while the pattern could still
               match) is reported only if there is no complete match.
        \value PartialPreferFirstMatch The first match found is reported,
               partial or complete.
        \value NoMatch No matching is done; the match object is valid but has
               no match. Useful as a default value.
    */
    enum MatchType { NormalMatch = 0, PartialPreferCompleteMatch, PartialPreferFirstMatch, NoMatch };

    /*!
        \enum QRegularExpression::MatchOption

        \value NoMatchOption No option is set.
        \value AnchorAtOffsetMatchOption The match must start exactly at the
               offset passed to match().
        \value AnchoredMatchOption Same as AnchorAtOffsetMatchOption.
        \value DontCheckSubjectStringMatchOption Accepted for compatibility;
               the subject is never checked for UTF-16 validity.
    */
    enum MatchOption {
        NoMatchOption = 0x0000,
        AnchorAtOffsetMatchOption = 0x0001,
        AnchoredMatchOption = AnchorAtOffsetMatchOption,
        DontCheckSubjectStringMatchOption = 0x0002
    };
    Q_DECLARE_FLAGS(MatchOptions, MatchOption)

    /*!
        \enum QRegularExpression::WildcardConversionOption
        \since 6.0

        \value DefaultWildcardConversion The result is anchored and the
               wildcards do not match the path separator \c{/}.
        \value UnanchoredWildcardConversion The result is not anchored.
        \value NonPathWildcardConversion The wildcards also match \c{/}.
    */
    enum WildcardConversionOption { DefaultWildcardConversion = 0x0, UnanchoredWildcardConversion = 0x1, NonPathWildcardConversion = 0x2 };
    Q_DECLARE_FLAGS(WildcardConversionOptions, WildcardConversionOption)

    QRegularExpression();
    explicit QRegularExpression(const QString &pattern, PatternOptions options = NoPatternOption);
    QRegularExpression(const QRegularExpression &re);
    QRegularExpression(QRegularExpression &&re) noexcept;
    ~QRegularExpression();
    QRegularExpression &operator=(const QRegularExpression &re);
    QRegularExpression &operator=(QRegularExpression &&re) noexcept;
    /*!
        Swaps this regular expression with \a other.
    */
    void swap(QRegularExpression &other) noexcept { d.swap(other.d); }

    QString pattern() const;
    void setPattern(const QString &pattern);
    PatternOptions patternOptions() const;
    void setPatternOptions(PatternOptions options);

    bool isValid() const;
    qsizetype patternErrorOffset() const;
    QString errorString() const;
    int captureCount() const;
    QStringList namedCaptureGroups() const;

    QRegularExpressionMatch match(const QString &subject, qsizetype offset = 0, MatchType matchType = NormalMatch,
                                  MatchOptions matchOptions = NoMatchOption) const;
    QRegularExpressionMatch matchView(QStringView subjectView, qsizetype offset = 0, MatchType matchType = NormalMatch,
                                      MatchOptions matchOptions = NoMatchOption) const;
    QRegularExpressionMatchIterator globalMatch(const QString &subject, qsizetype offset = 0, MatchType matchType = NormalMatch,
                                                MatchOptions matchOptions = NoMatchOption) const;
    QRegularExpressionMatchIterator globalMatchView(QStringView subjectView, qsizetype offset = 0, MatchType matchType = NormalMatch,
                                                    MatchOptions matchOptions = NoMatchOption) const;
    void optimize() const;

    static QString escape(QStringView str);
    static QString wildcardToRegularExpression(QStringView pattern, WildcardConversionOptions options = DefaultWildcardConversion);
    static QRegularExpression fromWildcard(QStringView pattern, Qt::CaseSensitivity cs = Qt::CaseInsensitive,
                                           WildcardConversionOptions options = DefaultWildcardConversion);
    static QString anchoredPattern(QStringView expression);

    friend bool operator==(const QRegularExpression &a, const QRegularExpression &b) noexcept;
    friend bool operator!=(const QRegularExpression &a, const QRegularExpression &b) noexcept { return !(a == b); }

private:
    friend class QRegularExpressionMatch;
    friend struct QRegularExpressionMatchIteratorPrivate;
    friend class QRegularExpressionMatchIterator;
    std::shared_ptr<QRegularExpressionPrivate> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QRegularExpression::PatternOptions)
Q_DECLARE_OPERATORS_FOR_FLAGS(QRegularExpression::MatchOptions)
Q_DECLARE_OPERATORS_FOR_FLAGS(QRegularExpression::WildcardConversionOptions)

std::size_t qHash(const QRegularExpression &key, std::size_t seed = 0) noexcept;

/*!
    \class QRegularExpressionMatch
    \since 5.0
    \brief The QRegularExpressionMatch class provides the results of matching a QRegularExpression against a string.

    A match object reports whether the expression matched, and the text and
    position of each capturing group. Groups are numbered from 1 in the order
    of their opening parentheses; group 0 is the whole match. Named groups can
    also be looked up by name.

    \differences
    The match keeps its own copy of the subject string, so capturedView()
    refers into that copy and stays valid as long as the match object.
    \enddifferences

    \sa QRegularExpression, QRegularExpressionMatchIterator
*/
class QRegularExpressionMatch {
public:
    QRegularExpressionMatch();
    QRegularExpressionMatch(const QRegularExpressionMatch &match);
    QRegularExpressionMatch(QRegularExpressionMatch &&match) noexcept;
    ~QRegularExpressionMatch();
    QRegularExpressionMatch &operator=(const QRegularExpressionMatch &match);
    QRegularExpressionMatch &operator=(QRegularExpressionMatch &&match) noexcept;
    /*!
        Swaps this match with \a other.
    */
    void swap(QRegularExpressionMatch &other) noexcept { d.swap(other.d); }

    QRegularExpression regularExpression() const;
    QRegularExpression::MatchType matchType() const;
    QRegularExpression::MatchOptions matchOptions() const;
    bool hasMatch() const;
    bool hasPartialMatch() const;
    bool isValid() const;
    int lastCapturedIndex() const;

    bool hasCaptured(int nth) const;
    bool hasCaptured(QStringView name) const;
    QString captured(int nth = 0) const;
    QString captured(QStringView name) const;
    QStringView capturedView(int nth = 0) const;
    QStringView capturedView(QStringView name) const;
    QStringList capturedTexts() const;
    qsizetype capturedStart(int nth = 0) const;
    qsizetype capturedLength(int nth = 0) const;
    qsizetype capturedEnd(int nth = 0) const;
    qsizetype capturedStart(QStringView name) const;
    qsizetype capturedLength(QStringView name) const;
    qsizetype capturedEnd(QStringView name) const;

private:
    friend class QRegularExpression;
    friend struct QRegularExpressionMatchIteratorPrivate;
    friend class QRegularExpressionMatchIterator;
    explicit QRegularExpressionMatch(std::shared_ptr<QRegularExpressionMatchPrivate> dd);
    int groupForName(QStringView name) const;
    std::shared_ptr<QRegularExpressionMatchPrivate> d;
};

/*!
    \class QRegularExpressionMatchIterator
    \since 5.0
    \brief The QRegularExpressionMatchIterator class provides an iterator on the results of a global match of a QRegularExpression against a string.

    Call hasNext() and next() to walk over the matches, or iterate with a
    range-based \c for loop. After an empty match, the next search starts at
    the same position but must not be empty there; if that fails, it starts
    one character later.

    \differences
    None.
    \enddifferences

    \sa QRegularExpression::globalMatch()
*/
class QRegularExpressionMatchIterator {
public:
    QRegularExpressionMatchIterator();
    QRegularExpressionMatchIterator(const QRegularExpressionMatchIterator &iterator);
    QRegularExpressionMatchIterator(QRegularExpressionMatchIterator &&iterator) noexcept;
    ~QRegularExpressionMatchIterator();
    QRegularExpressionMatchIterator &operator=(const QRegularExpressionMatchIterator &iterator);
    QRegularExpressionMatchIterator &operator=(QRegularExpressionMatchIterator &&iterator) noexcept;
    /*!
        Swaps this iterator with \a other.
    */
    void swap(QRegularExpressionMatchIterator &other) noexcept { d.swap(other.d); }

    bool isValid() const;
    bool hasNext() const;
    QRegularExpressionMatch next();
    QRegularExpressionMatch peekNext() const;
    QRegularExpression regularExpression() const;
    QRegularExpression::MatchType matchType() const;
    QRegularExpression::MatchOptions matchOptions() const;

    /*!
        \class QRegularExpressionMatchIterator::Sentinel
        \brief The Sentinel class marks the end of the matches in a range-based for loop.

        \differences
        None.
        \enddifferences
    */
    struct Sentinel {};
    class RangeIterator;

    RangeIterator begin() const;
    /*!
        Returns the sentinel that ends a range-based \c for loop.
    */
    Sentinel end() const { return {}; }

private:
    friend class QRegularExpression;
    std::shared_ptr<QRegularExpressionMatchIteratorPrivate> d;
};

/*!
    \class QRegularExpressionMatchIterator::RangeIterator
    \brief The RangeIterator class lets a range-based for loop walk over the matches.

    \differences
    The class name is an Nt detail; Qt uses an unnamed iterator type.
    \enddifferences
*/
class QRegularExpressionMatchIterator::RangeIterator {
public:
    /*!
        Returns the current match.
    */
    const QRegularExpressionMatch &operator*() const { return m_current; }
    /*!
        Advances to the next match and returns a reference to this iterator.
    */
    RangeIterator &operator++() {
        fetch();
        return *this;
    }
    friend bool operator!=(const RangeIterator &it, Sentinel) { return !it.m_done; }
    friend bool operator==(const RangeIterator &it, Sentinel) { return it.m_done; }

private:
    friend class QRegularExpressionMatchIterator;
    explicit RangeIterator(QRegularExpressionMatchIterator it) : m_it(std::move(it)) { fetch(); }
    void fetch() {
        if (m_it.hasNext())
            m_current = m_it.next();
        else
            m_done = true;
    }
    QRegularExpressionMatchIterator m_it;
    QRegularExpressionMatch m_current;
    bool m_done = false;
};

/*!
    Returns an iterator to the first remaining match, for range-based \c for
    loops. Iterating does not advance this object.
*/
inline QRegularExpressionMatchIterator::RangeIterator QRegularExpressionMatchIterator::begin() const { return RangeIterator(*this); }
