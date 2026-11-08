// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>

#include <memory>
#include <vector>

/*!
    \class QTextBoundaryFinder
    \since 4.4
    \brief The QTextBoundaryFinder class provides a way of finding Unicode text boundaries in a string.

    A QTextBoundaryFinder finds the positions in a string where a grapheme
    cluster (a user-perceived character), a word or a sentence starts or
    ends, or where a line may be broken. Positions are UTF-16 indexes
    between characters, from 0 to the length of the string.

    \code
    QTextBoundaryFinder finder(QTextBoundaryFinder::Word, text);
    for (qsizetype start = 0, end; (end = finder.toNextBoundary()) != -1; start = end)
        if (finder.boundaryReasons() & QTextBoundaryFinder::EndOfItem)
            words << text.mid(start, end - start);
    \endcode

    \differences
    The rules are simplified versions of Unicode Standard Annex #29 (grapheme
    clusters, words, sentences) and #14 (line breaking), using the general
    category of each character instead of the dedicated break properties.
    There is no dictionary-based word breaking for Thai or similar scripts,
    and ideographs are broken one by one. The optional work buffer of the
    constructor is ignored.
    \enddifferences

    \sa QString
*/
class QTextBoundaryFinder {
public:
    /*!
        \enum QTextBoundaryFinder::BoundaryType

        \value Grapheme Boundaries between user-perceived characters.
        \value Word Boundaries between words and the text between them.
        \value Sentence Boundaries between sentences.
        \value Line Positions where a line may be broken.
    */
    enum BoundaryType { Grapheme, Word, Sentence, Line };

    /*!
        \enum QTextBoundaryFinder::BoundaryReason

        \value NotAtBoundary The position is not a boundary.
        \value BreakOpportunity The position is a boundary.
        \value StartOfItem An item (a grapheme cluster, word or sentence)
               starts at the position.
        \value EndOfItem An item ends at the position.
        \value MandatoryBreak A line must be broken at the position.
        \value SoftHyphen A line may be broken at the position after a soft
               hyphen, which should then be shown as a hyphen.
    */
    enum BoundaryReason { NotAtBoundary = 0, BreakOpportunity = 0x1f, StartOfItem = 0x20, EndOfItem = 0x40, MandatoryBreak = 0x80, SoftHyphen = 0x100 };
    Q_DECLARE_FLAGS(BoundaryReasons, BoundaryReason)

    QTextBoundaryFinder();
    QTextBoundaryFinder(BoundaryType type, const QString &string);
    QTextBoundaryFinder(BoundaryType type, QStringView string, unsigned char *buffer = nullptr, qsizetype bufferSize = 0);
    QTextBoundaryFinder(BoundaryType type, const QChar *chars, qsizetype length, unsigned char *buffer = nullptr, qsizetype bufferSize = 0);
    /*!
        Constructs a copy of \a other.
    */
    QTextBoundaryFinder(const QTextBoundaryFinder &other) = default;
    /*!
        Move-constructs a finder from \a other, which becomes invalid.
    */
    QTextBoundaryFinder(QTextBoundaryFinder &&other) noexcept = default;
    /*!
        Assigns \a other to this finder and returns a reference to it.
    */
    QTextBoundaryFinder &operator=(const QTextBoundaryFinder &other) = default;
    /*!
        Move-assigns \a other to this finder and returns a reference to it.
    */
    QTextBoundaryFinder &operator=(QTextBoundaryFinder &&other) noexcept = default;
    /*!
        Destroys the finder.
    */
    ~QTextBoundaryFinder() = default;

    /*!
        Returns \c true if the finder was constructed with a string; otherwise
        returns \c false.
    */
    bool isValid() const { return m_valid; }
    /*!
        Returns the type of boundaries the finder looks for.
    */
    BoundaryType type() const { return m_type; }
    /*!
        Returns the string the finder works on.
    */
    QString string() const { return m_string; }

    void toStart();
    void toEnd();
    /*!
        Returns the current position, or -1 after moving past the last (or
        before the first) boundary.

        \sa setPosition()
    */
    qsizetype position() const { return m_pos; }
    void setPosition(qsizetype position);
    qsizetype toNextBoundary();
    qsizetype toPreviousBoundary();
    bool isAtBoundary() const;
    BoundaryReasons boundaryReasons() const;

private:
    void compute();
    BoundaryType m_type = Grapheme;
    QString m_string;
    qsizetype m_pos = 0;
    bool m_valid = false;
    std::vector<unsigned short> m_reasons; // one entry per position 0..length
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QTextBoundaryFinder::BoundaryReasons)
