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
namespace Tok {
template <typename H>
using HaystackType = std::conditional_t<std::is_same_v<std::decay_t<H>, QString> && !std::is_lvalue_reference_v<H>, QString, QStringView>;
template <typename N>
using NeedleType = std::conditional_t<
    std::is_same_v<std::decay_t<N>, QChar> || std::is_same_v<std::decay_t<N>, char16_t> || std::is_same_v<std::decay_t<N>, QLatin1Char> ||
        std::is_same_v<std::decay_t<N>, char>,
    QChar,
    std::conditional_t<(std::is_same_v<std::decay_t<N>, QString> && !std::is_lvalue_reference_v<N>) || std::is_same_v<std::decay_t<N>, QLatin1StringView> ||
                           std::is_same_v<std::decay_t<N>, const char *> || std::is_same_v<std::decay_t<N>, char *>,
                       QString, QStringView>>;

inline qsizetype find(QStringView hay, QStringView needle, qsizetype from, Qt::CaseSensitivity cs) {
    const qsizetype n = hay.size(), m = needle.size();
    for (qsizetype i = from; i + m <= n; ++i) {
        qsizetype j = 0;
        if (cs == Qt::CaseSensitive) {
            while (j < m && hay[i + j] == needle[j])
                ++j;
        } else {
            while (j < m && hay[i + j].toCaseFolded() == needle[j].toCaseFolded())
                ++j;
        }
        if (j == m)
            return i;
    }
    return -1;
}
} // namespace Tok
} // namespace QtPrivate

/*!
    \class QStringTokenizer
    \since 6.0
    \brief The QStringTokenizer class splits strings into tokens along given separators.

    A QStringTokenizer splits a string lazily: the tokens are QStringView
    objects into the string, produced one at a time while iterating, so no
    list is built and nothing is copied.

    \code
    for (QStringView part : QStringTokenizer{line, u','})
        handle(part.trimmed());

    QList<QStringView> parts = qTokenize(line, u';', Qt::SkipEmptyParts).toContainer();
    \endcode

    The haystack is viewed when it is an lvalue and moved into the tokenizer
    when it is a temporary QString, so tokens never dangle while the
    tokenizer is alive. The needle may be a QChar, a \c char16_t, a QString,
    a QStringView or a QLatin1StringView. An empty needle splits between
    every character, as QString::split() does.

    \differences
    The tokens are always QStringView objects, also for a QLatin1StringView
    haystack, which must be converted to a QString first. The iterator is a
    forward iterator compared against a sentinel returned by end().
    \enddifferences

    \sa QString::split(), qTokenize()
*/
template <typename Haystack, typename Needle>
class QStringTokenizer {
public:
    /*! \typedef QStringTokenizer::value_type

        The type of a token, QStringView. */
    using value_type = QStringView;
    /*! \typedef QStringTokenizer::size_type

        Alias for qsizetype. */
    using size_type = qsizetype;

    /*!
        \class QStringTokenizer::sentinel
        \brief The sentinel class marks the end of a QStringTokenizer.

        \differences
        None.
        \enddifferences
    */
    struct sentinel {};

    /*!
        \class QStringTokenizer::iterator
        \brief The iterator class iterates over the tokens of a QStringTokenizer.

        \differences
        The iterator is a forward iterator; Qt names it \c const_iterator as well.
        \enddifferences
    */
    class iterator {
    public:
        /*! \typedef QStringTokenizer::iterator::iterator_category

            The iterator is a forward iterator. */
        using iterator_category = std::forward_iterator_tag;
        /*! \typedef QStringTokenizer::iterator::value_type

            QStringView. */
        using value_type = QStringView;
        /*! \typedef QStringTokenizer::iterator::difference_type

            Alias for qsizetype. */
        using difference_type = qsizetype;
        /*! \typedef QStringTokenizer::iterator::pointer

            A pointer to a const QStringView. */
        using pointer = const QStringView *;
        /*! \typedef QStringTokenizer::iterator::reference

            A reference to a const QStringView. */
        using reference = const QStringView &;

        /*!
            Constructs an iterator at the end.
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
            Advances to the next token and returns a reference to this iterator.
        */
        iterator &operator++() {
            advance();
            return *this;
        }
        /*!
            Advances to the next token and returns the iterator as it was before.
        */
        iterator operator++(int) {
            iterator r = *this;
            advance();
            return r;
        }
        friend bool operator==(const iterator &a, const iterator &b) { return a.m_end == b.m_end && (a.m_end || a.m_start == b.m_start); }
        friend bool operator!=(const iterator &a, const iterator &b) { return !(a == b); }
        friend bool operator==(const iterator &a, sentinel) { return a.m_end; }
        friend bool operator!=(const iterator &a, sentinel) { return !a.m_end; }
        friend bool operator==(sentinel, const iterator &a) { return a.m_end; }
        friend bool operator!=(sentinel, const iterator &a) { return !a.m_end; }

    private:
        friend class QStringTokenizer;
        explicit iterator(const QStringTokenizer *t) : m_t(t), m_end(false) { advance(); }
        void advance() {
            const QStringView hay = m_t->haystack();
            const QStringView needle = m_t->needle();
            for (;;) {
                if (m_done) {
                    m_end = true;
                    return;
                }
                const qsizetype idx = m_searchFrom <= hay.size() ? QtPrivate::Tok::find(hay, needle, m_searchFrom, m_t->m_cs) : -1;
                if (idx < 0) {
                    m_token = hay.sliced(m_start);
                    m_done = true;
                } else {
                    m_token = hay.sliced(m_start, idx - m_start);
                    if (needle.isEmpty()) {
                        m_start = idx;
                        m_searchFrom = idx + 1;
                    } else {
                        m_start = idx + needle.size();
                        m_searchFrom = m_start;
                    }
                }
                if (m_t->m_sb == Qt::SkipEmptyParts && m_token.isEmpty())
                    continue;
                return;
            }
        }

        const QStringTokenizer *m_t = nullptr;
        QStringView m_token;
        qsizetype m_start = 0;
        qsizetype m_searchFrom = 0;
        bool m_done = false;
        bool m_end = true;
    };
    /*! \typedef QStringTokenizer::const_iterator

        Same as iterator. */
    using const_iterator = iterator;

    /*!
        Constructs a tokenizer that splits \a haystack at each occurrence of
        \a needle. \a sb selects whether empty tokens are produced and \a cs
        the case sensitivity of the needle.
    */
    QStringTokenizer(Haystack haystack, Needle needle, Qt::SplitBehavior sb = Qt::KeepEmptyParts, Qt::CaseSensitivity cs = Qt::CaseSensitive)
        : m_haystack(std::move(haystack)), m_needle(std::move(needle)), m_sb(sb), m_cs(cs) {}
    /*!
        \overload
        Constructs a tokenizer for \a haystack and \a needle with the case
        sensitivity \a cs and the split behavior \a sb.
    */
    QStringTokenizer(Haystack haystack, Needle needle, Qt::CaseSensitivity cs, Qt::SplitBehavior sb = Qt::KeepEmptyParts)
        : QStringTokenizer(std::move(haystack), std::move(needle), sb, cs) {}

    /*!
        Returns an iterator to the first token.
    */
    iterator begin() const { return iterator(this); }
    /*!
        Same as begin().
    */
    iterator cbegin() const { return begin(); }
    /*!
        Returns the sentinel that marks the end of the tokens.
    */
    sentinel end() const { return {}; }
    /*!
        Same as end().
    */
    sentinel cend() const { return {}; }

    /*!
        Appends all tokens to the container \a c and returns it. The default
        container is a \c{QList<QStringView>}; the views refer into the
        haystack.
    */
    template <typename Container = QList<QStringView>>
    Container toContainer(Container &&c = {}) const {
        for (auto it = begin(); it != end(); ++it)
            c.push_back(*it);
        return std::forward<Container>(c);
    }

private:
    QStringView haystack() const { return QStringView(m_haystack); }
    QStringView needle() const {
        if constexpr (std::is_same_v<Needle, QChar>)
            return QStringView(&m_needle, 1);
        else
            return QStringView(m_needle);
    }

    Haystack m_haystack;
    Needle m_needle;
    Qt::SplitBehavior m_sb;
    Qt::CaseSensitivity m_cs;
};

template <typename H, typename N, typename... Rest>
QStringTokenizer(H &&, N &&, Rest...) -> QStringTokenizer<QtPrivate::Tok::HaystackType<H>, QtPrivate::Tok::NeedleType<N>>;

template <typename H, typename N, typename... Rest>
auto qTokenize(H &&haystack, N &&needle, Rest &&...rest) {
    using Tokenizer = QStringTokenizer<QtPrivate::Tok::HaystackType<H>, QtPrivate::Tok::NeedleType<N>>;
    return Tokenizer(QtPrivate::Tok::HaystackType<H>(std::forward<H>(haystack)), QtPrivate::Tok::NeedleType<N>(std::forward<N>(needle)),
                     std::forward<Rest>(rest)...);
}
