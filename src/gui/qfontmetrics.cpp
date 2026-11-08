// MIT License - see LICENSE
#include <QtGui/qfontmetrics.h>

#include "qfontengine_p.h"
#include "qtextlayout_p.h"

#include <algorithm>

/*!
    \class QFontMetrics
    \brief The QFontMetrics class provides font metrics information.

    Font metrics measure text in the pixels of a font: the height of a line
    and the width of a string. Use them to lay text out:

    \code
    QFontMetrics fm(QFont("Noto Sans", 12));
    int width = fm.horizontalAdvance("Hello");
    QString shorter = fm.elidedText("A very long file name.txt", Qt::ElideMiddle, 120);
    \endcode

    \differences
    There is no QFontMetricsF, \c tightBoundingRect(), \c leftBearing(),
    \c maxWidth(), \c underlinePos() or \c capHeight(). Text is measured as a
    run of characters without shaping or kerning, so complex scripts such as Arabic
    are measured wrongly. The metrics are those of the font at 96 dots per inch, not of
    a paint device.
    \enddifferences

    \sa QFont, QPainter::drawText()
*/

/*!
    Constructs font metrics for \a font.
*/
QFontMetrics::QFontMetrics(const QFont &font) : m_engine(QFontEngine::create(font)) {}

/*!
    Constructs a copy of \a other.
*/
QFontMetrics::QFontMetrics(const QFontMetrics &other) = default;

/*!
    Assigns \a other to these metrics.
*/
QFontMetrics &QFontMetrics::operator=(const QFontMetrics &other) = default;

/*!
    Destroys the metrics.
*/
QFontMetrics::~QFontMetrics() = default;

/*!
    Returns the distance from the baseline to the top of the tallest
    character, in pixels.

    \sa descent(), height()
*/
int QFontMetrics::ascent() const { return m_engine->ascent(); }

/*!
    Returns the distance from the baseline to the bottom of the lowest
    character, in pixels.

    \sa ascent(), height()
*/
int QFontMetrics::descent() const { return m_engine->descent(); }

/*!
    Returns the height of a line of text: the ascent plus the descent.

    \sa lineSpacing()
*/
int QFontMetrics::height() const { return m_engine->ascent() + m_engine->descent(); }

/*!
    Returns the gap between the descent of one line and the ascent of the next.

    \sa lineSpacing()
*/
int QFontMetrics::leading() const { return m_engine->leading(); }

/*!
    Returns the distance between the baselines of two lines of text: the height plus the leading.

    \sa height(), leading()
*/
int QFontMetrics::lineSpacing() const { return height() + leading(); }

/*!
    Returns the height of the lower case letter x, in pixels.
*/
int QFontMetrics::xHeight() const { return m_engine->xHeight(); }

/*!
    Returns the width of a digit, which is a fair guess at the average width of a character.
*/
int QFontMetrics::averageCharWidth() const { return m_engine->averageCharWidth(); }

/*!
    Returns \c true if the font has a glyph for \a ch.
*/
bool QFontMetrics::inFont(QChar ch) const { return m_engine->hasGlyph(ch.unicode()); }

/*!
    Returns the width in pixels of the first \a length characters of \a text, or of all of it
    if \a length is negative, drawn on one line.

    \sa boundingRect()
*/
int QFontMetrics::horizontalAdvance(const QString &text, int length) const {
    return QtPrivate::textWidth(*m_engine, length < 0 ? text : text.left(length));
}

/*!
    \overload

    Returns the width in pixels of the character \a ch.
*/
int QFontMetrics::horizontalAdvance(QChar ch) const { return QtPrivate::textWidth(*m_engine, QString(ch)); }

/*!
    Returns the rectangle that \a text covers when it is drawn at the origin with
    QPainter::drawText(QPoint, QString): its top is at minus the ascent.

    \sa size()
*/
QRect QFontMetrics::boundingRect(const QString &text) const {
    const QtPrivate::TextBlock block = QtPrivate::layoutAtPoint(*m_engine, QPoint(0, 0), text);
    return block.bounds;
}

/*!
    \overload

    Returns the rectangle that \a text covers when it is drawn with
    QPainter::drawText() in \a rect with the given \a flags, which are
    Qt::AlignmentFlag and Qt::TextFlag values.
*/
QRect QFontMetrics::boundingRect(const QRect &rect, int flags, const QString &text) const {
    return QtPrivate::layoutText(*m_engine, rect, flags, text).bounds;
}

/*!
    \fn QRect QFontMetrics::boundingRect(int x, int y, int w, int h, int flags, const QString &text) const
    \overload

    Returns the rectangle that \a text covers when it is drawn in the rectangle at (\a x, \a y) of
    \a w by \a h with the given \a flags.
*/

/*!
    Returns the size of \a text when it is drawn with the given \a flags. With Qt::TextWordWrap the
    text has no width limit, so only new lines break it.

    \sa boundingRect()
*/
QSize QFontMetrics::size(int flags, const QString &text) const {
    return QtPrivate::layoutText(*m_engine, QRect(0, 0, 0, 0), flags, text).bounds.size();
}

/*!
    Returns \a text shortened to fit in \a width pixels, with an ellipsis where \a mode says. Text that
    fits, or Qt::ElideNone, is returned as it is. The \a flags are accepted for compatibility; only
    the first line is considered with Qt::TextSingleLine.
*/
QString QFontMetrics::elidedText(const QString &text, Qt::TextElideMode mode, int width, int flags) const {
    (void)flags;
    if (mode == Qt::ElideNone || horizontalAdvance(text) <= width)
        return text;
    const QString ellipsis =
        m_engine->hasGlyph(0x2026) ? QString(QChar(static_cast<ushort>(0x2026))) : QString::fromLatin1("...");
    const int available = width - horizontalAdvance(ellipsis);
    if (available <= 0)
        return QString();
    const qsizetype n = text.size();
    if (mode == Qt::ElideRight) {
        qsizetype keep = n;
        while (keep > 0 && horizontalAdvance(text.left(keep)) > available)
            --keep;
        return text.left(keep) + ellipsis;
    }
    if (mode == Qt::ElideLeft) {
        qsizetype keep = n;
        while (keep > 0 && horizontalAdvance(text.right(keep)) > available)
            --keep;
        return ellipsis + text.right(keep);
    }
    // Middle: take characters alternately from both ends while they fit.
    qsizetype left = 0, right = 0;
    for (;;) {
        bool grew = false;
        if (left + right < n &&
            horizontalAdvance(text.left(left + 1)) + horizontalAdvance(text.right(right)) <= available) {
            ++left;
            grew = true;
        }
        if (left + right < n &&
            horizontalAdvance(text.left(left)) + horizontalAdvance(text.right(right + 1)) <= available) {
            ++right;
            grew = true;
        }
        if (!grew)
            break;
    }
    return text.left(left) + ellipsis + text.right(right);
}
