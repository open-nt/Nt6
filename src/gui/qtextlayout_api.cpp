// MIT License - see LICENSE
#include <QtGui/qtextlayout.h>

#include <QtGui/qfontmetrics.h>
#include <QtGui/qpainter.h>

#include <algorithm>
#include <cmath>
#include <vector>

/*!
    \class QTextOption
    \brief The QTextOption class provides a description of general rich text properties.

    A text option says how a QTextLayout aligns its lines, how it breaks
    them, and how wide a tab is.

    \differences
    There are no text directions, flags (such as showing tabs and spaces)
    or lists of tab stops.
    \enddifferences

    \sa QTextLayout
*/

/*!
    \class QTextLine
    \brief The QTextLine class represents a line of text inside a QTextLayout.

    A line is made by QTextLayout::createLine(). setLineWidth() gives it
    its width, which decides how much of the text it takes, and
    setPosition() places it. Afterwards the line tells its size, the part
    of the text that it has, and where a cursor position is on it
    (cursorToX() and xToCursor()).

    A QTextLine refers to its layout: it is valid for as long as the
    layout is, and until the layout is laid out anew.

    \differences
    There is no \c setNumColumns(), no \c glyphRuns() and no setting for
    the leading: a line is as high as the tallest font on it. Positions are
    whole pixels, as Nt draws text at whole pixels.
    \enddifferences

    \sa QTextLayout
*/

/*!
    \class QTextLayout
    \brief The QTextLayout class is used to lay out and render text.

    A QTextLayout breaks one paragraph of text into lines and draws them.
    It is what QTextDocument uses for each of its paragraphs, and it is
    there for code that draws text itself and needs to know where its
    lines and characters are: an editor of its own, or a delegate that
    wraps text.

    The text has one font, and format ranges that change it for parts of
    the text: another family, size or weight, a color, an underline. The
    lines are made one by one, each as wide as the caller wants, which is
    how text flows around something:

    \code
    QTextLayout layout(text, font);
    layout.beginLayout();
    qreal y = 0;
    for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
        line.setLineWidth(width);
        line.setPosition(QPointF(0, y));
        y += line.height();
    }
    layout.endLayout();

    layout.draw(&painter, QPointF(10, 10));
    \endcode

    \differences
    Text is laid out character by character from left to right, without
    shaping, kerning, ligatures, bidirectional text or font fallback, as
    everywhere in Nt GUI. Lines break after spaces; there is no hyphenation
    and no rules of other scripts. There are no cursor movement styles,
    preedit areas, glyph runs or cached layouts, and \c draw() takes its
    selections but no cursor drawing (\c drawCursor()).
    \enddifferences

    \sa QTextLine, QTextDocument, QTextCharFormat
*/

namespace {

// A piece of the text in one format.
struct Span {
    int start = 0;
    int end = 0;
    QTextCharFormat format;
    QFont font;
};

struct LineData {
    int start = 0;
    int length = 0;   // with the line break at its end, if it has one
    int visible = 0;  // without the line break and the spaces at the end
    qreal x = 0;
    qreal y = 0;
    qreal width = 0;    // as set
    qreal natural = 0;  // of the visible text
    int ascent = 0;
    int descent = 0;
    bool sized = false;
};

bool isLineBreak(QChar c) { return c == QChar('\n') || c.unicode() == 0x2028; }

QFont fontFor(const QFont &base, const QTextCharFormat &format) {
    QFont f = base;
    const QStringList families = format.fontFamilies();
    if (!families.isEmpty())
        f.setFamily(families.first());
    else if (format.fontFixedPitch())
        f.setFamily(QStringLiteral("monospace"));
    if (format.hasProperty(QTextFormat::FontPointSize) && format.fontPointSize() > 0)
        f.setPointSizeF(format.fontPointSize());
    if (format.hasProperty(QTextFormat::FontWeight))
        f.setWeight(static_cast<QFont::Weight>(format.fontWeight()));
    if (format.hasProperty(QTextFormat::FontItalic))
        f.setItalic(format.fontItalic());
    return f;
}

} // namespace

struct QTextLayout::Data {
    QString text;
    QFont font;
    QTextOption option;
    QList<FormatRange> formats;
    QPointF position;
    std::vector<LineData> lines;
    int next = 0; // the first character that no line has yet
    bool trailingBreak = false; // the last line ended with a line break at the end of the text
    bool layouting = false;

    mutable std::vector<Span> spans;      // made when they are needed
    mutable std::vector<qreal> advances;  // of each character; the second half of a surrogate pair has 0
    mutable bool measured = false;

    void invalidate() {
        measured = false;
        spans.clear();
        advances.clear();
    }

    // Splits the text into spans of one format each, from the format ranges, and measures the characters.
    void measure() const {
        if (measured)
            return;
        measured = true;
        const int n = static_cast<int>(text.size());
        std::vector<int> cuts{0, n};
        for (const FormatRange &r : formats) {
            cuts.push_back(std::clamp(r.start, 0, n));
            cuts.push_back(std::clamp(r.start + std::max(0, r.length), 0, n));
        }
        std::sort(cuts.begin(), cuts.end());
        cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
        spans.clear();
        for (std::size_t i = 0; i + 1 < cuts.size(); ++i) {
            Span span;
            span.start = cuts[i];
            span.end = cuts[i + 1];
            // Later ranges go on top of earlier ones.
            for (const FormatRange &r : formats) {
                if (r.start <= span.start && r.start + r.length >= span.end)
                    span.format.merge(r.format);
            }
            span.font = fontFor(font, span.format);
            spans.push_back(span);
        }
        if (spans.empty()) {
            Span span;
            span.font = font;
            spans.push_back(span);
        }
        advances.assign(static_cast<std::size_t>(n), 0);
        for (const Span &span : spans) {
            const QFontMetrics metrics(span.font);
            const qreal tab = option.tabStopDistance() > 0 ? option.tabStopDistance()
                                                           : 8.0 * metrics.horizontalAdvance(QChar(' '));
            for (int i = span.start; i < span.end; ++i) {
                const QChar c = text.at(i);
                const auto at = static_cast<std::size_t>(i);
                if (isLineBreak(c)) {
                    advances[at] = 0;
                } else if (c == QChar('\t')) {
                    advances[at] = -tab; // negative: a tab, which depends on where it is
                } else if (c.isHighSurrogate() && i + 1 < span.end && text.at(i + 1).isLowSurrogate()) {
                    advances[at] = metrics.horizontalAdvance(text.mid(i, 2));
                    ++i;
                } else {
                    advances[at] = metrics.horizontalAdvance(c);
                }
            }
        }
    }

    // The width of the character at `i` when it starts at `x` on its line.
    qreal advanceAt(int i, qreal x) const {
        const qreal a = advances[static_cast<std::size_t>(i)];
        if (a >= 0)
            return a;
        const qreal tab = -a;
        return (std::floor(x / tab) + 1) * tab - x;
    }

    const Span &spanAt(int i) const {
        for (const Span &span : spans) {
            if (i < span.end)
                return span;
        }
        return spans.back();
    }

    // How far the text of a line starts right of the left edge of the line, for its alignment.
    qreal alignmentOffset(const LineData &line) const {
        const Qt::Alignment alignment = option.alignment();
        if (!line.sized || line.width <= line.natural)
            return 0;
        if (alignment & Qt::AlignRight)
            return line.width - line.natural;
        if (alignment & Qt::AlignHCenter)
            return std::floor((line.width - line.natural) / 2);
        return 0;
    }

    void fill(LineData &line, qreal width) const;
};

// Gives a line as much of the text as fits into `width`.
void QTextLayout::Data::fill(LineData &line, qreal width) const {
    measure();
    const int n = static_cast<int>(text.size());
    const QTextOption::WrapMode wrap = option.wrapMode();
    const bool wraps = wrap != QTextOption::NoWrap && wrap != QTextOption::ManualWrap;
    int pos = line.start;
    qreal x = 0;
    int breakAt = -1; // the position after the last space: where the line can end
    while (pos < n) {
        const QChar c = text.at(pos);
        if (isLineBreak(c)) {
            ++pos;
            break;
        }
        const int step = c.isHighSurrogate() && pos + 1 < n && text.at(pos + 1).isLowSurrogate() ? 2 : 1;
        const qreal advance = advanceAt(pos, x);
        if (wraps && x + advance > width && pos > line.start && !c.isSpace()) {
            if (wrap != QTextOption::WrapAnywhere && breakAt > line.start) {
                pos = breakAt;
                break;
            }
            // No space to break at: a word that is longer than the line.
            if (wrap != QTextOption::WordWrap)
                break;
        }
        x += advance;
        pos += step;
        if (c.isSpace())
            breakAt = pos;
    }
    line.length = pos - line.start;
    line.width = width;
    line.sized = true;
    // What shows: without the line break and the spaces at the end.
    int end = pos;
    while (end > line.start && (isLineBreak(text.at(end - 1)) || text.at(end - 1).isSpace()))
        --end;
    line.visible = end - line.start;
    line.natural = 0;
    for (int i = line.start; i < end; ++i)
        line.natural += advanceAt(i, line.natural);
    // The tallest font on the line; an empty line has the height of the font at its start.
    line.ascent = 0;
    line.descent = 0;
    const int last = std::max(line.start, end - 1);
    for (const Span &span : spans) {
        const bool onLine = span.start <= last && span.end > line.start;
        const bool emptyLine = end == line.start && line.start >= span.start && (line.start < span.end || &span == &spans.back());
        if (!onLine && !emptyLine)
            continue;
        const QFontMetrics metrics(span.font);
        line.ascent = std::max(line.ascent, metrics.ascent());
        line.descent = std::max(line.descent, metrics.descent());
    }
    if (line.ascent == 0 && line.descent == 0) {
        const QFontMetrics metrics(font);
        line.ascent = metrics.ascent();
        line.descent = metrics.descent();
    }
}

/*!
    \fn bool QTextLine::isValid() const

    Returns \c true if this is a line of a layout; otherwise returns
    \c false. QTextLayout::createLine() returns an invalid line when all
    the text is in lines.
*/

/*!
    \fn int QTextLine::lineNumber() const

    Returns the position of the line among the lines of its layout,
    counted from 0.
*/

#define NT_LINE(fallback)                                                                                              \
    if (!m_layout || m_index < 0 || m_index >= static_cast<int>(m_layout->d->lines.size()))                            \
        return fallback;                                                                                               \
    const LineData &line = m_layout->d->lines[static_cast<std::size_t>(m_index)]

/*!
    Returns the rectangle of the line: its position, its width as it was
    set, and its height.

    \sa naturalTextRect()
*/
QRectF QTextLine::rect() const {
    NT_LINE(QRectF());
    return QRectF(line.x, line.y, line.width, line.ascent + line.descent);
}

/*!
    Returns the horizontal position of the line in its layout.

    \sa setPosition()
*/
qreal QTextLine::x() const {
    NT_LINE(0);
    return line.x;
}

/*!
    Returns the vertical position of the top of the line in its layout.

    \sa setPosition()
*/
qreal QTextLine::y() const {
    NT_LINE(0);
    return line.y;
}

/*!
    Returns the width of the line, as setLineWidth() set it.

    \sa naturalTextWidth()
*/
qreal QTextLine::width() const {
    NT_LINE(0);
    return line.width;
}

/*!
    Returns how far the line reaches above its baseline: the ascent of
    the tallest font on it.
*/
qreal QTextLine::ascent() const {
    NT_LINE(0);
    return line.ascent;
}

/*!
    Returns how far the line reaches below its baseline.
*/
qreal QTextLine::descent() const {
    NT_LINE(0);
    return line.descent;
}

/*!
    Returns the height of the line: its ascent and its descent.
*/
qreal QTextLine::height() const {
    NT_LINE(0);
    return line.ascent + line.descent;
}

/*!
    Returns the leading of the line, which is always 0.
*/
qreal QTextLine::leading() const { return 0; }

/*!
    Returns the width of the text on the line, without the spaces at its
    end. It is more than width() if the line has a word that does not fit.

    \sa width()
*/
qreal QTextLine::naturalTextWidth() const {
    NT_LINE(0);
    return line.natural;
}

/*!
    Returns the rectangle that the text of the line takes: where the
    alignment puts it inside rect().
*/
QRectF QTextLine::naturalTextRect() const {
    NT_LINE(QRectF());
    return QRectF(line.x + m_layout->d->alignmentOffset(line), line.y, line.natural, line.ascent + line.descent);
}

/*!
    Lays the line out to the width \a width: the line takes the text from
    where the line before it ended up to what fits, or up to the next line
    break. Call this for each line right after QTextLayout::createLine().

    \sa naturalTextWidth(), textLength()
*/
void QTextLine::setLineWidth(qreal width) {
    if (!m_layout || m_index < 0 || m_index >= static_cast<int>(m_layout->d->lines.size()))
        return;
    QTextLayout::Data *d = m_layout->d.get();
    LineData &line = d->lines[static_cast<std::size_t>(m_index)];
    d->fill(line, std::max<qreal>(0, width));
    // Only the last line decides where the next one starts.
    if (m_index + 1 == static_cast<int>(d->lines.size())) {
        d->next = line.start + line.length;
        const int n = static_cast<int>(d->text.size());
        d->trailingBreak = line.length > 0 && d->next == n && isLineBreak(d->text.at(n - 1));
    }
}

/*!
    Places the top left corner of the line at \a pos, in the coordinates
    of its layout.

    \sa position()
*/
void QTextLine::setPosition(const QPointF &pos) {
    if (!m_layout || m_index < 0 || m_index >= static_cast<int>(m_layout->d->lines.size()))
        return;
    LineData &line = m_layout->d->lines[static_cast<std::size_t>(m_index)];
    line.x = pos.x();
    line.y = pos.y();
}

/*!
    Returns the position of the top left corner of the line in its layout.

    \sa setPosition()
*/
QPointF QTextLine::position() const {
    NT_LINE(QPointF());
    return QPointF(line.x, line.y);
}

/*!
    Returns the position in the text of the layout where the line starts.

    \sa textLength()
*/
int QTextLine::textStart() const {
    NT_LINE(0);
    return line.start;
}

/*!
    Returns the number of characters of the line, with the spaces and the
    line break at its end.

    \sa textStart()
*/
int QTextLine::textLength() const {
    NT_LINE(0);
    return line.length;
}

/*!
    Returns the horizontal position, in the layout, of the cursor position
    \a cursorPos of the text: the left edge of the character there, or its
    right edge with \a edge \c Trailing. A position outside of the line
    gives the start or the end of the line.

    \sa xToCursor()
*/
qreal QTextLine::cursorToX(int cursorPos, Edge edge) const {
    NT_LINE(0);
    const QTextLayout::Data *d = m_layout->d.get();
    d->measure();
    const int end = line.start + line.visible;
    int pos = std::clamp(cursorPos, line.start, end);
    if (edge == Trailing && pos < end)
        ++pos;
    qreal x = 0;
    for (int i = line.start; i < pos; ++i)
        x += d->advanceAt(i, x);
    return line.x + d->alignmentOffset(line) + x;
}

/*!
    Returns the cursor position of the text that is nearest to the
    horizontal position \a x of the layout: between the two characters
    whose border is closest.

    \sa cursorToX()
*/
int QTextLine::xToCursor(qreal x) const {
    NT_LINE(0);
    const QTextLayout::Data *d = m_layout->d.get();
    d->measure();
    const qreal local = x - line.x - d->alignmentOffset(line);
    const int end = line.start + line.visible;
    qreal at = 0;
    for (int i = line.start; i < end; ++i) {
        const qreal advance = d->advanceAt(i, at);
        const bool pair = d->text.at(i).isHighSurrogate() && i + 1 < end;
        if (local < at + advance / 2)
            return i;
        at += advance;
        if (pair)
            ++i;
    }
    return end;
}

#undef NT_LINE

/*!
    Draws the line with \a painter, with the layout at \a position.
*/
void QTextLine::draw(QPainter *painter, const QPointF &position) const {
    if (!m_layout || !painter || m_index < 0 || m_index >= static_cast<int>(m_layout->d->lines.size()))
        return;
    const QTextLayout::Data *d = m_layout->d.get();
    d->measure();
    const LineData &line = d->lines[static_cast<std::size_t>(m_index)];
    const QPen pen = painter->pen();
    const QFont font = painter->font();
    const int top = qRound(position.y() + line.y);
    const int baseline = top + line.ascent;
    const int end = line.start + line.visible;
    qreal x = position.x() + line.x + d->alignmentOffset(line);
    const qreal origin = x;
    for (const Span &span : d->spans) {
        const int from = std::max(span.start, line.start);
        const int to = std::min(span.end, end);
        if (from >= to)
            continue;
        // A tab ends a piece: what follows it starts at the tab stop.
        int piece = from;
        while (piece < to) {
            int stop = piece;
            while (stop < to && d->text.at(stop) != QChar('\t'))
                ++stop;
            qreal width = 0;
            for (int i = piece; i < stop; ++i)
                width += d->advanceAt(i, x - origin + width);
            const int left = qRound(x);
            const int pieceWidth = qRound(x + width) - left;
            const QBrush background = span.format.background();
            if (background.style() != Qt::NoBrush && pieceWidth > 0)
                painter->fillRect(QRect(left, top, pieceWidth, line.ascent + line.descent), background);
            if (stop > piece) {
                const QBrush foreground = span.format.foreground();
                const QColor color = foreground.style() != Qt::NoBrush ? foreground.color() : pen.color();
                painter->setPen(color);
                painter->setFont(span.font);
                painter->drawText(QPoint(left, baseline), d->text.mid(piece, stop - piece));
                if (span.format.fontUnderline() && pieceWidth > 0)
                    painter->drawLine(left, baseline + 1, left + pieceWidth - 1, baseline + 1);
                if (span.format.fontStrikeOut() && pieceWidth > 0) {
                    const int middle = baseline - QFontMetrics(span.font).ascent() / 3;
                    painter->drawLine(left, middle, left + pieceWidth - 1, middle);
                }
            }
            x += width;
            if (stop < to) { // the tab itself
                x += d->advanceAt(stop, x - origin);
                ++stop;
            }
            piece = stop;
        }
    }
    painter->setPen(pen);
    painter->setFont(font);
}

/*!
    Constructs an empty layout.
*/
QTextLayout::QTextLayout() : d(std::make_unique<Data>()) {}

/*!
    Constructs a layout for the text \a text, in the default font.
*/
QTextLayout::QTextLayout(const QString &text) : d(std::make_unique<Data>()) { d->text = text; }

/*!
    Constructs a layout for the text \a text in the font \a font.
    \a paintdevice is not used: Nt measures text the same for every
    device.
*/
QTextLayout::QTextLayout(const QString &text, const QFont &font, const QPaintDevice *paintdevice)
    : d(std::make_unique<Data>()) {
    static_cast<void>(paintdevice);
    d->text = text;
    d->font = font;
}

/*!
    Destroys the layout. Its lines are no longer valid.
*/
QTextLayout::~QTextLayout() = default;

/*!
    Sets the font of the text to \a font. The lines are discarded.

    \sa font()
*/
void QTextLayout::setFont(const QFont &font) {
    d->font = font;
    d->invalidate();
    clearLayout();
}

/*!
    Returns the font of the text, which format ranges change for parts of
    it.

    \sa setFont(), setFormats()
*/
QFont QTextLayout::font() const { return d->font; }

/*!
    Sets the text to lay out to \a string. The lines are discarded, and so
    are the format ranges.

    \sa text()
*/
void QTextLayout::setText(const QString &string) {
    d->text = string;
    d->formats.clear();
    d->invalidate();
    clearLayout();
}

/*!
    Returns the text of the layout.

    \sa setText()
*/
QString QTextLayout::text() const { return d->text; }

/*!
    Sets the alignment, the wrap mode and the tab stops of the layout to
    those of \a option. Lay the text out again afterwards.

    \sa textOption()
*/
void QTextLayout::setTextOption(const QTextOption &option) {
    d->option = option;
    d->invalidate();
}

/*!
    Returns the text option of the layout.

    \sa setTextOption()
*/
const QTextOption &QTextLayout::textOption() const { return d->option; }

/*!
    Sets the format ranges of the text to \a formats: each of them gives a
    part of the text its own font properties, colors, underline or strike
    out. Where ranges overlap, a later one goes on top of an earlier one.
    The lines are discarded.

    \sa formats(), clearFormats()
*/
void QTextLayout::setFormats(const QList<FormatRange> &formats) {
    d->formats = formats;
    d->invalidate();
    clearLayout();
}

/*!
    Returns the format ranges of the text.

    \sa setFormats()
*/
QList<QTextLayout::FormatRange> QTextLayout::formats() const { return d->formats; }

/*!
    Removes the format ranges.

    \sa setFormats()
*/
void QTextLayout::clearFormats() { setFormats(QList<FormatRange>()); }

/*!
    Starts a layout: discards the lines, so that createLine() starts at
    the beginning of the text.

    \sa endLayout(), createLine()
*/
void QTextLayout::beginLayout() {
    clearLayout();
    d->layouting = true;
}

/*!
    Ends the layout that beginLayout() started. Text that no line has
    taken does not show.

    \sa beginLayout()
*/
void QTextLayout::endLayout() { d->layouting = false; }

/*!
    Discards the lines of the layout.

    \sa beginLayout()
*/
void QTextLayout::clearLayout() {
    d->lines.clear();
    d->next = 0;
    d->trailingBreak = false;
}

/*!
    Returns a new line for the text that the lines before it have not
    taken, or an invalid line if there is none left. An empty text has one
    line, which is empty. Give the line its width with
    QTextLine::setLineWidth() before the next line is created.

    \sa beginLayout(), lineAt()
*/
QTextLine QTextLayout::createLine() {
    if (!d->layouting)
        return QTextLine();
    // A line that was created but not given a width takes nothing: its text is not handed out twice.
    if (!d->lines.empty() && !d->lines.back().sized)
        return QTextLine(this, static_cast<int>(d->lines.size()) - 1);
    // There is a line to make while text is left, for an empty text (one empty line), and after a line break
    // at the very end of the text (the empty line that follows it).
    const int n = static_cast<int>(d->text.size());
    if (d->next >= n && !d->lines.empty() && !d->trailingBreak)
        return QTextLine();
    d->trailingBreak = false;
    LineData line;
    line.start = d->next;
    d->lines.push_back(line);
    return QTextLine(this, static_cast<int>(d->lines.size()) - 1);
}

/*!
    Returns the number of lines of the layout.

    \sa lineAt()
*/
int QTextLayout::lineCount() const { return static_cast<int>(d->lines.size()); }

/*!
    Returns the line at position \a i, counted from 0, or an invalid line
    if there is none.

    \sa lineCount(), lineForTextPosition()
*/
QTextLine QTextLayout::lineAt(int i) const {
    return i >= 0 && i < lineCount() ? QTextLine(this, i) : QTextLine();
}

/*!
    Returns the line that has the position \a pos of the text, or an
    invalid line if no line has it. The position after the last character
    belongs to the last line.
*/
QTextLine QTextLayout::lineForTextPosition(int pos) const {
    for (int i = 0; i < lineCount(); ++i) {
        const LineData &line = d->lines[static_cast<std::size_t>(i)];
        if (pos >= line.start && pos < line.start + line.length)
            return QTextLine(this, i);
    }
    if (lineCount() > 0 && pos == static_cast<int>(d->text.size()) && pos >= 0)
        return QTextLine(this, lineCount() - 1);
    return QTextLine();
}

/*!
    Returns \c true if a cursor can stand at the position \a pos of the
    text: anywhere from 0 to the length of the text, except between the
    two halves of a surrogate pair.
*/
bool QTextLayout::isValidCursorPosition(int pos) const {
    const int n = static_cast<int>(d->text.size());
    if (pos < 0 || pos > n)
        return false;
    return !(pos > 0 && pos < n && d->text.at(pos).isLowSurrogate() && d->text.at(pos - 1).isHighSurrogate());
}

/*!
    Returns the cursor position after \a oldPos: the next character, or
    with \a mode \c SkipWords the start of the next word. It is the length
    of the text if there is nothing after \a oldPos.

    \sa previousCursorPosition()
*/
int QTextLayout::nextCursorPosition(int oldPos, CursorMode mode) const {
    const int n = static_cast<int>(d->text.size());
    int pos = std::clamp(oldPos, 0, n);
    if (pos >= n)
        return n;
    if (mode == SkipCharacters) {
        ++pos;
        while (!isValidCursorPosition(pos))
            ++pos;
        return pos;
    }
    while (pos < n && !d->text.at(pos).isSpace())
        ++pos;
    while (pos < n && d->text.at(pos).isSpace())
        ++pos;
    return pos;
}

/*!
    Returns the cursor position before \a oldPos: the character before
    it, or with \a mode \c SkipWords the start of the word before it. It
    is 0 if there is nothing before \a oldPos.

    \sa nextCursorPosition()
*/
int QTextLayout::previousCursorPosition(int oldPos, CursorMode mode) const {
    const int n = static_cast<int>(d->text.size());
    int pos = std::clamp(oldPos, 0, n);
    if (pos <= 0)
        return 0;
    if (mode == SkipCharacters) {
        --pos;
        while (!isValidCursorPosition(pos))
            --pos;
        return pos;
    }
    while (pos > 0 && d->text.at(pos - 1).isSpace())
        --pos;
    while (pos > 0 && !d->text.at(pos - 1).isSpace())
        --pos;
    return pos;
}

/*!
    Returns the position of the layout, which draw() adds to the position
    that it is given. A layout of a document has the position of its
    paragraph here.

    \sa setPosition()
*/
QPointF QTextLayout::position() const { return d->position; }

/*!
    Sets the position of the layout to \a p.

    \sa position()
*/
void QTextLayout::setPosition(const QPointF &p) { d->position = p; }

/*!
    Returns the smallest rectangle that has all the lines of the layout,
    in the coordinates of the layout.
*/
QRectF QTextLayout::boundingRect() const {
    if (d->lines.empty())
        return QRectF();
    qreal left = d->lines.front().x, top = d->lines.front().y;
    qreal right = left, bottom = top;
    for (const LineData &line : d->lines) {
        left = std::min(left, line.x);
        top = std::min(top, line.y);
        right = std::max(right, line.x + std::max(line.width, line.natural));
        bottom = std::max(bottom, line.y + line.ascent + line.descent);
    }
    return QRectF(left, top, right - left, bottom - top);
}

/*!
    Returns the width that the layout cannot go below without breaking a
    word: that of its longest word.

    \sa maximumWidth()
*/
qreal QTextLayout::minimumWidth() const {
    d->measure();
    qreal widest = 0, word = 0;
    for (int i = 0; i < static_cast<int>(d->text.size()); ++i) {
        const QChar c = d->text.at(i);
        if (c.isSpace() || isLineBreak(c)) {
            widest = std::max(widest, word);
            word = 0;
        } else {
            word += d->advanceAt(i, word);
        }
    }
    return std::max(widest, word);
}

/*!
    Returns the width that the text takes when no line is broken: that of
    its longest paragraph line.

    \sa minimumWidth()
*/
qreal QTextLayout::maximumWidth() const {
    d->measure();
    qreal widest = 0, current = 0;
    for (int i = 0; i < static_cast<int>(d->text.size()); ++i) {
        if (isLineBreak(d->text.at(i))) {
            widest = std::max(widest, current);
            current = 0;
        } else {
            current += d->advanceAt(i, current);
        }
    }
    return std::max(widest, current);
}

/*!
    Draws the lines of the layout with \a p, with the top left corner of
    the layout at \a pos plus position(). The text has the color of the
    pen of the painter where no format range gives it another one. The
    formats in \a selections are drawn on top of the formats of the layout,
    which is how a selection is shown. With a valid \a clip, only the
    lines that touch it are drawn.
*/
void QTextLayout::draw(QPainter *p, const QPointF &pos, const QList<FormatRange> &selections, const QRectF &clip) const {
    if (!p || d->lines.empty())
        return;
    const QPointF origin = pos + d->position;
    // Selections are formats on top of the others, for this one drawing.
    const QList<FormatRange> saved = d->formats;
    if (!selections.isEmpty()) {
        d->formats += selections;
        d->invalidate();
    }
    for (int i = 0; i < lineCount(); ++i) {
        const LineData &line = d->lines[static_cast<std::size_t>(i)];
        if (clip.isValid()) {
            const qreal top = origin.y() + line.y;
            if (top + line.ascent + line.descent < clip.top() || top > clip.top() + clip.height())
                continue;
        }
        QTextLine(this, i).draw(p, origin);
    }
    if (!selections.isEmpty()) {
        d->formats = saved;
        d->invalidate();
    }
}
