// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>

#include <iterator>
#include <type_traits>
#include <utility>

namespace QtPrivate {

// Finds the first occurrence of the needle in the haystack at or after index from, or returns -1.
qsizetype findString(const QChar *haystack, qsizetype haystackLength, const QChar *needle, qsizetype needleLength, qsizetype from, Qt::CaseSensitivity cs);

// Reads the optional Qt::SplitBehavior and Qt::CaseSensitivity arguments of qTokenize() in any order.
inline void applyTokenizerFlag(Qt::SplitBehavior &sb, Qt::CaseSensitivity &, Qt::SplitBehaviorFlags f) { sb = Qt::SplitBehavior(f); }
inline void applyTokenizerFlag(Qt::SplitBehavior &sb, Qt::CaseSensitivity &, Qt::SplitBehavior f) { sb = f; }
inline void applyTokenizerFlag(Qt::SplitBehavior &, Qt::CaseSensitivity &cs, Qt::CaseSensitivity f) { cs = f; }
template <typename... F>
void parseTokenizerFlags(Qt::SplitBehavior &sb, Qt::CaseSensitivity &cs, F... flags) {
    (applyTokenizerFlag(sb, cs, flags), ...);
}

inline QString needleString(QStringView v) { return v.toString(); }
inline QString needleString(const QString &s) { return s; }
inline QString needleString(QLatin1StringView s) { return QString(s); }
inline QString needleString(const char *s) { return QString(s); }
inline QString needleString(const char16_t *s) { return QString(s); }

} // namespace QtPrivate

/*!
    \class QStringTokenizer
    \brief The QStringTokenizer class splits a string into tokens along given separators, without copying.

    QStringTokenizer is what QStringView::tokenize(), QString::tokenize() and
    qTokenize() return. It is a range of QStringView: each element is one
    token, a view into the string that was tokenized. It does the work of
    QString::split(), but without allocating a list and a string for each
    part.

    \code
    for (QStringView part : QStringView(u"a,b,,c").tokenize(u',')) {
        // "a", "b", "", "c"
    }
    for (auto part : QString("a,b,,c").tokenize(QLatin1StringView(","), Qt::SkipEmptyParts)) {
        // "a", "b", "c"
    }
    \endcode

    The separator can be a QChar, a QStringView, a QString or anything those
    are constructed from. Pass Qt::SkipEmptyParts to leave out empty parts and
    Qt::CaseInsensitive to ignore the case of a string separator. When the
    tokenized text is a QString that was passed as an rvalue, the tokenizer
    keeps it alive; when it is a view or an lvalue, it must outlive the
    tokenizer and the tokens.

    An empty separator yields an empty first token, each single character,
    and an empty last token, as QString::split() does.

    \differences
    The \c Haystack argument is QStringView or QString, and the \c Needle
    argument selects only whether the separator is a single character (QChar)
    or a string; the separator itself is stored by the tokenizer. There is no
    QLatin1StringView haystack and no QRegularExpression separator. end()
    returns an iterator of the same type as begin(), not a sentinel.
    \enddifferences

    \sa QStringView::tokenize(), QString::split()
*/
template <typename Haystack, typename Needle>
class QStringTokenizer {
    static_assert(std::is_same_v<Haystack, QStringView> || std::is_same_v<Haystack, QString>,
                  "QStringTokenizer tokenizes a QStringView or a QString");

public:
    /*!
        The type of the tokens: QStringView.
    */
    using value_type = QStringView;
    /*!
        The type used for sizes, for STL compatibility.
    */
    using size_type = qsizetype;

    /*!
        \class QStringTokenizer::iterator
        \brief The iterator over the tokens of a QStringTokenizer.

        It is a forward iterator whose elements are QStringView tokens.
    */
    class iterator {
    public:
        /*!
            The category of the iterator.
        */
        using iterator_category = std::forward_iterator_tag;
        /*!
            The type of the tokens.
        */
        using value_type = QStringView;
        /*!
            The type of the difference of two iterators.
        */
        using difference_type = qsizetype;
        /*!
            The pointer type.
        */
        using pointer = const QStringView *;
        /*!
            The reference type.
        */
        using reference = const QStringView &;

        /*!
            Constructs an iterator that is at the end of any range.
        */
        iterator() = default;
        /*!
            Returns the current token.
        */
        reference operator*() const { return m_token; }
        /*!
            Returns a pointer to the current token.
        */
        pointer operator->() const { return &m_token; }
        /*!
            Advances the iterator to the next token and returns a reference
            to it.
        */
        iterator &operator++() {
            m_owner->advance(*this);
            return *this;
        }
        /*!
            \overload

            Advances the iterator to the next token and returns the iterator
            as it was before.
        */
        iterator operator++(int) {
            iterator copy = *this;
            ++*this;
            return copy;
        }
        friend bool operator==(const iterator &a, const iterator &b) {
            if (a.m_valid != b.m_valid)
                return false;
            return !a.m_valid || (a.m_owner == b.m_owner && a.m_start == b.m_start && a.m_extra == b.m_extra && a.m_token.data() == b.m_token.data()
                                  && a.m_token.size() == b.m_token.size());
        }
        friend bool operator!=(const iterator &a, const iterator &b) { return !(a == b); }

    private:
        friend class QStringTokenizer;
        const QStringTokenizer *m_owner = nullptr;
        QStringView m_token;
        qsizetype m_start = 0;
        qsizetype m_extra = 0;
        bool m_finished = false;
        bool m_valid = false;
    };
    /*!
        The const iterator type. It is the same as iterator.
    */
    using const_iterator = iterator;

    /*!
        Constructs a tokenizer for \a haystack that splits along \a needle,
        with the split behavior \a sb and the case sensitivity \a cs.
        qTokenize() is the usual way to create one.
    */
    template <typename N>
    QStringTokenizer(Haystack haystack, const N &needle, Qt::SplitBehavior sb = Qt::KeepEmptyParts, Qt::CaseSensitivity cs = Qt::CaseSensitive)
        : m_haystack(std::move(haystack)), m_skipEmpty(sb.testFlag(Qt::SkipEmptyParts)), m_cs(cs) {
        if constexpr (std::is_convertible_v<N, QChar>) {
            m_needle = QString(QChar(needle));
        } else {
            m_needle = QtPrivate::needleString(needle);
        }
    }

    /*!
        Returns an iterator to the first token. There is always at least one
        token, except when empty parts are skipped and there are no other
        parts.

        \sa end()
    */
    iterator begin() const {
        iterator it;
        it.m_owner = this;
        it.m_valid = true;
        advance(it);
        return it;
    }
    /*!
        Returns an iterator that follows the last token.

        \sa begin()
    */
    iterator end() const { return iterator(); }
    /*!
        Returns an iterator to the first token. This is the same as begin().
    */
    iterator cbegin() const { return begin(); }
    /*!
        Returns an iterator that follows the last token. This is the same as
        end().
    */
    iterator cend() const { return end(); }

    /*!
        Returns the tokens in a container of type \a Container, by default a
        QList of QStringView. The container receives the tokens through
        \c push_back() or \c append().
    */
    template <typename Container = QList<QStringView>>
    Container toContainer(Container &&c = Container()) const {
        for (QStringView v : *this) {
            if constexpr (std::is_constructible_v<typename std::decay_t<Container>::value_type, QStringView>)
                c.push_back(typename std::decay_t<Container>::value_type(v));
            else
                c.push_back(v.toString());
        }
        return std::forward<Container>(c);
    }

private:
    QStringView view() const { return QStringView(m_haystack); }

    void advance(iterator &it) const {
        const QStringView h = view();
        for (;;) {
            if (it.m_finished) {
                it = iterator();
                return;
            }
            const qsizetype hit = QtPrivate::findString(h.data(), h.size(), m_needle.constData(), m_needle.size(), it.m_start + it.m_extra, m_cs);
            if (hit < 0) {
                it.m_token = h.sliced(it.m_start);
                it.m_finished = true;
            } else {
                it.m_token = h.sliced(it.m_start, hit - it.m_start);
                const qsizetype sepLen = m_needle.size();
                it.m_start = hit + sepLen;
                it.m_extra = sepLen == 0 ? 1 : 0;
            }
            if (m_skipEmpty && it.m_token.isEmpty())
                continue;
            it.m_valid = true;
            return;
        }
    }

    Haystack m_haystack;
    QString m_needle;
    bool m_skipEmpty = false;
    Qt::CaseSensitivity m_cs = Qt::CaseSensitive;
};

/*!
    \fn template <typename Haystack, typename Needle, typename... Flags> auto qTokenize(Haystack &&haystack, Needle &&needle, Flags... flags)

    Returns a QStringTokenizer that splits \a haystack into tokens along
    \a needle. The optional \a flags are a Qt::SplitBehavior and a
    Qt::CaseSensitivity, in any order. If \a haystack is a QString passed as
    an rvalue, the tokenizer owns it; otherwise it refers to it.

    \sa QStringView::tokenize(), QString::tokenize()
*/
template <typename Haystack, typename Needle, typename... Flags>
auto qTokenize(Haystack &&haystack, Needle &&needle, Flags... flags) {
    Qt::SplitBehavior sb = Qt::KeepEmptyParts;
    Qt::CaseSensitivity cs = Qt::CaseSensitive;
    QtPrivate::parseTokenizerFlags(sb, cs, flags...);
    using Plain = std::remove_cv_t<std::remove_reference_t<Haystack>>;
    using Needle1 = std::remove_cv_t<std::remove_reference_t<Needle>>;
    using NeedleTag = std::conditional_t<std::is_convertible_v<Needle1, QChar> && !std::is_convertible_v<Needle1, QStringView>, QChar, QStringView>;
    if constexpr (std::is_same_v<Plain, QString> && !std::is_lvalue_reference_v<Haystack>) {
        return QStringTokenizer<QString, NeedleTag>(std::move(haystack), needle, sb, cs);
    } else {
        return QStringTokenizer<QStringView, NeedleTag>(QStringView(haystack), needle, sb, cs);
    }
}
