// MIT License - see LICENSE
#include <QtCore/qdatastream.h>
#include <QtCore/qdebug.h>
#include <QtCore/qregularexpression.h>

#include "qregexprogram_p.h"

#include <algorithm>

namespace {
const char16_t kEmptySubject[1] = {0};
} // namespace

class QRegularExpressionPrivate {
public:
    QString pattern;
    QRegularExpression::PatternOptions options;
    std::shared_ptr<const ntregex::Program> program;
    qsizetype errorOffset = -1;
    QString errorString = QStringLiteral("no error");

    static std::shared_ptr<const QRegularExpressionPrivate> create(const QString &pattern,
                                                                   QRegularExpression::PatternOptions options) {
        auto p = std::make_shared<QRegularExpressionPrivate>();
        p->pattern = pattern;
        p->options = options;
        ntregex::CompileError error;
        p->program = ntregex::compilePattern(reinterpret_cast<const char16_t *>(pattern.constData()), pattern.size(),
                                             static_cast<int>(options) & 0x7f, error);
        if (!p->program) {
            p->errorOffset = error.offset;
            p->errorString = QString::fromUtf8(ntregex::errorMessage(error.code));
        }
        return p;
    }

    static std::shared_ptr<const QRegularExpressionPrivate> defaultInstance() {
        static const std::shared_ptr<const QRegularExpressionPrivate> instance =
            create(QString(), QRegularExpression::NoPatternOption);
        return instance;
    }

    static QRegularExpressionMatch doMatch(const QRegularExpression &re, std::shared_ptr<const QString> holder,
                                           const char16_t *data, qsizetype length, qsizetype offset,
                                           QRegularExpression::MatchType type,
                                           QRegularExpression::MatchOptions options, bool previousMatchWasEmpty);
    static QRegularExpressionMatchIterator makeIterator(const QRegularExpression &re,
                                                        std::shared_ptr<const QString> holder, const char16_t *data,
                                                        qsizetype length, qsizetype offset,
                                                        QRegularExpression::MatchType type,
                                                        QRegularExpression::MatchOptions options);
};

class QRegularExpressionMatchPrivate {
public:
    QRegularExpression regex;
    std::shared_ptr<const QString> holder;
    const char16_t *data = kEmptySubject;
    qsizetype length = 0;
    QRegularExpression::MatchType type = QRegularExpression::NoMatch;
    QRegularExpression::MatchOptions options;
    bool valid = false;
    bool hasMatch = false;
    bool hasPartial = false;
    int capturedCount = 0;
    std::vector<qsizetype> offsets;
};

class QRegularExpressionMatchIteratorPrivate {
public:
    QRegularExpression regex;
    std::shared_ptr<const QString> holder;
    const char16_t *data = kEmptySubject;
    qsizetype length = 0;
    QRegularExpression::MatchType type = QRegularExpression::NormalMatch;
    QRegularExpression::MatchOptions options;
    QRegularExpressionMatch next;
    bool hasNext = false;
};

QRegularExpressionMatch QRegularExpressionPrivate::doMatch(const QRegularExpression &re,
                                                           std::shared_ptr<const QString> holder, const char16_t *data,
                                                           qsizetype length, qsizetype offset,
                                                           QRegularExpression::MatchType type,
                                                           QRegularExpression::MatchOptions options,
                                                           bool previousMatchWasEmpty) {
    auto md = std::make_shared<QRegularExpressionMatchPrivate>();
    md->regex = re;
    md->holder = std::move(holder);
    md->data = data ? data : kEmptySubject;
    md->length = length;
    md->type = type;
    md->options = options;
    const ntregex::Program *program = re.d->program.get();
    if (!program) {
        qWarning("QRegularExpressionPrivate::doMatch(): called on an invalid QRegularExpression object");
        return QRegularExpressionMatch(md);
    }
    md->valid = true;
    if (type == QRegularExpression::NoMatch || offset < 0 || offset > length)
        return QRegularExpressionMatch(md);

    const bool userAnchored = options.testFlag(QRegularExpression::AnchorAtOffsetMatchOption);
    ntregex::MatchRequest req;
    req.subject = md->data;
    req.length = length;
    req.offset = offset;
    req.anchored = userAnchored || previousMatchWasEmpty;
    req.notEmptyAtStart = previousMatchWasEmpty;
    req.partial = type == QRegularExpression::PartialPreferCompleteMatch ? 1
                  : type == QRegularExpression::PartialPreferFirstMatch  ? 2
                                                                         : 0;
    ntregex::ExecStatus status = ntregex::execute(*program, req, md->offsets);
    if (status == ntregex::ExecStatus::NoMatch && previousMatchWasEmpty) {
        qsizetype next = offset + 1;
        if (offset + 1 < length && (md->data[offset] & 0xfc00) == 0xd800 && (md->data[offset + 1] & 0xfc00) == 0xdc00)
            ++next;
        if (next <= length) {
            req.offset = next;
            req.anchored = userAnchored;
            req.notEmptyAtStart = false;
            status = ntregex::execute(*program, req, md->offsets);
        }
    }
    switch (status) {
    case ntregex::ExecStatus::Match: {
        md->hasMatch = true;
        int last = 0;
        for (std::size_t g = 0; g * 2 < md->offsets.size(); ++g)
            if (md->offsets[g * 2] >= 0 && md->offsets[g * 2 + 1] >= 0)
                last = static_cast<int>(g);
        md->capturedCount = last + 1;
        break;
    }
    case ntregex::ExecStatus::Partial:
        md->hasPartial = true;
        md->capturedCount = 1;
        break;
    case ntregex::ExecStatus::LimitExceeded:
        qWarning("QRegularExpressionPrivate::doMatch(): the match limit was exceeded");
        md->offsets.clear();
        break;
    case ntregex::ExecStatus::NoMatch:
        md->offsets.clear();
        break;
    }
    return QRegularExpressionMatch(md);
}

QRegularExpressionMatchIterator QRegularExpressionPrivate::makeIterator(
    const QRegularExpression &re, std::shared_ptr<const QString> holder, const char16_t *data, qsizetype length,
    qsizetype offset, QRegularExpression::MatchType type, QRegularExpression::MatchOptions options) {
    auto ip = std::make_shared<QRegularExpressionMatchIteratorPrivate>();
    ip->regex = re;
    ip->holder = holder;
    ip->data = data ? data : kEmptySubject;
    ip->length = length;
    ip->type = type;
    ip->options = options;
    ip->next = doMatch(re, std::move(holder), data, length, offset, type, options, false);
    ip->hasNext = ip->next.hasMatch() || ip->next.hasPartialMatch();
    return QRegularExpressionMatchIterator(ip);
}

// --- QRegularExpression ----------------------------------------------------------------------

/*!
    Constructs an empty regular expression object. It is valid and matches the empty string
    anywhere.

    \sa setPattern()
*/
QRegularExpression::QRegularExpression() : d(QRegularExpressionPrivate::defaultInstance()) {}

/*!
    Constructs a regular expression object for the given \a pattern as a pattern string, and
    sets the \a options. If the pattern is invalid, isValid() returns \c false.

    \sa setPattern(), setPatternOptions()
*/
QRegularExpression::QRegularExpression(const QString &pattern, PatternOptions options)
    : d(QRegularExpressionPrivate::create(pattern, options)) {}

/*!
    Constructs a regular expression object as a copy of \a re. The compiled pattern is shared.

    \sa operator=()
*/
QRegularExpression::QRegularExpression(const QRegularExpression &re) = default;

/*!
    Constructs a regular expression object by moving \a re. The moved-from object may only be
    assigned to or destroyed.
*/
QRegularExpression::QRegularExpression(QRegularExpression &&re) noexcept = default;

/*!
    Destroys the regular expression object.
*/
QRegularExpression::~QRegularExpression() = default;

/*!
    Assigns the regular expression \a re to this object, and returns a reference to the copy.
*/
QRegularExpression &QRegularExpression::operator=(const QRegularExpression &re) = default;

/*!
    Moves the regular expression \a re into this object, and returns a reference to it.
*/
QRegularExpression &QRegularExpression::operator=(QRegularExpression &&re) noexcept = default;

/*!
    Swaps this regular expression with \a other. This operation is very fast and never fails.
*/
void QRegularExpression::swap(QRegularExpression &other) noexcept { d.swap(other.d); }

/*!
    Returns the pattern options of the regular expression.

    \sa setPatternOptions()
*/
QRegularExpression::PatternOptions QRegularExpression::patternOptions() const { return d->options; }

/*!
    Sets the given \a options as the pattern options of the regular expression, and recompiles
    the pattern.

    \sa patternOptions()
*/
void QRegularExpression::setPatternOptions(PatternOptions options) {
    d = QRegularExpressionPrivate::create(d->pattern, options);
}

/*!
    Returns the pattern string of the regular expression.

    \sa setPattern(), patternOptions()
*/
QString QRegularExpression::pattern() const { return d->pattern; }

/*!
    Sets the pattern string of the regular expression to \a pattern, and compiles it. The
    pattern options are left unchanged.

    \sa pattern(), isValid()
*/
void QRegularExpression::setPattern(const QString &pattern) {
    d = QRegularExpressionPrivate::create(pattern, d->options);
}

/*!
    Returns \c true if the regular expression is a valid one; otherwise returns \c false. An
    invalid regular expression never matches.

    \sa errorString(), patternErrorOffset()
*/
bool QRegularExpression::isValid() const { return d->program != nullptr; }

/*!
    Returns the offset, in the pattern string, where the error that made the regular
    expression invalid was detected, or -1 if the expression is valid.

    \sa isValid(), errorString()
*/
qsizetype QRegularExpression::patternErrorOffset() const { return d->errorOffset; }

/*!
    Returns a textual description of the error found when checking the validity of the regular
    expression, or \c{"no error"} if the expression is valid.

    \sa isValid(), patternErrorOffset()
*/
QString QRegularExpression::errorString() const { return d->errorString; }

/*!
    Returns the number of capturing groups inside the pattern string, or -1 if the regular
    expression is not valid.

    \sa isValid(), namedCaptureGroups()
*/
int QRegularExpression::captureCount() const { return d->program ? d->program->captureCount : -1; }

/*!
    Returns a list of captureCount() + 1 elements, containing the names of the named capturing
    groups in the pattern string. The list is sorted by the group number: the element at index
    \c i is the name of group \c i, and is a null string if the group has no name. The element at
    index 0 is always null. The list is empty if the expression is not valid.

    \sa captureCount(), QRegularExpressionMatch::captured()
*/
QStringList QRegularExpression::namedCaptureGroups() const {
    QStringList names;
    if (!d->program)
        return names;
    for (const std::u16string &n : d->program->groupNames)
        names.append(n.empty() ? QString() : QString(n));
    return names;
}

/*!
    Attempts to match the regular expression against the given \a subject string, starting at
    the position \a offset inside the subject, using a match of type \a matchType and honoring
    the given \a matchOptions.

    The returned QRegularExpressionMatch object contains the results of the match. If the
    regular expression is not valid, a warning is printed and the result has no match and
    QRegularExpressionMatch::isValid() is \c false. An \a offset outside the subject gives no
    match.

    \sa QRegularExpressionMatch, globalMatch()
*/
QRegularExpressionMatch QRegularExpression::match(const QString &subject, qsizetype offset, MatchType matchType,
                                                  MatchOptions matchOptions) const {
    auto holder = std::make_shared<const QString>(subject);
    const char16_t *data = reinterpret_cast<const char16_t *>(holder->constData());
    return QRegularExpressionPrivate::doMatch(*this, holder, data, holder->size(), offset, matchType, matchOptions,
                                              false);
}

/*!
    \overload

    Attempts to match the regular expression against the string viewed by \a subjectView.
    The match keeps its own copy of the subject.
*/
QRegularExpressionMatch QRegularExpression::match(QStringView subjectView, qsizetype offset, MatchType matchType,
                                                  MatchOptions matchOptions) const {
    return match(subjectView.toString(), offset, matchType, matchOptions);
}

/*!
    Attempts to match the regular expression against the string viewed by \a subjectView, as
    match() does, but without copying the subject: the returned match, and anything obtained from
    it with capturedView(), refers to the data of \a subjectView, which must stay alive and
    unchanged as long as the match is used.

    \sa match(), QRegularExpressionMatch::capturedView()
*/
QRegularExpressionMatch QRegularExpression::matchView(QStringView subjectView, qsizetype offset, MatchType matchType,
                                                      MatchOptions matchOptions) const {
    return QRegularExpressionPrivate::doMatch(*this, nullptr, reinterpret_cast<const char16_t *>(subjectView.data()),
                                              subjectView.size(), offset, matchType, matchOptions, false);
}

/*!
    Attempts to perform a global match of the regular expression against the given \a subject
    string, starting at the position \a offset, using a match of type \a matchType and honoring
    \a matchOptions. A global match finds all the non-overlapping matches, one after the other.

    The returned QRegularExpressionMatchIterator is positioned before the first match.

    \sa QRegularExpressionMatchIterator, match()
*/
QRegularExpressionMatchIterator QRegularExpression::globalMatch(const QString &subject, qsizetype offset,
                                                                MatchType matchType,
                                                                MatchOptions matchOptions) const {
    auto holder = std::make_shared<const QString>(subject);
    const char16_t *data = reinterpret_cast<const char16_t *>(holder->constData());
    return QRegularExpressionPrivate::makeIterator(*this, holder, data, holder->size(), offset, matchType,
                                                   matchOptions);
}

/*!
    \overload

    Performs a global match against the string viewed by \a subjectView. The iterator keeps
    its own copy of the subject.
*/
QRegularExpressionMatchIterator QRegularExpression::globalMatch(QStringView subjectView, qsizetype offset,
                                                                MatchType matchType,
                                                                MatchOptions matchOptions) const {
    return globalMatch(subjectView.toString(), offset, matchType, matchOptions);
}

/*!
    Performs a global match against \a subjectView without copying the subject. The data viewed
    by \a subjectView must stay alive and unchanged as long as the iterator and the matches
    obtained from it are used.

    \sa globalMatch(), matchView()
*/
QRegularExpressionMatchIterator QRegularExpression::globalMatchView(QStringView subjectView, qsizetype offset,
                                                                    MatchType matchType,
                                                                    MatchOptions matchOptions) const {
    return QRegularExpressionPrivate::makeIterator(*this, nullptr,
                                                   reinterpret_cast<const char16_t *>(subjectView.data()),
                                                   subjectView.size(), offset, matchType, matchOptions);
}

/*!
    Compiles the pattern immediately, if that has not been done. Patterns are compiled when they
    are set, so this function does nothing.
*/
void QRegularExpression::optimize() const {}

/*!
    Returns the string \a str with every character that has a special meaning in a pattern
    escaped with a backslash, so that the result matches \a str literally when used as a
    pattern. All characters other than ASCII letters, digits, the underscore and non-ASCII
    characters are escaped, and a null character becomes \c{\0}.

    \sa anchoredPattern()
*/
QString QRegularExpression::escape(const QString &str) { return escape(QStringView(str)); }

/*!
    \overload

    Escapes the string viewed by \a str.
*/
QString QRegularExpression::escape(QStringView str) {
    QString result;
    for (qsizetype i = 0; i < str.size(); ++i) {
        const QChar c = str[i];
        const char16_t u = c.unicode();
        if (u == 0) {
            result += QStringLiteral("\\0");
            continue;
        }
        const bool plain = (u >= u'a' && u <= u'z') || (u >= u'A' && u <= u'Z') || (u >= u'0' && u <= u'9') ||
                           u == u'_' || u >= 0x80;
        if (!plain)
            result += QChar(u'\\');
        result += c;
    }
    return result;
}

/*!
    Returns a regular expression pattern that matches the wildcard (glob) pattern \a str. The
    wildcards are \c{*} (any number of characters), \c{?} (one character), and \c{[abc]},
    \c{[a-z]} and \c{[!abc]} for sets. A backslash escapes the next character. By default
    \c{*} and \c{?} do not match the path separator \c{/} and the result is anchored with
    \c{\A(?:...)\z}; \a options changes both.

    \sa fromWildcard(), anchoredPattern()
*/
QString QRegularExpression::wildcardToRegularExpression(QStringView str, WildcardConversionOptions options) {
    const bool nonPath = options.testFlag(NonPathWildcardConversion);
    const QString star = nonPath ? QStringLiteral(".*") : QStringLiteral("[^/]*");
    const QString question = nonPath ? QStringLiteral(".") : QStringLiteral("[^/]");
    const qsizetype len = str.size();
    QString rx;
    qsizetype i = 0;
    while (i < len) {
        const QChar c = str[i++];
        switch (c.unicode()) {
        case u'\\':
            if (i == len) {
                rx += QStringLiteral("\\\\");
            } else {
                const QChar next = str[i++];
                const char16_t nu = next.unicode();
                const bool plain = (nu >= u'a' && nu <= u'z') || (nu >= u'A' && nu <= u'Z') ||
                                   (nu >= u'0' && nu <= u'9') || nu == u'_' || nu >= 0x80;
                if (!plain)
                    rx += QChar(u'\\');
                rx += next;
            }
            break;
        case u'*':
            rx += star;
            break;
        case u'?':
            rx += question;
            break;
        case u']':
            rx += QStringLiteral("\\]");
            break;
        case u'[': {
            const qsizetype close = [&]() {
                qsizetype j = i;
                if (j < len && str[j] == u'!')
                    ++j;
                if (j < len && str[j] == u']')
                    ++j;
                while (j < len && str[j] != u']')
                    ++j;
                return j;
            }();
            if (close >= len) {
                rx += QStringLiteral("\\[");
                break;
            }
            rx += QChar(u'[');
            if (str[i] == u'!') {
                rx += QChar(u'^');
                ++i;
            }
            if (i < close && str[i] == u']') {
                rx += QStringLiteral("\\]");
                ++i;
            }
            for (; i < close; ++i) {
                if (str[i] == u'\\' || str[i] == u'[' || str[i] == u'^')
                    rx += QChar(u'\\');
                rx += str[i];
            }
            rx += QChar(u']');
            i = close + 1;
            break;
        }
        default: {
            const char16_t u = c.unicode();
            const bool plain = (u >= u'a' && u <= u'z') || (u >= u'A' && u <= u'Z') || (u >= u'0' && u <= u'9') ||
                               u == u'_' || u >= 0x80;
            if (!plain)
                rx += QChar(u'\\');
            rx += c;
            break;
        }
        }
    }
    return options.testFlag(UnanchoredWildcardConversion) ? rx : anchoredPattern(rx);
}

/*!
    \overload

    Converts the wildcard pattern \a str. Nt accepts a QString here as well as a QStringView, so
    that a string literal can be passed.
*/
QString QRegularExpression::wildcardToRegularExpression(const QString &str, WildcardConversionOptions options) {
    return wildcardToRegularExpression(QStringView(str), options);
}

/*!
    Returns the \a expression wrapped between the \c{\A} and \c{\z} anchors to match it against
    the whole of a subject: \c{\A(?:expression)\z}.

    \sa wildcardToRegularExpression()
*/
QString QRegularExpression::anchoredPattern(const QString &expression) {
    return QStringLiteral("\\A(?:") + expression + QStringLiteral(")\\z");
}

/*!
    \overload

    Anchors the expression viewed by \a expression.
*/
QString QRegularExpression::anchoredPattern(QStringView expression) {
    return anchoredPattern(expression.toString());
}

/*!
    Returns a regular expression that matches the wildcard (glob) pattern \a pattern, built
    with wildcardToRegularExpression() using \a options and made case-insensitive unless
    \a cs is Qt::CaseSensitive.

    \sa wildcardToRegularExpression()
*/
QRegularExpression QRegularExpression::fromWildcard(QStringView pattern, Qt::CaseSensitivity cs,
                                                    WildcardConversionOptions options) {
    return QRegularExpression(wildcardToRegularExpression(pattern, options),
                              cs == Qt::CaseInsensitive ? CaseInsensitiveOption : NoPatternOption);
}

/*!
    \overload

    Builds a regular expression for the wildcard pattern \a pattern.
*/
QRegularExpression QRegularExpression::fromWildcard(const QString &pattern, Qt::CaseSensitivity cs,
                                                    WildcardConversionOptions options) {
    return fromWildcard(QStringView(pattern), cs, options);
}

/*!
    Returns \c true if the regular expression is equal to \a other, that is, it has the same
    pattern string and the same pattern options; otherwise returns \c false.

    \sa operator!=()
*/
bool QRegularExpression::operator==(const QRegularExpression &other) const {
    return d == other.d || (d->pattern == other.d->pattern && d->options == other.d->options);
}

std::size_t qHash(const QRegularExpression &key, std::size_t seed) noexcept {
    return qHash(key.pattern(), seed) ^ static_cast<std::size_t>(static_cast<int>(key.patternOptions()));
}

QDataStream &operator<<(QDataStream &out, const QRegularExpression &re) {
    return out << re.pattern() << static_cast<quint32>(static_cast<int>(re.patternOptions()));
}

QDataStream &operator>>(QDataStream &in, QRegularExpression &re) {
    QString pattern;
    quint32 options = 0;
    in >> pattern >> options;
    if (in.status() == QDataStream::Ok)
        re = QRegularExpression(pattern, QRegularExpression::PatternOptions(
                                             static_cast<QRegularExpression::PatternOption>(options)));
    return in;
}

QDebug operator<<(QDebug debug, QRegularExpression::PatternOptions patternOptions) {
    const bool spaces = debug.autoInsertSpaces();
    std::string flags;
    if (static_cast<int>(patternOptions) == 0) {
        flags = "NoPatternOption";
    } else {
        static const struct {
            QRegularExpression::PatternOption option;
            const char *name;
        } names[] = {{QRegularExpression::CaseInsensitiveOption, "CaseInsensitiveOption"},
                     {QRegularExpression::DotMatchesEverythingOption, "DotMatchesEverythingOption"},
                     {QRegularExpression::MultilineOption, "MultilineOption"},
                     {QRegularExpression::ExtendedPatternSyntaxOption, "ExtendedPatternSyntaxOption"},
                     {QRegularExpression::InvertedGreedinessOption, "InvertedGreedinessOption"},
                     {QRegularExpression::DontCaptureOption, "DontCaptureOption"},
                     {QRegularExpression::UseUnicodePropertiesOption, "UseUnicodePropertiesOption"},
                     {QRegularExpression::OptimizeOnFirstUsageOption, "OptimizeOnFirstUsageOption"},
                     {QRegularExpression::DontAutomaticallyOptimizeOption, "DontAutomaticallyOptimizeOption"}};
        for (const auto &n : names) {
            if (patternOptions.testFlag(n.option)) {
                if (!flags.empty())
                    flags += '|';
                flags += n.name;
            }
        }
    }
    debug.nospace() << "QRegularExpression::PatternOptions(" << flags << ")";
    debug.setAutoInsertSpaces(spaces);
    return debug.maybeSpace();
}

QDebug operator<<(QDebug debug, const QRegularExpression &re) {
    const bool spaces = debug.autoInsertSpaces();
    debug.nospace() << "QRegularExpression(" << re.pattern() << ", " << re.patternOptions() << ")";
    debug.setAutoInsertSpaces(spaces);
    return debug.maybeSpace();
}

// --- QRegularExpressionMatch -----------------------------------------------------------------

namespace {
std::shared_ptr<const QRegularExpressionMatchPrivate> defaultMatchData() {
    static const std::shared_ptr<const QRegularExpressionMatchPrivate> instance =
        std::make_shared<QRegularExpressionMatchPrivate>();
    return instance;
}
} // namespace

/*!
    Constructs an invalid match object: it has no match and isValid() returns \c false.
*/
QRegularExpressionMatch::QRegularExpressionMatch() : d(defaultMatchData()) {}

/*!
    Destroys the match object.
*/
QRegularExpressionMatch::~QRegularExpressionMatch() = default;

/*!
    Constructs a match object as a copy of \a match. The result is shared.
*/
QRegularExpressionMatch::QRegularExpressionMatch(const QRegularExpressionMatch &match) = default;

/*!
    Constructs a match object by moving \a match.
*/
QRegularExpressionMatch::QRegularExpressionMatch(QRegularExpressionMatch &&match) noexcept = default;

/*!
    Assigns the match \a match to this object, and returns a reference to the copy.
*/
QRegularExpressionMatch &QRegularExpressionMatch::operator=(const QRegularExpressionMatch &match) = default;

/*!
    Moves the match \a match into this object, and returns a reference to it.
*/
QRegularExpressionMatch &QRegularExpressionMatch::operator=(QRegularExpressionMatch &&match) noexcept = default;

QRegularExpressionMatch::QRegularExpressionMatch(std::shared_ptr<const QRegularExpressionMatchPrivate> priv)
    : d(std::move(priv)) {}

/*!
    Swaps this match with \a other. This operation is very fast and never fails.
*/
void QRegularExpressionMatch::swap(QRegularExpressionMatch &other) noexcept { d.swap(other.d); }

/*!
    Returns the regular expression that produced this match.

    \sa matchType(), matchOptions()
*/
QRegularExpression QRegularExpressionMatch::regularExpression() const { return d->regex; }

/*!
    Returns the match type that was used to get this match.

    \sa regularExpression(), matchOptions()
*/
QRegularExpression::MatchType QRegularExpressionMatch::matchType() const { return d->type; }

/*!
    Returns the match options that were used to get this match.

    \sa regularExpression(), matchType()
*/
QRegularExpression::MatchOptions QRegularExpressionMatch::matchOptions() const { return d->options; }

/*!
    Returns \c true if the regular expression matched against the subject string, or \c false
    otherwise. A partial match does not count.

    \sa hasPartialMatch()
*/
bool QRegularExpressionMatch::hasMatch() const { return d->hasMatch; }

/*!
    Returns \c true if the regular expression partially matched against the subject string, or
    \c false otherwise. A partial match is one that reaches the end of the subject and could
    become a complete match if the subject were longer. The text and offsets of the partial
    match are available as capture group 0.

    \sa hasMatch(), QRegularExpression::MatchType
*/
bool QRegularExpressionMatch::hasPartialMatch() const { return d->hasPartial; }

/*!
    Returns \c true if the match object was obtained as a result from the
    QRegularExpression::match() function invoked on a valid QRegularExpression object; returns
    \c false if the regular expression was invalid, or if this match object was default
    constructed.

    \sa QRegularExpression::isValid()
*/
bool QRegularExpressionMatch::isValid() const { return d->valid; }

/*!
    Returns the index of the last capture group that captured something, or -1 if there is no
    match. A group that did not take part in the match, such as an optional one, counts
    only if a later group did.

    \sa captured()
*/
int QRegularExpressionMatch::lastCapturedIndex() const { return d->capturedCount - 1; }

/*!
    Returns the text captured by the \a nth capturing group, or a null string if the group did
    not capture anything or does not exist. Capture group 0 is the whole match.

    \sa capturedView(), capturedStart(), capturedEnd(), capturedLength()
*/
QString QRegularExpressionMatch::captured(int nth) const {
    const qsizetype start = capturedStart(nth);
    if (start < 0)
        return QString();
    return QString(reinterpret_cast<const QChar *>(d->data) + start, capturedLength(nth));
}

int QRegularExpressionMatch::indexForName(QStringView name) const {
    const auto &names = d->regex.d->program ? d->regex.d->program->groupNames : std::vector<std::u16string>();
    const std::u16string wanted(reinterpret_cast<const char16_t *>(name.data()), static_cast<std::size_t>(name.size()));
    for (std::size_t i = 1; i < names.size(); ++i)
        if (names[i] == wanted)
            return static_cast<int>(i);
    return -1;
}

namespace {
// Returns the group number for a name, or -1 after printing the warning that Qt prints.
int namedGroup(const QRegularExpressionMatch &m, QStringView name, int index, const char *function) {
    if (name.isEmpty()) {
        qWarning("QRegularExpressionMatch::%s: empty capturing group name passed", function);
        return -1;
    }
    if (index < 0)
        qWarning("QRegularExpressionMatch::%s: no capturing group named '%s'", function,
                 name.toString().toUtf8().constData());
    (void)m;
    return index;
}
} // namespace

/*!
    \overload

    Returns the text captured by the capturing group named \a name, or a null string if there
    is no such group or it did not capture anything. A warning is printed for an unknown name.
*/
QString QRegularExpressionMatch::captured(const QString &name) const { return captured(QStringView(name)); }

/*!
    \overload

    Returns the text captured by the capturing group named \a name.
*/
QString QRegularExpressionMatch::captured(QStringView name) const {
    const int nth = namedGroup(*this, name, name.isEmpty() ? -1 : indexForName(name), "captured");
    return nth < 0 ? QString() : captured(nth);
}

/*!
    Returns a view of the text captured by the \a nth capturing group, or a null view if the
    group did not capture anything or does not exist. The view refers to the subject held by the
    match, and is valid as long as this match object, or one copied from it, lives.

    \sa captured()
*/
QStringView QRegularExpressionMatch::capturedView(int nth) const {
    const qsizetype start = capturedStart(nth);
    if (start < 0)
        return QStringView();
    return QStringView(reinterpret_cast<const QChar *>(d->data) + start, capturedLength(nth));
}

/*!
    \overload

    Returns a view of the text captured by the capturing group named \a name.
*/
QStringView QRegularExpressionMatch::capturedView(const QString &name) const {
    return capturedView(QStringView(name));
}

/*!
    \overload

    Returns a view of the text captured by the capturing group named \a name.
*/
QStringView QRegularExpressionMatch::capturedView(QStringView name) const {
    const int nth = namedGroup(*this, name, name.isEmpty() ? -1 : indexForName(name), "capturedView");
    return nth < 0 ? QStringView() : capturedView(nth);
}

/*!
    Returns a list of the captured texts: element 0 is the whole match, element 1 the text of the
    first capturing group, and so on, up to lastCapturedIndex(). Groups that captured nothing
    give null strings. The list is empty if there is no match.

    \sa captured(), lastCapturedIndex()
*/
QStringList QRegularExpressionMatch::capturedTexts() const {
    QStringList list;
    for (int i = 0; i <= lastCapturedIndex(); ++i)
        list.append(captured(i));
    return list;
}

/*!
    Returns the offset inside the subject string of the start of the text captured by the
    \a nth capturing group, or -1 if the group did not capture anything or does not exist.

    \sa capturedEnd(), capturedLength(), captured()
*/
qsizetype QRegularExpressionMatch::capturedStart(int nth) const {
    if (nth < 0 || nth > lastCapturedIndex())
        return -1;
    return d->offsets[static_cast<std::size_t>(nth) * 2];
}

/*!
    \overload

    Returns the offset of the start of the text captured by the group named \a name.
*/
qsizetype QRegularExpressionMatch::capturedStart(const QString &name) const { return capturedStart(QStringView(name)); }

/*!
    \overload

    Returns the offset of the start of the text captured by the group named \a name.
*/
qsizetype QRegularExpressionMatch::capturedStart(QStringView name) const {
    const int nth = namedGroup(*this, name, name.isEmpty() ? -1 : indexForName(name), "capturedStart");
    return nth < 0 ? -1 : capturedStart(nth);
}

/*!
    Returns the length of the text captured by the \a nth capturing group, or 0 if the group did
    not capture anything or does not exist.

    \sa capturedStart(), capturedEnd()
*/
qsizetype QRegularExpressionMatch::capturedLength(int nth) const { return capturedEnd(nth) - capturedStart(nth); }

/*!
    \overload

    Returns the length of the text captured by the group named \a name.
*/
qsizetype QRegularExpressionMatch::capturedLength(const QString &name) const {
    return capturedLength(QStringView(name));
}

/*!
    \overload

    Returns the length of the text captured by the group named \a name.
*/
qsizetype QRegularExpressionMatch::capturedLength(QStringView name) const {
    const int nth = namedGroup(*this, name, name.isEmpty() ? -1 : indexForName(name), "capturedLength");
    return nth < 0 ? 0 : capturedLength(nth);
}

/*!
    Returns the offset inside the subject string of the character just after the end of the text
    captured by the \a nth capturing group, or -1 if the group did not capture anything or does
    not exist.

    \sa capturedStart(), capturedLength()
*/
qsizetype QRegularExpressionMatch::capturedEnd(int nth) const {
    if (nth < 0 || nth > lastCapturedIndex())
        return -1;
    return d->offsets[static_cast<std::size_t>(nth) * 2 + 1];
}

/*!
    \overload

    Returns the offset just after the end of the text captured by the group named \a name.
*/
qsizetype QRegularExpressionMatch::capturedEnd(const QString &name) const { return capturedEnd(QStringView(name)); }

/*!
    \overload

    Returns the offset just after the end of the text captured by the group named \a name.
*/
qsizetype QRegularExpressionMatch::capturedEnd(QStringView name) const {
    const int nth = namedGroup(*this, name, name.isEmpty() ? -1 : indexForName(name), "capturedEnd");
    return nth < 0 ? -1 : capturedEnd(nth);
}

QDebug operator<<(QDebug debug, const QRegularExpressionMatch &match) {
    const bool spaces = debug.autoInsertSpaces();
    debug.nospace() << "QRegularExpressionMatch(";
    if (!match.isValid()) {
        debug << "Invalid)";
    } else {
        debug << "Valid";
        if (match.hasMatch()) {
            debug << ", has match: ";
            for (int i = 0; i <= match.lastCapturedIndex(); ++i) {
                debug << i << ":(" << static_cast<qint64>(match.capturedStart(i)) << ", "
                      << static_cast<qint64>(match.capturedEnd(i)) << ", " << match.captured(i) << ")";
                if (i < match.lastCapturedIndex())
                    debug << ", ";
            }
        } else if (match.hasPartialMatch()) {
            debug << ", has partial match: (" << static_cast<qint64>(match.capturedStart(0)) << ", "
                  << static_cast<qint64>(match.capturedEnd(0)) << ", " << match.captured(0) << ")";
        } else {
            debug << ", no match";
        }
        debug << ")";
    }
    debug.setAutoInsertSpaces(spaces);
    return debug.maybeSpace();
}

// --- QRegularExpressionMatchIterator ---------------------------------------------------------

namespace {
std::shared_ptr<QRegularExpressionMatchIteratorPrivate> defaultIteratorData() {
    return std::make_shared<QRegularExpressionMatchIteratorPrivate>();
}
} // namespace

/*!
    Constructs an empty, invalid iterator: hasNext() returns \c false.
*/
QRegularExpressionMatchIterator::QRegularExpressionMatchIterator() : d(defaultIteratorData()) {}

/*!
    Destroys the iterator.
*/
QRegularExpressionMatchIterator::~QRegularExpressionMatchIterator() = default;

/*!
    Constructs an iterator as a copy of \a iterator. The copy continues from the same position
    but the two then advance independently.
*/
QRegularExpressionMatchIterator::QRegularExpressionMatchIterator(const QRegularExpressionMatchIterator &iterator)
    : d(iterator.d ? std::make_shared<QRegularExpressionMatchIteratorPrivate>(*iterator.d) : nullptr) {}

/*!
    Constructs an iterator by moving \a iterator.
*/
QRegularExpressionMatchIterator::QRegularExpressionMatchIterator(QRegularExpressionMatchIterator &&iterator) noexcept =
    default;

/*!
    Assigns the iterator \a iterator to this object, and returns a reference to the copy.
*/
QRegularExpressionMatchIterator &
QRegularExpressionMatchIterator::operator=(const QRegularExpressionMatchIterator &iterator) {
    if (this != &iterator)
        d = iterator.d ? std::make_shared<QRegularExpressionMatchIteratorPrivate>(*iterator.d) : nullptr;
    return *this;
}

/*!
    Moves the iterator \a iterator into this object, and returns a reference to it.
*/
QRegularExpressionMatchIterator &
QRegularExpressionMatchIterator::operator=(QRegularExpressionMatchIterator &&iterator) noexcept = default;

QRegularExpressionMatchIterator::QRegularExpressionMatchIterator(
    std::shared_ptr<QRegularExpressionMatchIteratorPrivate> priv)
    : d(std::move(priv)) {}

/*!
    Swaps this iterator with \a other. This operation is very fast and never fails.
*/
void QRegularExpressionMatchIterator::swap(QRegularExpressionMatchIterator &other) noexcept { d.swap(other.d); }

/*!
    Returns \c true if the regular expression of the iterator is valid; otherwise returns
    \c false.

    \sa QRegularExpression::isValid()
*/
bool QRegularExpressionMatchIterator::isValid() const { return d->next.isValid(); }

/*!
    Returns \c true if there is at least one match result ahead of the iterator; otherwise
    returns \c false.

    \sa next()
*/
bool QRegularExpressionMatchIterator::hasNext() const { return d->hasNext; }

/*!
    Returns the next match result and advances the iterator by one position. Calling it when
    hasNext() is \c false prints a warning and returns the last match.

    \sa hasNext(), peekNext()
*/
QRegularExpressionMatch QRegularExpressionMatchIterator::next() {
    if (!d->hasNext) {
        qWarning("QRegularExpressionMatchIterator::next() called on an iterator already at end");
        return d->next;
    }
    QRegularExpressionMatch current = d->next;
    if (current.hasMatch()) {
        const bool empty = current.capturedLength(0) == 0;
        d->next = QRegularExpressionPrivate::doMatch(d->regex, d->holder, d->data, d->length, current.capturedEnd(0),
                                                     d->type, d->options, empty);
    } else {
        auto ended = std::make_shared<QRegularExpressionMatchPrivate>(*current.d);
        ended->hasPartial = false;
        ended->capturedCount = 0;
        ended->offsets.clear();
        d->next = QRegularExpressionMatch(ended);
    }
    d->hasNext = d->next.hasMatch() || d->next.hasPartialMatch();
    return current;
}

/*!
    Returns the next match result without moving the iterator. Calling it when hasNext() is
    \c false prints a warning and returns the last match.

    \sa hasNext(), next()
*/
QRegularExpressionMatch QRegularExpressionMatchIterator::peekNext() const {
    if (!d->hasNext)
        qWarning("QRegularExpressionMatchIterator::peekNext() called on an iterator already at end");
    return d->next;
}

/*!
    Returns the regular expression of the iterator.

    \sa matchType(), matchOptions()
*/
QRegularExpression QRegularExpressionMatchIterator::regularExpression() const { return d->regex; }

/*!
    Returns the match type of the iterator.

    \sa regularExpression(), matchOptions()
*/
QRegularExpression::MatchType QRegularExpressionMatchIterator::matchType() const { return d->type; }

/*!
    Returns the match options of the iterator.

    \sa regularExpression(), matchType()
*/
QRegularExpression::MatchOptions QRegularExpressionMatchIterator::matchOptions() const { return d->options; }

// --- QString integration ---------------------------------------------------------------------

/*!
    \overload

    Returns the index position of the first match of the regular expression \a re in the string,
    searching forward from index position \a from, or -1 if there is no match. A negative \a from
    counts from the end of the string. If \a rmatch is not \c nullptr, it receives the match
    object for the first match found.

    \sa lastIndexOf(), contains(), QRegularExpression::match()
*/
qsizetype QString::indexOf(const QRegularExpression &re, qsizetype from, QRegularExpressionMatch *rmatch) const {
    if (!re.isValid()) {
        qWarning("QString::indexOf: invalid QRegularExpression object");
        return -1;
    }
    if (from < 0)
        from = std::max<qsizetype>(from + size(), 0);
    QRegularExpressionMatch match = re.match(*this, from);
    if (!match.hasMatch())
        return -1;
    const qsizetype result = match.capturedStart();
    if (rmatch)
        *rmatch = std::move(match);
    return result;
}

/*!
    \overload

    Returns the index position of the start of the last match of the regular expression \a re
    that starts at or before index position \a from, or -1 if there is none. A negative \a from
    counts from the end of the string. The matches considered are those found by a global
    match, so they do not overlap. If \a rmatch is not \c nullptr, it receives the match
    object.

    \sa indexOf(), contains()
*/
qsizetype QString::lastIndexOf(const QRegularExpression &re, qsizetype from, QRegularExpressionMatch *rmatch) const {
    if (!re.isValid()) {
        qWarning("QString::lastIndexOf: invalid QRegularExpression object");
        return -1;
    }
    const qsizetype endpos = from < 0 ? size() + from + 1 : from + 1;
    QRegularExpressionMatchIterator it = re.globalMatch(*this);
    qsizetype last = -1;
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        const qsizetype start = match.capturedStart();
        if (start >= endpos)
            break;
        last = start;
        if (rmatch)
            *rmatch = std::move(match);
    }
    return last;
}

/*!
    \overload

    Returns the index position of the start of the last match of the regular expression \a re in
    the string, or -1 if there is none. If \a rmatch is not \c nullptr, it receives the match
    object.
*/
qsizetype QString::lastIndexOf(const QRegularExpression &re, QRegularExpressionMatch *rmatch) const {
    return lastIndexOf(re, -1, rmatch);
}

/*!
    \overload

    Returns \c true if the regular expression \a re matches somewhere in the string; otherwise
    returns \c false. If \a rmatch is not \c nullptr, it receives the match object.

    \sa indexOf(), QRegularExpression::match()
*/
bool QString::contains(const QRegularExpression &re, QRegularExpressionMatch *rmatch) const {
    if (!re.isValid()) {
        qWarning("QString::contains: invalid QRegularExpression object");
        return false;
    }
    QRegularExpressionMatch match = re.match(*this);
    const bool found = match.hasMatch();
    if (found && rmatch)
        *rmatch = std::move(match);
    return found;
}

/*!
    \overload

    Returns the number of times the regular expression \a re matches in the string. The matches
    do not overlap, and an empty match counts too, so \c{count(QRegularExpression(""))} is
    size() + 1.

    \sa contains(), indexOf()
*/
qsizetype QString::count(const QRegularExpression &re) const {
    if (!re.isValid()) {
        qWarning("QString::count: invalid QRegularExpression object");
        return 0;
    }
    qsizetype n = 0;
    QRegularExpressionMatchIterator it = re.globalMatch(*this);
    while (it.hasNext()) {
        it.next();
        ++n;
    }
    return n;
}

/*!
    \overload

    Replaces every match of the regular expression \a re in the string with \a after, and
    returns a reference to the string. In \a after, \c{\1} to \c{\99} stand for the text captured
    by the corresponding group and \c{\0} for the whole match; a backslash that is not followed by
    a valid group number is copied as it is.

    \sa remove(), QRegularExpression::globalMatch()
*/
QString &QString::replace(const QRegularExpression &re, const QString &after) {
    if (!re.isValid()) {
        qWarning("QString::replace: invalid QRegularExpression object");
        return *this;
    }
    struct Piece {
        QString text;
        int group;
    };
    std::vector<Piece> pieces;
    const int captures = re.captureCount();
    QString literal;
    for (qsizetype i = 0; i < after.size(); ++i) {
        const QChar c = after[i];
        if (c == u'\\' && i + 1 < after.size() && after[i + 1].isDigit()) {
            int group = after[i + 1].unicode() - u'0';
            qsizetype used = 2;
            if (i + 2 < after.size() && after[i + 2].isDigit()) {
                const int two = group * 10 + (after[i + 2].unicode() - u'0');
                if (two <= captures) {
                    group = two;
                    used = 3;
                }
            }
            if (group <= captures) {
                if (!literal.isEmpty()) {
                    pieces.push_back({literal, -1});
                    literal.clear();
                }
                pieces.push_back({QString(), group});
                i += used - 1;
                continue;
            }
        }
        literal += c;
    }
    if (!literal.isEmpty())
        pieces.push_back({literal, -1});

    QRegularExpressionMatchIterator it = re.globalMatch(*this);
    if (!it.hasNext())
        return *this;
    QString result;
    qsizetype last = 0;
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const qsizetype start = std::max(m.capturedStart(), last);
        result += QString(constData() + last, start - last);
        for (const Piece &p : pieces)
            result += p.group < 0 ? p.text : m.captured(p.group);
        last = std::max(m.capturedEnd(), start);
    }
    result += QString(constData() + last, size() - last);
    *this = result;
    return *this;
}

/*!
    \overload

    Removes every match of the regular expression \a re from the string, and returns a reference
    to the string.

    \sa replace()
*/
QString &QString::remove(const QRegularExpression &re) { return replace(re, QString()); }

/*!
    \overload

    Splits the string into substrings wherever the regular expression \a sep matches, and
    returns the list of those strings. If \a sep does not match anywhere, the list holds the
    whole string. \a behavior says whether empty parts are kept. An empty match splits between
    every two characters, with an empty part at each end when empty parts are kept.

    \sa QStringList::join(), section()
*/
QStringList QString::split(const QRegularExpression &sep, Qt::SplitBehavior behavior) const {
    QStringList list;
    if (!sep.isValid()) {
        qWarning("QString::split: invalid QRegularExpression object");
        return list;
    }
    qsizetype start = 0;
    QRegularExpressionMatchIterator it = sep.globalMatch(*this);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const qsizetype end = std::max(m.capturedStart(), start);
        if (start != end || !behavior.testFlag(Qt::SkipEmptyParts))
            list.append(QString(constData() + start, end - start));
        start = m.capturedEnd();
    }
    if (start != size() || !behavior.testFlag(Qt::SkipEmptyParts))
        list.append(QString(constData() + start, size() - start));
    return list;
}

/*!
    \overload

    Returns a section of the string, using the matches of the regular expression \a re as the
    separators. The fields are numbered from 0; \a start and \a end are the first and last field
    of the section, and a negative number counts from the last field. \a flags controls how empty
    fields and separators are treated; SectionCaseInsensitiveSeps makes \a re case-insensitive.

    \sa split()
*/
QString QString::section(const QRegularExpression &re, qsizetype start, qsizetype end, SectionFlags flags) const {
    if (!re.isValid()) {
        qWarning("QString::section: invalid QRegularExpression object");
        return QString();
    }
    QRegularExpression sep(re);
    if (flags.testFlag(SectionCaseInsensitiveSeps))
        sep.setPatternOptions(sep.patternOptions() | QRegularExpression::CaseInsensitiveOption);
    // Each chunk is a separator followed by the field after it; the first has no separator.
    struct Chunk {
        qsizetype sepLength;
        QString text;
    };
    std::vector<Chunk> chunks;
    qsizetype lastStart = 0;
    qsizetype lastSep = 0;
    QRegularExpressionMatchIterator it = sep.globalMatch(*this);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const qsizetype at = std::max(m.capturedStart(), lastStart);
        chunks.push_back({lastSep, QString(constData() + lastStart, at - lastStart)});
        lastStart = at;
        lastSep = m.capturedLength();
    }
    chunks.push_back({lastSep, QString(constData() + lastStart, size() - lastStart)});

    const qsizetype count = static_cast<qsizetype>(chunks.size());
    const bool skipEmpty = flags.testFlag(SectionSkipEmpty);
    qsizetype counted = count;
    if (skipEmpty) {
        for (const Chunk &c : chunks)
            if (c.sepLength == c.text.size())
                --counted;
    }
    if (start < 0)
        start += counted;
    if (end < 0)
        end += counted;
    if (start >= count || end < 0 || start > end)
        return QString();

    QString result;
    qsizetype firstIndex = -1;
    qsizetype lastIndex = -1;
    qsizetype x = 0;
    for (qsizetype i = 0; x <= end && i < count; ++i) {
        const Chunk &c = chunks[static_cast<std::size_t>(i)];
        const bool empty = c.sepLength == c.text.size();
        if (x >= start) {
            if (firstIndex < 0)
                firstIndex = i;
            lastIndex = i;
            if (x > start || i == 0)
                result += c.text;
            else
                result += c.text.mid(c.sepLength);
        }
        if (!empty || !skipEmpty)
            ++x;
    }
    if (flags.testFlag(SectionIncludeLeadingSep) && firstIndex >= 0) {
        const Chunk &c = chunks[static_cast<std::size_t>(firstIndex)];
        result.prepend(c.text.left(c.sepLength));
    }
    if (flags.testFlag(SectionIncludeTrailingSep) && lastIndex >= 0 && lastIndex + 1 < count) {
        const Chunk &c = chunks[static_cast<std::size_t>(lastIndex + 1)];
        result += c.text.left(c.sepLength);
    }
    return result;
}
