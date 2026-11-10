// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qstringview.h>

#include <memory>

class QDataStream;
class QDebug;
class QRegularExpressionMatch;
class QRegularExpressionMatchIterator;
class QRegularExpressionPrivate;
class QRegularExpressionMatchPrivate;
class QRegularExpressionMatchIteratorPrivate;

/*!
    \class QRegularExpression
    \brief The QRegularExpression class provides pattern matching using regular expressions.

    A QRegularExpression holds a Perl-compatible regular expression pattern and its options.
    The pattern is compiled when it is set, so that \l{isValid()}{validity} and the error
    can be checked at once, and the compiled form is shared between copies. Matching is done
    with match(), which returns a QRegularExpressionMatch, or with globalMatch(), which
    returns a QRegularExpressionMatchIterator over all the non-overlapping matches.

    \code
    QRegularExpression re("^(\\d\\d?)/(\\d\\d?)/(\\d\\d\\d\\d)$");
    QRegularExpressionMatch m = re.match("08/12/1985");
    if (m.hasMatch()) {
        QString day = m.captured(1);   // "08"
        QString year = m.captured(3);  // "1985"
    }
    \endcode

    \section1 Pattern syntax

    The engine is written for Nt and follows PCRE2 and Perl:

    \list
    \li Literals and the escapes \c{\a \e \f \n \r \t \0dd \ddd \o{...} \xhh \x{h...} \cx
        \N{U+h...} \Q...\E}.
    \li Character classes with ranges, negation, class escapes (\c{\d \D \w \W \s \S \h \H \v \V}),
        POSIX classes (\c{[[:alpha:]]}, \c{[[:^digit:]]}), and Unicode properties
        (\c{\p{L}}, \c{\p{Lu}}, \c{\p{Nd}}, \c{\p{Greek}}, \c{\P{...}}, \c{\p{^...}}).
    \li \c{.}, \c{\N}, \c{\R}, \c{\X}, \c{\C}, and the anchors \c{^ $ \A \z \Z \G \b \B}, plus
        \c{\K} to reset the start of the match.
    \li Greedy, lazy (\c{*?}) and possessive (\c{*+}) quantifiers: \c{* + ? {n} {n,} {n,m}}.
    \li Groups: capturing, non-capturing \c{(?:...)}, named (\c{(?<name>...)}, \c{(?'name'...)},
        \c{(?P<name>...)}), atomic \c{(?>...)}, branch reset \c{(?|...)}, comments \c{(?#...)}.
    \li Lookahead \c{(?=...)}, \c{(?!...)} and lookbehind \c{(?<=...)}, \c{(?<!...)}. A lookbehind
        may be of variable length as long as it is bounded.
    \li Back references \c{\1}, \c{\g{-1}}, \c{\k<name>}, \c{(?P=name)}, and conditionals
        \c{(?(1)yes|no)}, \c{(?(<name>)...)}, \c{(?(?=...)yes|no)}, \c{(?(DEFINE)...)}.
    \li Inline options \c{(?i) (?m) (?s) (?x) (?n) (?U) (?^) (?i-m:...)}.
    \endlist

    Without UseUnicodePropertiesOption the class escapes, POSIX classes and word boundaries
    work on ASCII characters only; \c{\p{...}} always uses the Unicode data. Subjects are
    interpreted as UTF-16, so a surrogate pair is one character. The newline is \c{\n}.

    \section1 Limits

    A match that needs more than 10 million steps or 4 million backtracking states ends with
    no match and a warning, so that a catastrophic pattern cannot hang or exhaust memory. Groups
    can be nested 250 levels deep and a compiled pattern is limited to about 200000
    instructions.

    \section1 Unicode data

    General categories, scripts and simple case mappings come from the Unicode 16.0 database
    for the whole code space.

    \differences
    The engine is Nt's own; Qt uses PCRE2. Recursion and subroutine calls (\c{(?R)}, \c{(?1)},
    \c{(?&name)}, \c{\g<name>}), backtracking verbs such as \c{(*SKIP)}, callouts, script runs,
    duplicate names (\c{(?J)}) and the \c{(?(R)...)} and \c{(?(VERSION...)...)} conditions are
    not supported, and make the pattern invalid. Error messages and offsets follow PCRE2 for the
    common errors and may differ for rare ones. Case-insensitive matching uses simple Unicode case
    folding and does not apply to \c{\p{...}}; \c{\X} uses a simplified grapheme cluster rule.
    A lone surrogate in a subject is matched as a character of its own instead of making the match
    fail, and DontCheckSubjectStringMatchOption has no effect. There is no JIT: patterns are
    compiled when they are set, so optimize() does nothing and OptimizeOnFirstUsageOption and
    DontAutomaticallyOptimizeOption have no effect. QRegularExpression is not a QVariant or
    QMetaType type. wildcardToRegularExpression() and fromWildcard() also take a QString. Reaching
    the limits described above is reported as a failed match with a warning, as Qt does for the PCRE2
    match limit.
    \enddifferences

    \sa QRegularExpressionMatch, QRegularExpressionMatchIterator
*/
class QRegularExpression {
public:
    /*!
        \enum QRegularExpression::PatternOption

        The options that change how a pattern is interpreted. They are stored in a
        PatternOptions value.

        \value NoPatternOption No options.
        \value CaseInsensitiveOption Letters match in either case.
        \value DotMatchesEverythingOption The dot (\c{.}) also matches newline characters.
        \value MultilineOption \c{^} and \c{$} also match at line breaks inside the subject.
        \value ExtendedPatternSyntaxOption White space in the pattern is ignored, and \c{#}
               starts a comment that runs to the end of the line.
        \value InvertedGreedinessOption Quantifiers are lazy by default and greedy with a
               \c{?} suffix.
        \value DontCaptureOption Unnamed groups do not capture; named groups still do.
        \value UseUnicodePropertiesOption \c{\w \d \s \b} and the POSIX classes use Unicode
               properties instead of ASCII.
        \value OptimizeOnFirstUsageOption Accepted for compatibility; it has no effect.
        \value DontAutomaticallyOptimizeOption Accepted for compatibility; it has no effect.
    */
    enum PatternOption {
        NoPatternOption = 0x0000,
        CaseInsensitiveOption = 0x0001,
        DotMatchesEverythingOption = 0x0002,
        MultilineOption = 0x0004,
        ExtendedPatternSyntaxOption = 0x0008,
        InvertedGreedinessOption = 0x0010,
        DontCaptureOption = 0x0020,
        UseUnicodePropertiesOption = 0x0040,
        OptimizeOnFirstUsageOption = 0x0080,
        DontAutomaticallyOptimizeOption = 0x0100
    };
    /*!
        The PatternOptions type is a typedef for \c{QFlags<PatternOption>}. It stores an OR
        combination of PatternOption values.
    */
    Q_DECLARE_FLAGS(PatternOptions, PatternOption)

    /*!
        \enum QRegularExpression::MatchType

        The kind of matching to do.

        \value NormalMatch A normal match: the subject must contain a complete match.
        \value PartialPreferCompleteMatch Also report a partial match, that is a match that
               reaches the end of the subject and could be completed with more text, but
               only if there is no complete match.
        \value PartialPreferFirstMatch Report a partial match as soon as the end of the
               subject is reached, even if a complete match could be found.
        \value NoMatch No matching is done; the returned match has no result. It can be used
               to check the validity of the expression.
    */
    enum MatchType { NormalMatch, PartialPreferCompleteMatch, PartialPreferFirstMatch, NoMatch };

    /*!
        \enum QRegularExpression::MatchOption

        Options for a single match operation. They are stored in a MatchOptions value.

        \value NoMatchOption No options.
        \value AnchorAtOffsetMatchOption The match must start exactly at the offset.
        \value AnchoredMatchOption The same as AnchorAtOffsetMatchOption, under its Qt 5 name.
        \value DontCheckSubjectStringMatchOption Accepted for compatibility; the engine never
               rejects a subject, so it has no effect.
    */
    enum MatchOption {
        NoMatchOption = 0x0000,
        AnchorAtOffsetMatchOption = 0x0001,
        AnchoredMatchOption = AnchorAtOffsetMatchOption,
        DontCheckSubjectStringMatchOption = 0x0002
    };
    /*!
        The MatchOptions type is a typedef for \c{QFlags<MatchOption>}. It stores an OR
        combination of MatchOption values.
    */
    Q_DECLARE_FLAGS(MatchOptions, MatchOption)

    /*!
        \enum QRegularExpression::WildcardConversionOption

        Options for wildcardToRegularExpression(). They are stored in a
        WildcardConversionOptions value.

        \value DefaultWildcardConversion The result is anchored, and \c{*} and \c{?} do not
               match the path separator \c{/}.
        \value UnanchoredWildcardConversion The result is not anchored with \c{\A} and \c{\z}.
        \value NonPathWildcardConversion \c{*} and \c{?} also match \c{/}.
    */
    enum WildcardConversionOption {
        DefaultWildcardConversion = 0x0,
        UnanchoredWildcardConversion = 0x1,
        NonPathWildcardConversion = 0x2
    };
    /*!
        The WildcardConversionOptions type is a typedef for
        \c{QFlags<WildcardConversionOption>}.
    */
    Q_DECLARE_FLAGS(WildcardConversionOptions, WildcardConversionOption)

    QRegularExpression();
    explicit QRegularExpression(const QString &pattern, PatternOptions options = NoPatternOption);
    QRegularExpression(const QRegularExpression &re);
    QRegularExpression(QRegularExpression &&re) noexcept;
    ~QRegularExpression();
    QRegularExpression &operator=(const QRegularExpression &re);
    QRegularExpression &operator=(QRegularExpression &&re) noexcept;
    void swap(QRegularExpression &other) noexcept;

    PatternOptions patternOptions() const;
    void setPatternOptions(PatternOptions options);
    QString pattern() const;
    void setPattern(const QString &pattern);

    bool isValid() const;
    qsizetype patternErrorOffset() const;
    QString errorString() const;
    int captureCount() const;
    QStringList namedCaptureGroups() const;

    QRegularExpressionMatch match(const QString &subject, qsizetype offset = 0, MatchType matchType = NormalMatch,
                                  MatchOptions matchOptions = NoMatchOption) const;
    QRegularExpressionMatch match(QStringView subjectView, qsizetype offset = 0, MatchType matchType = NormalMatch,
                                  MatchOptions matchOptions = NoMatchOption) const;
    QRegularExpressionMatch matchView(QStringView subjectView, qsizetype offset = 0, MatchType matchType = NormalMatch,
                                      MatchOptions matchOptions = NoMatchOption) const;
    QRegularExpressionMatchIterator globalMatch(const QString &subject, qsizetype offset = 0,
                                                MatchType matchType = NormalMatch,
                                                MatchOptions matchOptions = NoMatchOption) const;
    QRegularExpressionMatchIterator globalMatch(QStringView subjectView, qsizetype offset = 0,
                                                MatchType matchType = NormalMatch,
                                                MatchOptions matchOptions = NoMatchOption) const;
    QRegularExpressionMatchIterator globalMatchView(QStringView subjectView, qsizetype offset = 0,
                                                    MatchType matchType = NormalMatch,
                                                    MatchOptions matchOptions = NoMatchOption) const;

    void optimize() const;

    static QString escape(const QString &str);
    static QString escape(QStringView str);
    static QString wildcardToRegularExpression(QStringView str, WildcardConversionOptions options = DefaultWildcardConversion);
    static QString wildcardToRegularExpression(const QString &str, WildcardConversionOptions options = DefaultWildcardConversion);
    static QString anchoredPattern(const QString &expression);
    static QString anchoredPattern(QStringView expression);
    static QRegularExpression fromWildcard(QStringView pattern, Qt::CaseSensitivity cs = Qt::CaseInsensitive,
                                           WildcardConversionOptions options = DefaultWildcardConversion);
    static QRegularExpression fromWildcard(const QString &pattern, Qt::CaseSensitivity cs = Qt::CaseInsensitive,
                                           WildcardConversionOptions options = DefaultWildcardConversion);

    bool operator==(const QRegularExpression &other) const;
    /*!
        Returns \c true if this regular expression is not equal to \a other; otherwise returns
        \c false.

        \sa operator==()
    */
    bool operator!=(const QRegularExpression &other) const { return !operator==(other); }

private:
    friend class QRegularExpressionMatch;
    friend class QRegularExpressionMatchIterator;
    friend class QRegularExpressionPrivate;

    std::shared_ptr<const QRegularExpressionPrivate> d;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QRegularExpression::PatternOptions)
Q_DECLARE_OPERATORS_FOR_FLAGS(QRegularExpression::MatchOptions)
Q_DECLARE_OPERATORS_FOR_FLAGS(QRegularExpression::WildcardConversionOptions)

std::size_t qHash(const QRegularExpression &key, std::size_t seed = 0) noexcept;
QDataStream &operator<<(QDataStream &out, const QRegularExpression &re);
QDataStream &operator>>(QDataStream &in, QRegularExpression &re);
QDebug operator<<(QDebug debug, const QRegularExpression &re);
QDebug operator<<(QDebug debug, QRegularExpression::PatternOptions patternOptions);

/*!
    \class QRegularExpressionMatch
    \brief The QRegularExpressionMatch class provides the results of a QRegularExpression match.

    A QRegularExpressionMatch is returned by QRegularExpression::match() and by
    QRegularExpressionMatchIterator::next(). It tells whether the subject matched, and gives
    the text and the offsets of the whole match (capture group 0) and of each capturing group.
    The object is cheap to copy; copies share the result.

    \code
    QRegularExpression re("(?<year>\\d{4})-(?<month>\\d\\d)");
    QRegularExpressionMatch m = re.match("on 2024-05-17");
    QString year = m.captured("year");        // "2024"
    qsizetype where = m.capturedStart(0);     // 3
    \endcode

    The match keeps a copy of the subject, except for matches made with
    QRegularExpression::matchView(), which refer to the caller's data.

    \differences
    capturedView() returns a QStringView into the match's own copy of the subject, or into the
    caller's data for matches made with matchView().
    \enddifferences

    \sa QRegularExpression, QRegularExpressionMatchIterator
*/
class QRegularExpressionMatch {
public:
    QRegularExpressionMatch();
    ~QRegularExpressionMatch();
    QRegularExpressionMatch(const QRegularExpressionMatch &match);
    QRegularExpressionMatch(QRegularExpressionMatch &&match) noexcept;
    QRegularExpressionMatch &operator=(const QRegularExpressionMatch &match);
    QRegularExpressionMatch &operator=(QRegularExpressionMatch &&match) noexcept;
    void swap(QRegularExpressionMatch &other) noexcept;

    QRegularExpression regularExpression() const;
    QRegularExpression::MatchType matchType() const;
    QRegularExpression::MatchOptions matchOptions() const;
    bool hasMatch() const;
    bool hasPartialMatch() const;
    bool isValid() const;
    int lastCapturedIndex() const;

    QString captured(int nth = 0) const;
    QString captured(const QString &name) const;
    QString captured(QStringView name) const;
    QStringView capturedView(int nth = 0) const;
    QStringView capturedView(const QString &name) const;
    QStringView capturedView(QStringView name) const;
    QStringList capturedTexts() const;
    qsizetype capturedStart(int nth = 0) const;
    qsizetype capturedStart(const QString &name) const;
    qsizetype capturedStart(QStringView name) const;
    qsizetype capturedLength(int nth = 0) const;
    qsizetype capturedLength(const QString &name) const;
    qsizetype capturedLength(QStringView name) const;
    qsizetype capturedEnd(int nth = 0) const;
    qsizetype capturedEnd(const QString &name) const;
    qsizetype capturedEnd(QStringView name) const;

private:
    friend class QRegularExpression;
    friend class QRegularExpressionPrivate;
    friend class QRegularExpressionMatchIterator;

    explicit QRegularExpressionMatch(std::shared_ptr<const QRegularExpressionMatchPrivate> priv);
    int indexForName(QStringView name) const;

    std::shared_ptr<const QRegularExpressionMatchPrivate> d;
};

QDebug operator<<(QDebug debug, const QRegularExpressionMatch &match);

/*!
    \class QRegularExpressionMatchIterator
    \brief The QRegularExpressionMatchIterator class provides an iterator on the results of a global match of a QRegularExpression.

    A QRegularExpressionMatchIterator is returned by QRegularExpression::globalMatch(). It is a
    Java-style iterator over the successive, non-overlapping matches of a pattern in a subject:
    hasNext() tells whether another match exists, and next() returns it.

    \code
    QRegularExpression re("(\\w+)");
    QRegularExpressionMatchIterator it = re.globalMatch("the quick fox");
    while (it.hasNext())
        qDebug() << it.next().captured(1);   // "the", "quick", "fox"
    \endcode

    After a match of the empty string, the next search starts at the same position and must
    not be empty, and moves on by one character if it finds nothing, so every iteration makes
    progress. After a partial match the iterator is at its end.

    \differences
    The iterator has no begin() or end() for range-based for loops.
    \enddifferences

    \sa QRegularExpression, QRegularExpressionMatch
*/
class QRegularExpressionMatchIterator {
public:
    QRegularExpressionMatchIterator();
    ~QRegularExpressionMatchIterator();
    QRegularExpressionMatchIterator(const QRegularExpressionMatchIterator &iterator);
    QRegularExpressionMatchIterator(QRegularExpressionMatchIterator &&iterator) noexcept;
    QRegularExpressionMatchIterator &operator=(const QRegularExpressionMatchIterator &iterator);
    QRegularExpressionMatchIterator &operator=(QRegularExpressionMatchIterator &&iterator) noexcept;
    void swap(QRegularExpressionMatchIterator &other) noexcept;

    bool isValid() const;
    bool hasNext() const;
    QRegularExpressionMatch next();
    QRegularExpressionMatch peekNext() const;

    QRegularExpression regularExpression() const;
    QRegularExpression::MatchType matchType() const;
    QRegularExpression::MatchOptions matchOptions() const;

private:
    friend class QRegularExpression;
    friend class QRegularExpressionPrivate;

    explicit QRegularExpressionMatchIterator(std::shared_ptr<QRegularExpressionMatchIteratorPrivate> priv);

    std::shared_ptr<QRegularExpressionMatchIteratorPrivate> d;
};
