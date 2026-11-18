// MIT License - see LICENSE
#include <QtGui/qtextdocument.h>

#include <QtCore/qhash.h>
#include <QtGui/qcolor.h>
#include <QtGui/qfontmetrics.h>
#include <QtGui/qpainter.h>
#include <QtGui/qtextobject.h>
#include <QtGui/qtexttable.h>

#include "qtextmarkdown_p.h"

#include <algorithm>
#include <cmath>
#include <vector>

/*!
    \class QTextDocument
    \brief The QTextDocument class holds formatted text.

    A QTextDocument is text in paragraphs, each with a format of its own
    and with formats for parts of its text: fonts, sizes, colors, links,
    headings and lists. It reads and writes a subset of HTML, lays itself
    out for a width, and draws itself:

    \code
    QTextDocument document;
    document.setHtml("<h2>Report</h2><p>Sales grew by <b>12&nbsp;%</b>; see the "
                     "<a href=\"details.html\">details</a>.</p>");
    document.setTextWidth(300);
    document.drawContents(&painter);
    \endcode

    size() is the size that the document takes for textWidth(), and
    idealWidth() the width that it would like. documentLayout() tells what
    is where: the link under a point, with anchorAt(). begin() and the
    QTextBlock objects that it leads to give the paragraphs and their
    formats.

    \section1 HTML

    setHtml() understands these elements; others are ignored, their text
    is kept.

    \list
    \li Paragraphs and structure: \c p, \c div, \c br, \c hr, \c h1 to
        \c h6, \c pre, \c blockquote, \c center, and \c ul, \c ol and
        \c li for lists.
    \li Text: \c b and \c strong, \c i and \c em, \c u, \c s, \c strike
        and \c del, \c tt, \c code and \c kbd, \c big and \c small,
        \c font with \c color, \c face and \c size, \c span, and \c a with
        \c href for a link.
    \li Tables become QTextTable frames: a row is a paragraph, with tabs
        between its cells, laid out in columns. \c border, \c cellpadding
        and \c cellspacing set the format of the table.
    \li An \c img is shown as its \c alt text.
    \endlist

    The \c style attribute sets \c color, \c background-color,
    \c font-weight, \c font-style, \c font-size, \c font-family,
    \c text-decoration and, for a paragraph, \c text-align and the
    margins. \c align does the alignment too. The usual character
    references are decoded.

    \differences
    The document is made as a whole, with setPlainText() or setHtml(): it
    has no QTextCursor, no undo and redo, and its contents are not edited
    in place. Tables are simple (see QTextTable), and there are no images,
    style sheets, resources, printing, \c find() (QPlainTextEdit::find() searches) or page sizes. The layout is
    the one of the document: \c setDocumentLayout() is not there, and
    QAbstractTextDocumentLayout is a small class that answers questions
    about it. The HTML that toHtml() writes is simpler than that of Qt.
    Lists do not nest by structure: a nested list is a list with a deeper
    indentation.
    \enddifferences

    \sa QTextLayout, QTextCharFormat, QTextBlock
*/

/*!
    \class QTextBlock
    \brief The QTextBlock class provides a container for text fragments in a QTextDocument.

    A block is a paragraph of a document. It has a text(), a block format
    for the paragraph as a whole, and character formats for the parts of
    its text (textFormats()). next() leads through the document:

    \code
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next())
        qDebug() << block.blockNumber() << block.text();
    \endcode

    A QTextBlock refers to its document: it is valid for as long as the
    document is, and until the document gets other contents.

    \differences
    A block gives its formats as a list of format ranges; there are no
    fragment iterators, no user data or states, no revisions, and blocks
    cannot be hidden. \c end() of a document is an invalid block.
    \enddifferences

    \sa QTextDocument
*/

/*!
    \class QAbstractTextDocumentLayout
    \brief The QAbstractTextDocumentLayout class is an abstract base class used to implement custom layouts for QTextDocuments.

    In Nt this class is what QTextDocument::documentLayout() returns: it
    answers questions about where things are in the document as it is laid
    out: the link at a point, the rectangle of a paragraph, and the size of
    the whole.

    \differences
    This is not a base class for layouts of an application, and not a
    QObject: it has no signals, no \c draw(), \c hitTest() or
    \c formatAt(), and a document cannot be given another layout.
    \enddifferences

    \sa QTextDocument
*/

namespace {

constexpr qreal kIndentStep = 40;

struct Block {
    QString text; // a line break inside it is U+2028
    QList<QTextLayout::FormatRange> formats;
    QTextBlockFormat format;
    int listStyle = 0;  // QTextListFormat::Style, or 0 for a block that is not in a list
    int listNumber = 0; // of an item of a numbered list
    int listLevel = 0; // the depth of the lists that the block is in
    bool rule = false; // a horizontal line
    bool collapsed = false; // while reading HTML: the text ends with a space that stands for white space
    int table = -1;         // the table that the block is a row of
    std::unique_ptr<QTextLayout> layout;
    qreal y = 0;
    qreal left = 0;
    qreal height = 0;
};

// A table of the document: its rows are the blocks from `first` to `last`, its cells parted by tabs.
struct TableInfo {
    int first = -1;
    int last = -1;
    int rows = 0;
    int columns = 0;
    int column = 0; // while reading HTML: the cells of the current row so far
    QTextTableFormat format;
    // As laid out:
    qreal x = 0;    // the left edge
    qreal cell = 0; // the width of a column
    qreal bottom = 0;
    std::vector<qreal> rowTops;
};

QString marker(const Block &block) {
    switch (block.listStyle) {
    case QTextListFormat::ListDisc:
        return QString(QChar(0x2022));
    case QTextListFormat::ListCircle:
        return QString(QChar(0x25e6));
    case QTextListFormat::ListSquare:
        return QString(QChar(0x25aa));
    case QTextListFormat::ListDecimal:
        return QString::number(block.listNumber) + QChar('.');
    case QTextListFormat::ListLowerAlpha:
        return QString(QChar(static_cast<char16_t>('a' + (block.listNumber - 1) % 26))) + QChar('.');
    case QTextListFormat::ListUpperAlpha:
        return QString(QChar(static_cast<char16_t>('A' + (block.listNumber - 1) % 26))) + QChar('.');
    default:
        return QString();
    }
}

} // namespace

class QTextDocumentPrivate {
public:
    explicit QTextDocumentPrivate(QTextDocument *document) : q(document), layoutObject(document) {}

    QTextDocument *q;
    std::vector<Block> blocks;
    QFont font;
    QTextOption option;
    QString title;
    QString url;
    qreal textWidth = -1;
    qreal margin = 4;
    QAbstractTextDocumentLayout layoutObject;
    bool laidOut = false;
    QSizeF laidOutSize;
    std::vector<TableInfo> tables;
    std::vector<TableInfo> parsedTables;    // the tables of the HTML being read, for setBlocks()
    QTextFrame *root = nullptr;             // a child of the document
    std::vector<QTextTable *> tableObjects; // children of the root frame, one for each of the tables
    QString styleSheet;
    struct StyleRule {
        QStringList parts; // a selector: its simple selectors, the element itself last
        QString declarations;
    };
    std::vector<StyleRule> styleRules;

    void setBlocks(std::vector<Block> &&fresh) {
        blocks = std::move(fresh);
        if (blocks.empty())
            blocks.emplace_back(); // a document always has a block
        tables = std::move(parsedTables);
        parsedTables.clear();
        rebuildFrames();
        laidOut = false;
        emit q->contentsChanged();
    }
    QTextFrame *rootFrame() {
        if (!root)
            root = new QTextFrame(q);
        return root;
    }
    void rebuildFrames() {
        for (QTextTable *table : tableObjects)
            delete table;
        tableObjects.clear();
        for (const TableInfo &info : tables) {
            auto *table = new QTextTable(q);
            table->setParent(rootFrame());
            table->m_rows = info.rows;
            table->m_columns = info.columns;
            table->m_firstBlock = info.first;
            table->m_lastBlock = info.last;
            table->m_format = info.format;
            tableObjects.push_back(table);
        }
    }
    void tableFormatChanged(const QTextTable *table) {
        const auto it = std::find(tableObjects.begin(), tableObjects.end(), table);
        if (it == tableObjects.end())
            return;
        tables[static_cast<std::size_t>(it - tableObjects.begin())].format = table->format();
        laidOut = false;
        emit q->contentsChanged();
    }
    void ensureLayout();
    void parseHtml(const QString &html);
    // The blocks of `html`, with its tables in parsedTables; the title goes to `title`.
    std::vector<Block> htmlBlocks(const QString &html);
    // Puts `fragment` in the place of the characters from `from` to `to`; with `isHtml` it is HTML, otherwise
    // plain text. Returns the position after it.
    int replace(int from, int to, const QString &fragment, bool isHtml);
    QString markdown() const;
    QString blockHtml(const Block &block) const;
    static QTextDocumentPrivate *get(const QTextDocument *document) { return document->d.get(); }
};

// Lays the blocks out one under the other, for the text width.
void QTextDocumentPrivate::ensureLayout() {
    if (laidOut)
        return;
    laidOut = true;
    const bool wraps = textWidth > 0;
    const qreal available = wraps ? std::max<qreal>(1, textWidth - 2 * margin) : 1e7;
    qreal y = margin;
    qreal widest = 0;
    // The columns of a table are as wide as its widest cell.
    QFont bold = font;
    bold.setBold(true);
    const QFontMetricsF boldMetrics(bold);
    for (TableInfo &table : tables) {
        qreal cell = 0;
        for (int b = table.first; b <= table.last; ++b)
            for (const QString &text : blocks[static_cast<std::size_t>(b)].text.split(QChar('\t')))
                cell = std::max(cell, boldMetrics.horizontalAdvance(text));
        const qreal padding = table.format.cellPadding() + table.format.cellSpacing() / 2;
        table.cell = std::ceil(cell + 2 * padding + table.format.border());
        table.rowTops.clear();
    }
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        Block &block = blocks[index];
        const QTextBlockFormat &format = block.format;
        TableInfo *table = block.table >= 0 ? &tables[static_cast<std::size_t>(block.table)] : nullptr;
        qreal left = margin + format.leftMargin() + (format.indent() + block.listLevel) * kIndentStep;
        qreal cellInset = 0; // the border and the padding of a cell
        if (table) {
            cellInset = table->format.border() + table->format.cellPadding() + table->format.cellSpacing() / 2;
            table->x = left;
            table->rowTops.push_back(y);
            left += cellInset;
        }
        const qreal width = std::max<qreal>(1, available - (left - margin) - format.rightMargin());
        y += format.topMargin();
        if (!block.layout)
            block.layout = std::make_unique<QTextLayout>();
        QTextLayout &layout = *block.layout;
        if (layout.text() != block.text)
            layout.setText(block.text);
        layout.setFont(font);
        layout.setFormats(block.formats);
        QTextOption blockOption = option;
        blockOption.setAlignment(format.alignment());
        if (!wraps || table)
            blockOption.setWrapMode(QTextOption::NoWrap);
        if (table)
            blockOption.setTabStopDistance(table->cell);
        layout.setTextOption(blockOption);
        y += cellInset;
        layout.setPosition(QPointF(left, y));
        layout.beginLayout();
        qreal dy = 0;
        bool first = true;
        for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
            const qreal indent = first ? format.textIndent() : 0;
            // Without a text width a line is as wide as its text, so that nothing is moved by the alignment.
            line.setLineWidth(std::max<qreal>(1, width - indent));
            line.setPosition(QPointF(indent, dy));
            dy += line.height();
            widest = std::max(widest, left - margin + indent + line.naturalTextWidth() + format.rightMargin());
            first = false;
        }
        layout.endLayout();
        if (block.rule)
            dy = 9;
        block.y = y;
        block.left = left;
        block.height = dy;
        y += dy + format.bottomMargin();
        if (table) {
            y += cellInset - table->format.border();
            table->bottom = y;
            widest = std::max(widest, table->x - margin + table->cell * table->columns);
            if (static_cast<int>(index) == table->last)
                y += table->format.border();
        }
    }
    laidOutSize = QSizeF(wraps ? textWidth : std::ceil(widest) + 2 * margin, std::ceil(y + margin));
}

// ---------------------------------------------------------------------------------------------------------
// HTML in.

namespace {

const QHash<QString, char32_t> &entities() {
    static const QHash<QString, char32_t> table{
        {QStringLiteral("amp"), '&'},        {QStringLiteral("lt"), '<'},         {QStringLiteral("gt"), '>'},
        {QStringLiteral("quot"), '"'},       {QStringLiteral("apos"), '\''},      {QStringLiteral("nbsp"), 0xa0},
        {QStringLiteral("copy"), 0xa9},      {QStringLiteral("reg"), 0xae},       {QStringLiteral("trade"), 0x2122},
        {QStringLiteral("mdash"), 0x2014},   {QStringLiteral("ndash"), 0x2013},   {QStringLiteral("hellip"), 0x2026},
        {QStringLiteral("laquo"), 0xab},     {QStringLiteral("raquo"), 0xbb},     {QStringLiteral("lsquo"), 0x2018},
        {QStringLiteral("rsquo"), 0x2019},   {QStringLiteral("ldquo"), 0x201c},   {QStringLiteral("rdquo"), 0x201d},
        {QStringLiteral("bull"), 0x2022},    {QStringLiteral("middot"), 0xb7},    {QStringLiteral("deg"), 0xb0},
        {QStringLiteral("plusmn"), 0xb1},    {QStringLiteral("times"), 0xd7},     {QStringLiteral("divide"), 0xf7},
        {QStringLiteral("euro"), 0x20ac},    {QStringLiteral("pound"), 0xa3},     {QStringLiteral("yen"), 0xa5},
        {QStringLiteral("cent"), 0xa2},      {QStringLiteral("sect"), 0xa7},      {QStringLiteral("para"), 0xb6},
        {QStringLiteral("larr"), 0x2190},    {QStringLiteral("rarr"), 0x2192},    {QStringLiteral("uarr"), 0x2191},
        {QStringLiteral("darr"), 0x2193},    {QStringLiteral("auml"), 0xe4},      {QStringLiteral("ouml"), 0xf6},
        {QStringLiteral("uuml"), 0xfc},      {QStringLiteral("Auml"), 0xc4},      {QStringLiteral("Ouml"), 0xd6},
        {QStringLiteral("Uuml"), 0xdc},      {QStringLiteral("szlig"), 0xdf},     {QStringLiteral("eacute"), 0xe9},
        {QStringLiteral("egrave"), 0xe8},    {QStringLiteral("agrave"), 0xe0},    {QStringLiteral("aacute"), 0xe1},
        {QStringLiteral("ccedil"), 0xe7},    {QStringLiteral("ntilde"), 0xf1},    {QStringLiteral("shy"), 0xad},
    };
    return table;
}

void appendCodePoint(QString &out, char32_t cp) {
    if (cp == 0 || cp > 0x10ffff || (cp >= 0xd800 && cp < 0xe000))
        cp = 0xfffd;
    if (cp >= 0x10000) {
        cp -= 0x10000;
        out.append(QChar(static_cast<char16_t>(0xd800 + (cp >> 10))));
        out.append(QChar(static_cast<char16_t>(0xdc00 + (cp & 0x3ff))));
    } else {
        out.append(QChar(static_cast<char16_t>(cp)));
    }
}

// Replaces the character references of `text`. A reference that is not known stays as it is written.
QString decodeEntities(const QString &text) {
    if (!text.contains(QChar('&')))
        return text;
    QString out;
    const qsizetype n = text.size();
    for (qsizetype i = 0; i < n; ++i) {
        const QChar c = text.at(i);
        if (c != QChar('&')) {
            out.append(c);
            continue;
        }
        const qsizetype end = text.indexOf(QChar(';'), i + 1);
        if (end < 0 || end - i > 12) {
            out.append(c);
            continue;
        }
        const QString name = text.mid(i + 1, end - i - 1);
        bool ok = false;
        char32_t cp = 0;
        if (name.startsWith(QStringLiteral("#x")) || name.startsWith(QStringLiteral("#X"))) {
            cp = name.mid(2).toUInt(&ok, 16);
        } else if (name.startsWith(QChar('#'))) {
            cp = name.mid(1).toUInt(&ok, 10);
        } else {
            const auto it = entities().constFind(name);
            if (it != entities().constEnd()) {
                cp = it.value();
                ok = true;
            }
        }
        if (!ok) {
            out.append(c);
            continue;
        }
        appendCodePoint(out, cp);
        i = end;
    }
    return out;
}

QColor cssColor(const QString &value) {
    const QString v = value.trimmed();
    if (v.startsWith(QStringLiteral("rgb(")) && v.endsWith(QChar(')'))) {
        const QStringList parts = v.mid(4, v.size() - 5).split(QChar(','));
        if (parts.size() == 3) {
            const auto channel = [](const QString &part) { return std::clamp(part.trimmed().toInt(), 0, 255); };
            return QColor(channel(parts.at(0)), channel(parts.at(1)), channel(parts.at(2)));
        }
        return QColor();
    }
    return QColor::isValidColor(v) ? QColor::fromString(v) : QColor();
}

// A font size of CSS in points, or 0 if it is not understood. `base` is the size that percentages and the
// relative keywords are taken of.
qreal cssFontSize(const QString &value, qreal base) {
    const QString v = value.trimmed().toLower();
    static const QHash<QString, qreal> keywords{
        {QStringLiteral("xx-small"), 0.6}, {QStringLiteral("x-small"), 0.75}, {QStringLiteral("small"), 0.89},
        {QStringLiteral("medium"), 1.0},   {QStringLiteral("large"), 1.2},    {QStringLiteral("x-large"), 1.5},
        {QStringLiteral("xx-large"), 2.0}, {QStringLiteral("smaller"), 0.83}, {QStringLiteral("larger"), 1.2},
    };
    const auto it = keywords.constFind(v);
    if (it != keywords.constEnd())
        return base * it.value();
    bool ok = false;
    if (v.endsWith(QStringLiteral("pt"))) {
        const qreal size = v.left(v.size() - 2).toDouble(&ok);
        return ok && size > 0 ? size : 0;
    }
    if (v.endsWith(QStringLiteral("px"))) {
        const qreal size = v.left(v.size() - 2).toDouble(&ok);
        return ok && size > 0 ? size * 0.75 : 0;
    }
    if (v.endsWith(QChar('%'))) {
        const qreal percent = v.left(v.size() - 1).toDouble(&ok);
        return ok && percent > 0 ? base * percent / 100 : 0;
    }
    if (v.endsWith(QStringLiteral("em"))) {
        const qreal factor = v.left(v.size() - 2).toDouble(&ok);
        return ok && factor > 0 ? base * factor : 0;
    }
    return 0;
}

qreal cssLength(const QString &value) {
    QString v = value.trimmed().toLower();
    if (v.endsWith(QStringLiteral("px")))
        v.chop(2);
    bool ok = false;
    const qreal length = v.toDouble(&ok);
    return ok && length >= 0 ? length : -1;
}

Qt::Alignment alignmentOf(const QString &value, bool *ok) {
    const QString v = value.trimmed().toLower();
    *ok = true;
    if (v == QStringLiteral("left"))
        return Qt::AlignLeft;
    if (v == QStringLiteral("right"))
        return Qt::AlignRight;
    if (v == QStringLiteral("center") || v == QStringLiteral("middle"))
        return Qt::AlignHCenter;
    if (v == QStringLiteral("justify"))
        return Qt::AlignLeft; // lines are not stretched
    *ok = false;
    return Qt::AlignLeft;
}

struct Tag {
    QString name; // in lower case
    QHash<QString, QString> attributes;
    bool closing = false;
    bool selfClosing = false;
};

// Reads the tag that starts at html[pos], which is a '<', and moves pos behind it. Returns false if there is
// no tag there: the '<' is text then.
bool readTag(const QString &html, qsizetype &pos, Tag &tag) {
    const qsizetype n = html.size();
    qsizetype i = pos + 1;
    if (i < n && html.at(i) == QChar('/')) {
        tag.closing = true;
        ++i;
    }
    const qsizetype nameStart = i;
    while (i < n && (html.at(i).isLetterOrNumber() || html.at(i) == QChar(':') || html.at(i) == QChar('-')))
        ++i;
    if (i == nameStart || !html.at(nameStart).isLetter())
        return false;
    tag.name = html.mid(nameStart, i - nameStart).toLower();
    while (i < n) {
        while (i < n && html.at(i).isSpace())
            ++i;
        if (i >= n)
            break;
        if (html.at(i) == QChar('>')) {
            ++i;
            pos = i;
            return true;
        }
        if (html.at(i) == QChar('/')) {
            tag.selfClosing = true;
            ++i;
            continue;
        }
        const qsizetype attributeStart = i;
        while (i < n && !html.at(i).isSpace() && html.at(i) != QChar('=') && html.at(i) != QChar('>') &&
               html.at(i) != QChar('/'))
            ++i;
        const QString attribute = html.mid(attributeStart, i - attributeStart).toLower();
        while (i < n && html.at(i).isSpace())
            ++i;
        QString value;
        if (i < n && html.at(i) == QChar('=')) {
            ++i;
            while (i < n && html.at(i).isSpace())
                ++i;
            if (i < n && (html.at(i) == QChar('"') || html.at(i) == QChar('\''))) {
                const QChar quote = html.at(i);
                const qsizetype end = html.indexOf(quote, i + 1);
                value = html.mid(i + 1, (end < 0 ? n : end) - i - 1);
                i = end < 0 ? n : end + 1;
            } else {
                const qsizetype valueStart = i;
                while (i < n && !html.at(i).isSpace() && html.at(i) != QChar('>'))
                    ++i;
                value = html.mid(valueStart, i - valueStart);
            }
        }
        if (!attribute.isEmpty())
            tag.attributes.insert(attribute, decodeEntities(value));
        else if (i == attributeStart)
            ++i; // a stray character: skip it, so that the loop moves on
    }
    pos = n;
    return true;
}

bool isBlockElement(const QString &name) {
    static const QStringList names{
        QStringLiteral("p"),     QStringLiteral("div"),        QStringLiteral("h1"),     QStringLiteral("h2"),
        QStringLiteral("h3"),    QStringLiteral("h4"),         QStringLiteral("h5"),     QStringLiteral("h6"),
        QStringLiteral("ul"),    QStringLiteral("ol"),         QStringLiteral("li"),     QStringLiteral("pre"),
        QStringLiteral("blockquote"), QStringLiteral("center"), QStringLiteral("table"), QStringLiteral("tr"),
        QStringLiteral("hr"),    QStringLiteral("body"),       QStringLiteral("html"),   QStringLiteral("dl"),
        QStringLiteral("dt"),    QStringLiteral("dd"),         QStringLiteral("address"), QStringLiteral("tbody"),
        QStringLiteral("thead"), QStringLiteral("tfoot"),      QStringLiteral("caption"), QStringLiteral("title"),
        QStringLiteral("head"),
    };
    return names.contains(name);
}

// What an element changes for the text inside it, and what it was before.
struct Scope {
    QString name;
    QTextCharFormat charFormat;
    QTextBlockFormat blockFormat;
    int listStyle = 0;
    int listLevel = -1;
    int listCounter = 0;
    bool pre = false;
    bool skip = false;  // the text is not part of the document: style, script, head
    bool title = false; // the text is the title
    QStringList classes;
};

// Whether a simple selector, such as "pre", ".user" or "code.inline", matches an element.
bool selectorMatches(const QString &simple, const QString &name, const QStringList &classes) {
    const QStringList pieces = simple.split(QChar('.'));
    if (!pieces.first().isEmpty() && pieces.first() != QStringLiteral("*") && pieces.first().toLower() != name)
        return false;
    for (qsizetype i = 1; i < pieces.size(); ++i)
        if (!classes.contains(pieces.at(i)))
            return false;
    return true;
}

} // namespace

void QTextDocumentPrivate::parseHtml(const QString &html) {
    title.clear();
    setBlocks(htmlBlocks(html));
}

std::vector<Block> QTextDocumentPrivate::htmlBlocks(const QString &html) {
    std::vector<Block> out;
    const qreal baseSize = font.pointSizeF() > 0 ? font.pointSizeF() : 10.0;

    std::vector<Scope> scopes(1);
    std::vector<std::size_t> openTables; // the tables being read, innermost last
    parsedTables.clear();
    Block current;
    bool blockOpen = false;    // current is the block of an element that was opened
    int counters[16] = {0};

    const auto beginBlock = [&](const Scope &scope) {
        current = Block();
        current.format = scope.blockFormat;
        current.listLevel = scope.listLevel + 1; // 0 outside a list
        blockOpen = true;
    };
    // Takes the space at the end of the text away, which white space of the HTML put there.
    const auto trimSpace = [&] {
        if (current.collapsed && current.text.endsWith(QChar(' '))) {
            current.text.chop(1);
            const int size = static_cast<int>(current.text.size());
            for (qsizetype i = current.formats.size() - 1; i >= 0; --i) {
                QTextLayout::FormatRange &range = current.formats[i];
                range.length = std::min(range.length, size - range.start);
                if (range.length <= 0)
                    current.formats.removeAt(i);
            }
        }
        current.collapsed = false;
    };
    const auto flushBlock = [&](bool force) {
        // The space at the end of a paragraph does not show; neither does a line break at its very end.
        trimSpace();
        if (!current.text.isEmpty() || current.rule || force) {
            for (QTextLayout::FormatRange &range : current.formats)
                range.length = std::min(range.length, static_cast<int>(current.text.size()) - range.start);
            out.push_back(std::move(current));
        }
        current = Block();
        blockOpen = false;
    };
    const auto appendText = [&](const QString &text, const QTextCharFormat &format) {
        if (text.isEmpty())
            return;
        if (!blockOpen)
            beginBlock(scopes.back());
        const int start = static_cast<int>(current.text.size());
        current.text.append(text);
        if (format.isEmpty())
            return;
        if (!current.formats.isEmpty()) {
            QTextLayout::FormatRange &last = current.formats.last();
            if (last.start + last.length == start && last.format == format) {
                last.length += static_cast<int>(text.size());
                return;
            }
        }
        QTextLayout::FormatRange range;
        range.start = start;
        range.length = static_cast<int>(text.size());
        range.format = format;
        current.formats.append(range);
    };
    const auto applyStyle = [&](Scope &scope, const QString &style, bool block) {
        for (const QString &declaration : style.split(QChar(';'), Qt::SkipEmptyParts)) {
            const qsizetype colon = declaration.indexOf(QChar(':'));
            if (colon < 0)
                continue;
            const QString property = declaration.left(colon).trimmed().toLower();
            const QString value = declaration.mid(colon + 1).trimmed();
            const QString lower = value.toLower();
            if (property == QStringLiteral("color")) {
                const QColor color = cssColor(value);
                if (color.isValid())
                    scope.charFormat.setForeground(QBrush(color));
            } else if (property == QStringLiteral("background-color") || property == QStringLiteral("background")) {
                const QColor color = cssColor(value);
                if (color.isValid())
                    scope.charFormat.setBackground(QBrush(color));
            } else if (property == QStringLiteral("font-weight")) {
                bool ok = false;
                const int number = lower.toInt(&ok);
                if (lower == QStringLiteral("bold") || lower == QStringLiteral("bolder") || (ok && number >= 600))
                    scope.charFormat.setFontWeight(QFont::Bold);
                else if (lower == QStringLiteral("normal") || lower == QStringLiteral("lighter") || ok)
                    scope.charFormat.setFontWeight(QFont::Normal);
            } else if (property == QStringLiteral("font-style")) {
                scope.charFormat.setFontItalic(lower == QStringLiteral("italic") || lower == QStringLiteral("oblique"));
            } else if (property == QStringLiteral("text-decoration")) {
                scope.charFormat.setFontUnderline(lower.contains(QStringLiteral("underline")));
                scope.charFormat.setFontStrikeOut(lower.contains(QStringLiteral("line-through")));
            } else if (property == QStringLiteral("font-size")) {
                const qreal now = scope.charFormat.fontPointSize() > 0 ? scope.charFormat.fontPointSize() : baseSize;
                const qreal size = cssFontSize(value, now);
                if (size > 0)
                    scope.charFormat.setFontPointSize(size);
            } else if (property == QStringLiteral("font-family")) {
                QStringList families;
                for (const QString &family : value.split(QChar(','), Qt::SkipEmptyParts)) {
                    QString name = family.trimmed();
                    if (name.size() >= 2 && (name.startsWith(QChar('\'')) || name.startsWith(QChar('"'))))
                        name = name.mid(1, name.size() - 2);
                    if (!name.isEmpty())
                        families.append(name);
                }
                if (!families.isEmpty())
                    scope.charFormat.setFontFamilies(families);
            } else if (property == QStringLiteral("white-space")) {
                scope.pre = lower.startsWith(QStringLiteral("pre"));
            } else if (block && property == QStringLiteral("text-align")) {
                bool ok = false;
                const Qt::Alignment alignment = alignmentOf(value, &ok);
                if (ok)
                    scope.blockFormat.setAlignment(alignment);
            } else if (block && property.startsWith(QStringLiteral("margin"))) {
                const qreal length = cssLength(value);
                if (length < 0)
                    continue;
                if (property == QStringLiteral("margin-top") || property == QStringLiteral("margin"))
                    scope.blockFormat.setTopMargin(length);
                if (property == QStringLiteral("margin-bottom") || property == QStringLiteral("margin"))
                    scope.blockFormat.setBottomMargin(length);
                if (property == QStringLiteral("margin-left") || property == QStringLiteral("margin"))
                    scope.blockFormat.setLeftMargin(length);
                if (property == QStringLiteral("margin-right") || property == QStringLiteral("margin"))
                    scope.blockFormat.setRightMargin(length);
            } else if (block && property == QStringLiteral("text-indent")) {
                const qreal length = cssLength(value);
                if (length >= 0)
                    scope.blockFormat.setTextIndent(length);
            }
        }
    };

    const qsizetype n = html.size();
    qsizetype pos = 0;
    while (pos < n) {
        const QChar c = html.at(pos);
        if (c == QChar('<')) {
            if (html.mid(pos, 4) == QStringLiteral("<!--")) {
                const qsizetype end = html.indexOf(QStringLiteral("-->"), pos + 4);
                pos = end < 0 ? n : end + 3;
                continue;
            }
            if (pos + 1 < n && (html.at(pos + 1) == QChar('!') || html.at(pos + 1) == QChar('?'))) {
                const qsizetype end = html.indexOf(QChar('>'), pos);
                pos = end < 0 ? n : end + 1;
                continue;
            }
            Tag tag;
            if (readTag(html, pos, tag)) {
                const QString &name = tag.name;
                if (tag.closing) {
                    // Close the innermost element of that name, and with it what was left open inside it.
                    qsizetype match = -1;
                    for (qsizetype i = static_cast<qsizetype>(scopes.size()) - 1; i > 0; --i) {
                        if (scopes[static_cast<std::size_t>(i)].name == name) {
                            match = i;
                            break;
                        }
                    }
                    if (match > 0) {
                        if (isBlockElement(name))
                            flushBlock(false);
                        scopes.resize(static_cast<std::size_t>(match));
                        if (name == QStringLiteral("table") && !openTables.empty()) {
                            parsedTables[openTables.back()].last = static_cast<int>(out.size()) - 1;
                            openTables.pop_back();
                        }
                    }
                    continue;
                }
                if (name == QStringLiteral("br")) {
                    if (!scopes.back().skip) {
                        trimSpace();
                        appendText(QString(QChar(0x2028)), QTextCharFormat());
                    }
                    continue;
                }
                if (name == QStringLiteral("hr")) {
                    flushBlock(false);
                    current = Block();
                    current.rule = true;
                    current.format.setTopMargin(4);
                    current.format.setBottomMargin(4);
                    out.push_back(std::move(current));
                    current = Block();
                    continue;
                }
                if (name == QStringLiteral("img")) {
                    const QString alt = tag.attributes.value(QStringLiteral("alt"));
                    if (!alt.isEmpty() && !scopes.back().skip && !scopes.back().title) {
                        current.collapsed = false;
                        appendText(alt, scopes.back().charFormat);
                    }
                    continue;
                }
                if (name == QStringLiteral("meta") || name == QStringLiteral("link") || name == QStringLiteral("input") ||
                    name == QStringLiteral("col") || name == QStringLiteral("base"))
                    continue;
                // A paragraph, a list item and a table row end when the next one starts.
                if (name == QStringLiteral("p") || name == QStringLiteral("li") || name == QStringLiteral("tr") ||
                    name == QStringLiteral("dt") || name == QStringLiteral("dd")) {
                    for (qsizetype i = static_cast<qsizetype>(scopes.size()) - 1; i > 0; --i) {
                        const QString &open = scopes[static_cast<std::size_t>(i)].name;
                        if (open == name || (name != QStringLiteral("p") && open == QStringLiteral("p"))) {
                            flushBlock(false);
                            scopes.resize(static_cast<std::size_t>(i));
                            break;
                        }
                        if (open == QStringLiteral("ul") || open == QStringLiteral("ol") || open == QStringLiteral("table") ||
                            open == QStringLiteral("div") || open == QStringLiteral("blockquote"))
                            break;
                    }
                }
                Scope scope = scopes.back();
                scope.name = name;
                const bool block = isBlockElement(name);
                if (block) {
                    flushBlock(false);
                    // The margins are those of the element itself, not of what it is in.
                    scope.blockFormat.clearProperty(QTextFormat::BlockTopMargin);
                    scope.blockFormat.clearProperty(QTextFormat::BlockBottomMargin);
                    scope.blockFormat.clearProperty(QTextFormat::HeadingLevel);
                }
                if (name == QStringLiteral("table")) {
                    TableInfo info;
                    info.first = static_cast<int>(out.size());
                    bool ok = false;
                    const qreal border = tag.attributes.value(QStringLiteral("border")).toDouble(&ok);
                    info.format.setBorder(ok ? border : 0); // HTML tables have no border unless they say so
                    const qreal padding = tag.attributes.value(QStringLiteral("cellpadding")).toDouble(&ok);
                    if (ok)
                        info.format.setCellPadding(padding);
                    const qreal spacing = tag.attributes.value(QStringLiteral("cellspacing")).toDouble(&ok);
                    if (ok)
                        info.format.setCellSpacing(spacing);
                    openTables.push_back(parsedTables.size());
                    parsedTables.push_back(info);
                } else if (name == QStringLiteral("tr") && !openTables.empty()) {
                    TableInfo &info = parsedTables[openTables.back()];
                    ++info.rows;
                    info.column = 0;
                }
                if (name == QStringLiteral("b") || name == QStringLiteral("strong") || name == QStringLiteral("th")) {
                    scope.charFormat.setFontWeight(QFont::Bold);
                } else if (name == QStringLiteral("i") || name == QStringLiteral("em") || name == QStringLiteral("cite") ||
                           name == QStringLiteral("var") || name == QStringLiteral("dfn") || name == QStringLiteral("address")) {
                    scope.charFormat.setFontItalic(true);
                } else if (name == QStringLiteral("u") || name == QStringLiteral("ins")) {
                    scope.charFormat.setFontUnderline(true);
                } else if (name == QStringLiteral("s") || name == QStringLiteral("strike") || name == QStringLiteral("del")) {
                    scope.charFormat.setFontStrikeOut(true);
                } else if (name == QStringLiteral("tt") || name == QStringLiteral("code") || name == QStringLiteral("kbd") ||
                           name == QStringLiteral("samp")) {
                    scope.charFormat.setFontFixedPitch(true);
                } else if (name == QStringLiteral("big") || name == QStringLiteral("small")) {
                    const qreal now = scope.charFormat.fontPointSize() > 0 ? scope.charFormat.fontPointSize() : baseSize;
                    scope.charFormat.setFontPointSize(now * (name == QStringLiteral("big") ? 1.2 : 0.83));
                } else if (name == QStringLiteral("a")) {
                    const QString href = tag.attributes.value(QStringLiteral("href"));
                    if (!href.isEmpty()) {
                        scope.charFormat.setAnchor(true);
                        scope.charFormat.setAnchorHref(href);
                        scope.charFormat.setFontUnderline(true);
                        scope.charFormat.setForeground(QBrush(QColor(0, 0, 255)));
                    }
                } else if (name == QStringLiteral("font")) {
                    const QColor color = cssColor(tag.attributes.value(QStringLiteral("color")));
                    if (color.isValid())
                        scope.charFormat.setForeground(QBrush(color));
                    const QString face = tag.attributes.value(QStringLiteral("face"));
                    if (!face.isEmpty())
                        scope.charFormat.setFontFamilies(QStringList{face.split(QChar(',')).first().trimmed()});
                    const QString size = tag.attributes.value(QStringLiteral("size")).trimmed();
                    if (!size.isEmpty()) {
                        // 1 to 7, with 3 as the size of the text around; +n and -n are steps from it.
                        static const qreal factors[] = {0.6, 0.75, 0.89, 1.0, 1.2, 1.5, 2.0, 3.0};
                        bool ok = false;
                        int step = size.toInt(&ok);
                        if (ok) {
                            if (size.startsWith(QChar('+')) || size.startsWith(QChar('-')))
                                step += 3;
                            scope.charFormat.setFontPointSize(baseSize * factors[std::clamp(step, 1, 7)]);
                        }
                    }
                } else if (name.size() == 2 && name.at(0) == QChar('h') && name.at(1) >= QChar('1') && name.at(1) <= QChar('6')) {
                    static const qreal factors[] = {2.0, 1.5, 1.17, 1.0, 0.83, 0.67};
                    const int level = name.at(1).unicode() - '0';
                    scope.charFormat.setFontWeight(QFont::Bold);
                    scope.charFormat.setFontPointSize(baseSize * factors[level - 1]);
                    scope.blockFormat.setHeadingLevel(level);
                    scope.blockFormat.setTopMargin(level <= 2 ? 14 : 12);
                    scope.blockFormat.setBottomMargin(level <= 2 ? 10 : 8);
                } else if (name == QStringLiteral("p")) {
                    scope.blockFormat.setTopMargin(scope.listLevel >= 0 ? 0 : 12);
                    scope.blockFormat.setBottomMargin(scope.listLevel >= 0 ? 0 : 12);
                } else if (name == QStringLiteral("pre")) {
                    scope.pre = true;
                    scope.charFormat.setFontFixedPitch(true);
                    scope.blockFormat.setTopMargin(12);
                    scope.blockFormat.setBottomMargin(12);
                } else if (name == QStringLiteral("blockquote") || name == QStringLiteral("dd")) {
                    scope.blockFormat.setIndent(scope.blockFormat.indent() + 1);
                    if (name == QStringLiteral("blockquote")) {
                        scope.blockFormat.setTopMargin(12);
                        scope.blockFormat.setBottomMargin(12);
                    }
                } else if (name == QStringLiteral("center")) {
                    scope.blockFormat.setAlignment(Qt::AlignHCenter);
                } else if (name == QStringLiteral("ul") || name == QStringLiteral("ol")) {
                    scope.listLevel = std::min(scope.listLevel + 1, 15);
                    counters[scope.listLevel] = 0;
                    if (name == QStringLiteral("ul")) {
                        static const int styles[] = {QTextListFormat::ListDisc, QTextListFormat::ListCircle,
                                                     QTextListFormat::ListSquare};
                        scope.listStyle = styles[std::min(scope.listLevel, 2)];
                    } else {
                        const QString type = tag.attributes.value(QStringLiteral("type"));
                        scope.listStyle = type == QStringLiteral("a")   ? QTextListFormat::ListLowerAlpha
                                          : type == QStringLiteral("A") ? QTextListFormat::ListUpperAlpha
                                                                        : QTextListFormat::ListDecimal;
                        bool ok = false;
                        const int start = tag.attributes.value(QStringLiteral("start")).toInt(&ok);
                        if (ok)
                            counters[scope.listLevel] = start - 1;
                    }
                } else if (name == QStringLiteral("head") || name == QStringLiteral("style") ||
                           name == QStringLiteral("script")) {
                    scope.skip = true;
                } else if (name == QStringLiteral("title")) {
                    scope.title = true;
                }
                if (name == QStringLiteral("td") || name == QStringLiteral("th")) {
                    if (!openTables.empty()) {
                        TableInfo &info = parsedTables[openTables.back()];
                        info.columns = std::max(info.columns, ++info.column);
                        if (name == QStringLiteral("th") && info.rows <= 1)
                            info.format.setHeaderRowCount(1);
                    }
                    // The cells of a row are parted by tabs.
                    trimSpace();
                    if (blockOpen && !current.text.isEmpty())
                        appendText(QStringLiteral("\t"), QTextCharFormat());
                }
                bool alignOk = false;
                const Qt::Alignment alignment = alignmentOf(tag.attributes.value(QStringLiteral("align")), &alignOk);
                if (alignOk && block)
                    scope.blockFormat.setAlignment(alignment);
                scope.classes = tag.attributes.value(QStringLiteral("class")).split(QChar(' '), Qt::SkipEmptyParts);
                // The rules of the default style sheet whose selector matches, then the style attribute.
                for (const StyleRule &rule : styleRules) {
                    if (!selectorMatches(rule.parts.last(), name, scope.classes))
                        continue;
                    qsizetype at = static_cast<qsizetype>(scopes.size()) - 1;
                    bool ok = true;
                    for (qsizetype p = rule.parts.size() - 2; p >= 0 && ok; --p) {
                        while (at > 0 && !selectorMatches(rule.parts.at(p), scopes[static_cast<std::size_t>(at)].name,
                                                          scopes[static_cast<std::size_t>(at)].classes))
                            --at;
                        ok = at > 0;
                        --at;
                    }
                    if (ok)
                        applyStyle(scope, rule.declarations, block);
                }
                const QString style = tag.attributes.value(QStringLiteral("style"));
                if (!style.isEmpty())
                    applyStyle(scope, style, block);
                if (block && !scope.skip && !scope.title && name != QStringLiteral("ul") && name != QStringLiteral("ol") &&
                    name != QStringLiteral("table") && name != QStringLiteral("html") && name != QStringLiteral("body") &&
                    name != QStringLiteral("tbody") && name != QStringLiteral("thead") && name != QStringLiteral("tfoot") &&
                    name != QStringLiteral("dl")) {
                    beginBlock(scope);
                    if (name == QStringLiteral("li") && scope.listLevel >= 0) {
                        current.listStyle = scope.listStyle;
                        current.listNumber = ++counters[scope.listLevel];
                    }
                    blockOpen = name == QStringLiteral("p") || name == QStringLiteral("li") || name == QStringLiteral("pre");
                    // Only these keep a block that stays empty; the others make one when text comes.
                    if (!blockOpen)
                        current.format = scope.blockFormat;
                }
                if (!tag.selfClosing)
                    scopes.push_back(scope);
                continue;
            }
            // A '<' that starts no tag is text.
        }
        // Text up to the next tag.
        qsizetype end = html.indexOf(QChar('<'), pos + 1);
        if (end < 0)
            end = n;
        const QString raw = decodeEntities(html.mid(pos, end - pos));
        pos = end;
        const Scope &scope = scopes.back();
        if (scope.title) {
            title += raw.simplified();
            continue;
        }
        if (scope.skip)
            continue;
        if (scope.pre) {
            QString text = raw;
            text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
            // The line break right after <pre> is not part of the text.
            if (current.text.isEmpty() && text.startsWith(QChar('\n')))
                text.remove(0, 1);
            text.replace(QChar('\n'), QChar(0x2028));
            current.collapsed = false;
            appendText(text, scope.charFormat);
            continue;
        }
        // Runs of white space are one space; none at the start of a paragraph or after a line break.
        QString word;
        const auto flushWord = [&] {
            if (word.isEmpty())
                return;
            current.collapsed = false;
            appendText(word, scope.charFormat);
            word.clear();
        };
        for (const QChar ch : raw) {
            if (ch != QChar(' ') && ch != QChar('\n') && ch != QChar('\t') && ch != QChar('\r') && ch != QChar('\f')) {
                word.append(ch);
                continue;
            }
            flushWord();
            if (!blockOpen || current.text.isEmpty())
                continue;
            const QChar last = current.text.at(current.text.size() - 1);
            if (last == QChar(' ') || last == QChar(0x2028) || last == QChar('\t'))
                continue;
            appendText(QStringLiteral(" "), scope.charFormat);
            current.collapsed = true;
        }
        flushWord();
    }
    flushBlock(false);
    for (std::size_t open : openTables) // tables that were not closed end with the document
        parsedTables[open].last = static_cast<int>(out.size()) - 1;
    // A table without rows is dropped; the rows of the others know their table.
    std::vector<TableInfo> kept;
    for (TableInfo &info : parsedTables) {
        if (info.last < info.first || info.rows == 0)
            continue;
        info.format.setColumns(info.columns);
        for (int b = info.first; b <= info.last; ++b)
            out[static_cast<std::size_t>(b)].table = static_cast<int>(kept.size());
        kept.push_back(info);
    }
    parsedTables = std::move(kept);
    return out;
}

int QTextDocumentPrivate::replace(int from, int to, const QString &fragment, bool isHtml) {
    if (from > to)
        std::swap(from, to);
    // The fragment as blocks, with its tables numbered after the tables of the document.
    std::vector<Block> pieces;
    std::vector<TableInfo> pieceTables;
    if (isHtml) {
        const QString keptTitle = title;
        pieces = htmlBlocks(fragment);
        title = keptTitle;
        pieceTables = std::move(parsedTables);
        parsedTables.clear();
        for (Block &b : pieces)
            if (b.table >= 0)
                b.table += static_cast<int>(tables.size());
    } else {
        QString text = fragment;
        text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        for (const QString &line : text.split(QChar('\n'))) {
            Block b;
            b.text = line;
            pieces.push_back(std::move(b));
        }
    }
    // Where the range starts and ends: a block and a place in it.
    const auto locate = [this](int pos, std::size_t *block, int *offset) {
        int start = 0;
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            const int length = static_cast<int>(blocks[i].text.size());
            if (pos <= start + length || i + 1 == blocks.size()) {
                *block = i;
                *offset = std::clamp(pos - start, 0, length);
                return;
            }
            start += length + 1;
        }
    };
    std::size_t first = 0, last = 0;
    int firstOffset = 0, lastOffset = 0;
    locate(std::max(0, from), &first, &firstOffset);
    locate(std::max(0, to), &last, &lastOffset);
    // The part of a block before or after a place, with its formats.
    const auto cut = [](const Block &b, int start, int end) {
        Block part;
        part.format = b.format;
        part.listStyle = b.listStyle;
        part.listNumber = b.listNumber;
        part.listLevel = b.listLevel;
        part.table = b.table;
        part.text = b.text.mid(start, end - start);
        for (QTextLayout::FormatRange range : b.formats) {
            const int s0 = std::max(range.start, start), e0 = std::min(range.start + range.length, end);
            if (e0 <= s0)
                continue;
            range.start = s0 - start;
            range.length = e0 - s0;
            part.formats.append(range);
        }
        return part;
    };
    const auto append = [](Block &target, const Block &more) {
        const int shift = static_cast<int>(target.text.size());
        target.text += more.text;
        for (QTextLayout::FormatRange range : more.formats) {
            range.start += shift;
            target.formats.append(range);
        }
    };
    Block left = cut(blocks[first], 0, firstOffset);
    const Block right = cut(blocks[last], lastOffset, static_cast<int>(blocks[last].text.size()));
    std::vector<Block> middle;
    if (pieces.empty()) {
        append(left, right);
        middle.push_back(std::move(left));
    } else {
        // The first piece goes on in the block where the range starts; one that starts an empty block brings its
        // own paragraph format.
        const bool startsEmpty = left.text.isEmpty();
        Block head = startsEmpty ? cut(pieces.front(), 0, 0) : std::move(left);
        append(head, pieces.front());
        middle.push_back(std::move(head));
        for (std::size_t i = 1; i < pieces.size(); ++i)
            middle.push_back(std::move(pieces[i]));
        append(middle.back(), right);
    }
    int end = 0; // the position after the fragment
    for (std::size_t i = 0; i < first; ++i)
        end += static_cast<int>(blocks[i].text.size()) + 1;
    for (std::size_t i = 0; i + 1 < middle.size(); ++i)
        end += static_cast<int>(middle[i].text.size()) + 1;
    end += static_cast<int>(middle.back().text.size() - right.text.size());
    std::vector<Block> fresh;
    fresh.reserve(blocks.size() - (last - first + 1) + middle.size());
    for (std::size_t i = 0; i < first; ++i)
        fresh.push_back(std::move(blocks[i]));
    for (Block &b : middle)
        fresh.push_back(std::move(b));
    for (std::size_t i = last + 1; i < blocks.size(); ++i)
        fresh.push_back(std::move(blocks[i]));
    // The tables that keep rows, numbered again.
    std::vector<TableInfo> all = tables;
    all.insert(all.end(), pieceTables.begin(), pieceTables.end());
    std::vector<int> renumber(all.size(), -1);
    std::vector<TableInfo> kept;
    for (std::size_t i = 0; i < fresh.size(); ++i) {
        const int t = fresh[i].table;
        if (t < 0 || t >= static_cast<int>(all.size())) {
            fresh[i].table = -1;
            continue;
        }
        int &n = renumber[static_cast<std::size_t>(t)];
        if (n < 0) {
            n = static_cast<int>(kept.size());
            kept.push_back(all[static_cast<std::size_t>(t)]);
            kept.back().first = static_cast<int>(i);
        }
        kept[static_cast<std::size_t>(n)].last = static_cast<int>(i);
        fresh[i].table = n;
    }
    parsedTables = std::move(kept);
    setBlocks(std::move(fresh));
    return end;
}

// ---------------------------------------------------------------------------------------------------------
// HTML out.

QString QTextDocumentPrivate::blockHtml(const Block &block) const {
    // The pieces of the text between the places where a format starts or ends.
    const int n = static_cast<int>(block.text.size());
    std::vector<int> cuts{0, n};
    for (const QTextLayout::FormatRange &range : block.formats) {
        cuts.push_back(std::clamp(range.start, 0, n));
        cuts.push_back(std::clamp(range.start + range.length, 0, n));
    }
    std::sort(cuts.begin(), cuts.end());
    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
    QString html;
    for (std::size_t i = 0; i + 1 < cuts.size(); ++i) {
        QTextCharFormat format;
        for (const QTextLayout::FormatRange &range : block.formats) {
            if (range.start <= cuts[i] && range.start + range.length >= cuts[i + 1])
                format.merge(range.format);
        }
        QString text = block.text.mid(cuts[i], cuts[i + 1] - cuts[i]).toHtmlEscaped();
        text.replace(QChar(0x2028), QStringLiteral("<br/>"));
        QStringList style;
        const QStringList families = format.fontFamilies();
        if (!families.isEmpty())
            style << QStringLiteral("font-family:'") + families.first() + QChar('\'');
        else if (format.fontFixedPitch())
            style << QStringLiteral("font-family:'monospace'");
        if (format.hasProperty(QTextFormat::FontPointSize) && format.fontPointSize() > 0)
            style << QStringLiteral("font-size:") + QString::number(format.fontPointSize()) + QStringLiteral("pt");
        if (format.hasProperty(QTextFormat::FontWeight))
            style << QStringLiteral("font-weight:") + QString::number(format.fontWeight());
        if (format.fontItalic())
            style << QStringLiteral("font-style:italic");
        if (format.fontUnderline() || format.fontStrikeOut()) {
            QStringList decoration;
            if (format.fontUnderline())
                decoration << QStringLiteral("underline");
            if (format.fontStrikeOut())
                decoration << QStringLiteral("line-through");
            style << QStringLiteral("text-decoration:") + decoration.join(QChar(' '));
        }
        if (format.foreground().style() != Qt::NoBrush)
            style << QStringLiteral("color:") + format.foreground().color().name();
        if (format.background().style() != Qt::NoBrush)
            style << QStringLiteral("background-color:") + format.background().color().name();
        if (!style.isEmpty())
            text = QStringLiteral("<span style=\"") + style.join(QStringLiteral("; ")) + QStringLiteral("\">") + text +
                   QStringLiteral("</span>");
        if (format.isAnchor())
            text = QStringLiteral("<a href=\"") + format.anchorHref().toHtmlEscaped() + QStringLiteral("\">") + text +
                   QStringLiteral("</a>");
        html += text;
    }
    return html;
}

// ---------------------------------------------------------------------------------------------------------

/*!
    Returns \c true if \a text looks like rich text: if its first line
    has a tag of HTML. A QLabel uses this to decide how to show a text
    whose format it was not told.

    \sa convertFromPlainText()
*/
bool Qt::mightBeRichText(const QString &text) {
    if (text.isEmpty())
        return false;
    qsizetype start = 0;
    while (start < text.size() && text.at(start).isSpace())
        ++start;
    if (text.mid(start, 5) == QStringLiteral("<?xml") || text.mid(start, 2) == QStringLiteral("<!"))
        return true;
    static const QStringList known{
        QStringLiteral("a"),     QStringLiteral("b"),    QStringLiteral("big"),   QStringLiteral("blockquote"),
        QStringLiteral("body"),  QStringLiteral("br"),   QStringLiteral("center"), QStringLiteral("code"),
        QStringLiteral("div"),   QStringLiteral("em"),   QStringLiteral("font"),  QStringLiteral("h1"),
        QStringLiteral("h2"),    QStringLiteral("h3"),   QStringLiteral("h4"),    QStringLiteral("h5"),
        QStringLiteral("h6"),    QStringLiteral("hr"),   QStringLiteral("html"),  QStringLiteral("i"),
        QStringLiteral("img"),   QStringLiteral("li"),   QStringLiteral("ol"),    QStringLiteral("p"),
        QStringLiteral("pre"),   QStringLiteral("qt"),   QStringLiteral("s"),     QStringLiteral("small"),
        QStringLiteral("span"),  QStringLiteral("strong"), QStringLiteral("table"), QStringLiteral("tt"),
        QStringLiteral("u"),     QStringLiteral("ul"),
    };
    qsizetype lineEnd = text.indexOf(QChar('\n'), start);
    if (lineEnd < 0)
        lineEnd = text.size();
    for (qsizetype open = text.indexOf(QChar('<'), start); open >= 0 && open < lineEnd; open = text.indexOf(QChar('<'), open + 1)) {
        qsizetype i = open + 1;
        if (i < text.size() && text.at(i) == QChar('/'))
            ++i;
        const qsizetype nameStart = i;
        while (i < text.size() && text.at(i).isLetterOrNumber())
            ++i;
        if (i == nameStart || i >= text.size())
            continue;
        const QChar after = text.at(i);
        if ((after == QChar('>') || after == QChar('/') || after.isSpace()) &&
            known.contains(text.mid(nameStart, i - nameStart).toLower()) && text.indexOf(QChar('>'), i) >= 0)
            return true;
    }
    return false;
}

/*!
    Returns \a plain as HTML that shows the same text: the characters
    that mean something in HTML are escaped, and the line breaks are kept.
    With \a mode Qt::WhiteSpacePre the spaces are kept too.

    \sa mightBeRichText()
*/
QString Qt::convertFromPlainText(const QString &plain, Qt::WhiteSpaceMode mode) {
    QString html = QStringLiteral("<p>");
    int column = 0;
    for (qsizetype i = 0; i < plain.size(); ++i) {
        const QChar c = plain.at(i);
        if (c == QChar('\n')) {
            // An empty line parts paragraphs; a single line break is a line break.
            int breaks = 1;
            while (i + 1 < plain.size() && plain.at(i + 1) == QChar('\n')) {
                ++i;
                ++breaks;
            }
            if (breaks == 1) {
                html += QStringLiteral("<br>\n");
            } else {
                html += QStringLiteral("</p>\n");
                for (int b = 2; b < breaks; ++b)
                    html += QStringLiteral("<br>\n");
                html += QStringLiteral("<p>");
            }
            column = 0;
        } else if (mode == Qt::WhiteSpacePre && c == QChar('\t')) {
            html += QChar(0xa0);
            ++column;
            while (column % 8 != 0) {
                html += QChar(0xa0);
                ++column;
            }
        } else if (mode == Qt::WhiteSpacePre && c.isSpace()) {
            html += QChar(0xa0);
            ++column;
        } else {
            html += QString(c).toHtmlEscaped();
            ++column;
        }
    }
    return html + QStringLiteral("</p>");
}

#define NT_BLOCK(fallback)                                                                                             \
    const QTextDocumentPrivate *d = m_document ? QTextDocumentPrivate::get(m_document) : nullptr;                      \
    if (!d || m_index < 0 || m_index >= static_cast<int>(d->blocks.size()))                                            \
        return fallback;                                                                                               \
    const Block &block = d->blocks[static_cast<std::size_t>(m_index)]

/*!
    \fn bool QTextBlock::isVisible() const
    \since 6.0

    Returns \c true if the block is shown: every valid block is, as Nt does not hide blocks.
*/

/*!
    \fn bool QTextDocument::isModified() const
    \since 6.0

    Returns \c true if the document was modified since setModified(false). An editor such as QPlainTextEdit sets
    it when the user edits; the document does not set it itself.

    \sa setModified(), modificationChanged()
*/

/*!
    \since 6.0

    Marks the document as modified if \a m is \c true, and as saved otherwise, and emits modificationChanged() if
    that changes.

    \code
    if (save(editor->toPlainText()))
        editor->document()->setModified(false);
    \endcode
*/
void QTextDocument::setModified(bool m) {
    if (m_modified == m)
        return;
    m_modified = m;
    emit modificationChanged(m);
}

/*!
    Returns \c true if this is a block of a document; otherwise returns
    \c false.
*/
bool QTextBlock::isValid() const {
    NT_BLOCK(false);
    static_cast<void>(block);
    return true;
}

/*!
    Returns the text of the block, without formats. A line break inside
    the block is the character U+2028.
*/
QString QTextBlock::text() const {
    NT_BLOCK(QString());
    return block.text;
}

/*!
    Returns the position of the first character of the block in the
    document. Each block before it counts with its text and one more, for
    the paragraph separator.

    \sa length()
*/
int QTextBlock::position() const {
    NT_BLOCK(0);
    static_cast<void>(block);
    int position = 0;
    for (int i = 0; i < m_index; ++i)
        position += static_cast<int>(d->blocks[static_cast<std::size_t>(i)].text.size()) + 1;
    return position;
}

/*!
    Returns the length of the block: that of its text, and one for the
    paragraph separator.

    \sa position()
*/
int QTextBlock::length() const {
    NT_BLOCK(0);
    return static_cast<int>(block.text.size()) + 1;
}

/*!
    Returns \c true if the position \a position of the document is in
    this block; otherwise returns \c false.
*/
bool QTextBlock::contains(int position) const {
    const int start = this->position();
    return isValid() && position >= start && position < start + length();
}

/*!
    \fn int QTextBlock::blockNumber() const

    Returns the position of the block among the blocks of its document,
    counted from 0, or -1 for an invalid block.
*/

/*!
    Returns the format of the block as a paragraph: its alignment,
    margins, indentation and heading level.
*/
QTextBlockFormat QTextBlock::blockFormat() const {
    NT_BLOCK(QTextBlockFormat());
    return block.format;
}

/*!
    Returns the character format at the start of the block, which is
    empty if the text there has none of its own.

    \sa textFormats()
*/
QTextCharFormat QTextBlock::charFormat() const {
    NT_BLOCK(QTextCharFormat());
    QTextCharFormat format;
    for (const QTextLayout::FormatRange &range : block.formats) {
        if (range.start == 0 && range.length > 0)
            format.merge(range.format);
    }
    return format;
}

/*!
    Returns the character formats of the block: for each of them, the
    part of the text that it applies to.
*/
QList<QTextLayout::FormatRange> QTextBlock::textFormats() const {
    NT_BLOCK(QList<QTextLayout::FormatRange>());
    return block.formats;
}

/*!
    Returns the layout of the block, with its lines as the document laid
    them out for its text width, or \c nullptr for an invalid block. The
    position of the layout is that of the block in the document.
*/
QTextLayout *QTextBlock::layout() const {
    NT_BLOCK(nullptr);
    static_cast<void>(block);
    QTextDocumentPrivate::get(m_document)->ensureLayout();
    return block.layout.get();
}

#undef NT_BLOCK

/*!
    \fn const QTextDocument *QTextBlock::document() const

    Returns the document of the block, or \c nullptr.
*/

/*!
    Returns the block after this one, or an invalid block after the last.

    \sa previous()
*/
QTextBlock QTextBlock::next() const {
    if (!isValid() || m_index + 1 >= m_document->blockCount())
        return QTextBlock();
    return QTextBlock(m_document, m_index + 1);
}

/*!
    Returns the block before this one, or an invalid block before the
    first.

    \sa next()
*/
QTextBlock QTextBlock::previous() const {
    if (!isValid() || m_index == 0)
        return QTextBlock();
    return QTextBlock(m_document, m_index - 1);
}

/*!
    \class QAbstractTextDocumentLayout::PaintContext
    \brief The PaintContext struct says how a text document is drawn.

    \differences
    There are no selections. cursorPosition is kept but no cursor is drawn.
    \enddifferences
*/

/*!
    \variable QAbstractTextDocumentLayout::PaintContext::cursorPosition
    \brief The position of the cursor, or -1 for none.
*/

/*!
    \variable QAbstractTextDocumentLayout::PaintContext::palette
    \brief The palette; its QPalette::Text color is the color of text without one of its own.
*/

/*!
    \variable QAbstractTextDocumentLayout::PaintContext::clip
    \brief The rectangle to draw, in document coordinates; all of the document if null.
*/

/*!
    Draws the document with \a painter as \a context says.
*/
void QAbstractTextDocumentLayout::draw(QPainter *painter, const PaintContext &context) {
    if (!painter || !m_document)
        return;
    painter->save();
    painter->setPen(context.palette.color(QPalette::Text));
    m_document->drawContents(painter, context.clip);
    painter->restore();
}

/*!
    Returns the target of the link at the position \a pos of the
    document, or an empty string if there is no link there.

    \sa QTextCharFormat::anchorHref()
*/
QString QAbstractTextDocumentLayout::anchorAt(const QPointF &pos) const {
    QTextDocumentPrivate *d = QTextDocumentPrivate::get(m_document);
    d->ensureLayout();
    for (const Block &block : d->blocks) {
        if (pos.y() < block.y || pos.y() >= block.y + block.height || !block.layout)
            continue;
        const QTextLayout &layout = *block.layout;
        const QPointF local = pos - layout.position();
        for (int i = 0; i < layout.lineCount(); ++i) {
            const QTextLine line = layout.lineAt(i);
            const QRectF text = line.naturalTextRect();
            if (local.y() < line.y() || local.y() >= line.y() + line.height() || local.x() < text.x() ||
                local.x() >= text.x() + text.width())
                continue;
            // The character under the point: the one whose left edge is at or before it.
            int at = line.xToCursor(local.x());
            if (at > line.textStart() && line.cursorToX(at) > local.x())
                --at;
            for (qsizetype r = block.formats.size() - 1; r >= 0; --r) {
                const QTextLayout::FormatRange &range = block.formats.at(r);
                if (at >= range.start && at < range.start + range.length && range.format.isAnchor())
                    return range.format.anchorHref();
            }
            return QString();
        }
    }
    return QString();
}

/*!
    Returns the size of the document as it is laid out; the same as
    QTextDocument::size().
*/
QSizeF QAbstractTextDocumentLayout::documentSize() const { return m_document->size(); }

/*!
    Returns the rectangle that \a block takes in the document, or an
    empty rectangle if it is not a block of this document.
*/
QRectF QAbstractTextDocumentLayout::blockBoundingRect(const QTextBlock &block) const {
    QTextDocumentPrivate *d = QTextDocumentPrivate::get(m_document);
    if (block.document() != m_document || !block.isValid())
        return QRectF();
    d->ensureLayout();
    const Block &b = d->blocks[static_cast<std::size_t>(block.blockNumber())];
    return QRectF(b.left, b.y, std::max<qreal>(0, d->laidOutSize.width() - b.left - d->margin), b.height);
}

/*!
    \fn QTextDocument *QAbstractTextDocumentLayout::document() const

    Returns the document that this is the layout of.
*/

/*!
    Constructs an empty document with the given \a parent.
*/
QTextDocument::QTextDocument(QObject *parent) : QObject(parent), d(std::make_unique<QTextDocumentPrivate>(this)) {
    d->blocks.emplace_back();
}

/*!
    \overload

    Constructs a document with the plain text \a text and the given
    \a parent.
*/
QTextDocument::QTextDocument(const QString &text, QObject *parent) : QTextDocument(parent) { setPlainText(text); }

/*!
    Destroys the document.
*/
QTextDocument::~QTextDocument() = default;

/*!
    Returns \c true if the document has no text; otherwise returns
    \c false.
*/
bool QTextDocument::isEmpty() const {
    for (const Block &block : d->blocks) {
        if (!block.text.isEmpty() || block.rule)
            return false;
    }
    return d->blocks.size() <= 1;
}

/*!
    Removes the contents of the document. Its font, its text width and
    its margin stay.
*/
void QTextDocument::clear() {
    d->title.clear();
    d->setBlocks(std::vector<Block>());
}

/*!
    Makes \a text, without formats, the contents of the document: a
    paragraph for each of its lines.

    \sa toPlainText(), setHtml()
*/
void QTextDocument::setPlainText(const QString &text) {
    std::vector<Block> blocks;
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    for (const QString &line : normalized.split(QChar('\n'))) {
        Block block;
        block.text = line;
        blocks.push_back(std::move(block));
    }
    d->title.clear();
    d->setBlocks(std::move(blocks));
}

/*!
    Returns the text of the document without its formats, with a line
    break between paragraphs and for each line break inside one.

    \sa setPlainText(), toHtml()
*/
QString QTextDocument::toPlainText() const {
    QString text;
    for (std::size_t i = 0; i < d->blocks.size(); ++i) {
        if (i > 0)
            text.append(QChar('\n'));
        text.append(d->blocks[i].text);
    }
    text.replace(QChar(0x2028), QChar('\n'));
    return text;
}

/*!
    Makes the HTML in \a html the contents of the document. The
    description of the class lists what is understood; what is not is
    left out, and its text kept. Sizes that are relative, as those of
    headings, are taken of the default font as it is now: set the font
    first.

    \sa toHtml(), setPlainText(), setDefaultFont()
*/
void QTextDocument::setHtml(const QString &html) { d->parseHtml(html); }

/*!
    \since 6.0

    Makes the Markdown in \a markdown the contents of the document. With the default \a features, GitHub
    Flavored Markdown, tables, strikethrough and task lists are read as well.

    \code
    browser->document()->setMarkdown("# Help\n\n| Key | Action |\n|-----|--------|\n| F1  | Help   |\n");
    \endcode

    The Markdown becomes HTML, which is read as setHtml() reads it: headings, paragraphs, emphasis, code spans and
    blocks, block quotes, lists, links, images (as their text), rules and tables.

    \sa toMarkdown(), setHtml()
*/
void QTextDocument::setMarkdown(const QString &markdown, MarkdownFeatures features) {
    const bool github = (features & MarkdownDialectGitHub) == MarkdownDialectGitHub;
    const bool noHtml = (features & MarkdownNoHTML) == MarkdownNoHTML;
    d->parseHtml(QtPrivate::markdownToHtml(markdown, github, noHtml));
}

/*!
    \since 6.0

    Returns the contents of the document as Markdown: headings, lists, tables, bold, italic, code and links.
    \a features is accepted for compatibility.

    \sa setMarkdown()
*/
QString QTextDocument::toMarkdown(MarkdownFeatures features) const {
    (void)features;
    return d->markdown();
}

/*!
    \since 6.0

    Sets the style sheet that the HTML of setHtml(), and of QTextCursor::insertHtml(), is read with to \a sheet. Rules
    of element names, classes (\c{.user}, \c{div.note}) and descendants (\c{pre code}) set the properties that the
    \c style attribute can set; the \c style attribute of an element comes after them.

    \code
    document->setDefaultStyleSheet(".error { color: #ff6b6b; font-style: italic; }");
    document->setHtml("<p class=\"error\">Not connected</p>");
    \endcode

    The contents of the document are not read again: set the style sheet before the HTML.
*/
void QTextDocument::setDefaultStyleSheet(const QString &sheet) {
    d->styleSheet = sheet;
    d->styleRules.clear();
    QString css = sheet;
    // No comments.
    for (qsizetype start = css.indexOf(QStringLiteral("/*")); start >= 0; start = css.indexOf(QStringLiteral("/*"))) {
        const qsizetype end = css.indexOf(QStringLiteral("*/"), start + 2);
        css.remove(start, end < 0 ? css.size() - start : end + 2 - start);
    }
    for (const QString &rule : css.split(QChar('}'), Qt::SkipEmptyParts)) {
        const qsizetype brace = rule.indexOf(QChar('{'));
        if (brace < 0)
            continue;
        const QString declarations = rule.mid(brace + 1).trimmed();
        for (const QString &selector : rule.left(brace).split(QChar(','), Qt::SkipEmptyParts)) {
            const QStringList parts = selector.simplified().split(QChar(' '), Qt::SkipEmptyParts);
            if (!parts.isEmpty())
                d->styleRules.push_back({parts, declarations});
        }
    }
}

int QTextDocument::replaceRange(int from, int to, const QString &fragment, bool isHtml) {
    return d->replace(from, to, fragment, isHtml);
}

/*!
    \since 6.0

    Returns the default style sheet.
*/
QString QTextDocument::defaultStyleSheet() const { return d->styleSheet; }

/*!
    \since 6.0

    Returns the frame of the whole document. The tables of the document are its child frames.

    \sa QTextFrame::childFrames()
*/
QTextFrame *QTextDocument::rootFrame() const { return d->rootFrame(); }

/*!
    Returns the contents of the document as HTML that setHtml() reads
    back to the same text and formats.

    \sa setHtml(), toPlainText()
*/
QString QTextDocument::toHtml() const {
    QString html = QStringLiteral("<!DOCTYPE HTML>\n<html><head><meta charset=\"utf-8\"/>");
    if (!d->title.isEmpty())
        html += QStringLiteral("<title>") + d->title.toHtmlEscaped() + QStringLiteral("</title>");
    html += QStringLiteral("</head><body>\n");
    int openList = 0; // the style of the list that is open, or 0
    int openLevel = -1;
    const auto closeList = [&] {
        if (openList != 0)
            html += openList == QTextListFormat::ListDecimal || openList == QTextListFormat::ListLowerAlpha ||
                            openList == QTextListFormat::ListUpperAlpha
                        ? QStringLiteral("</ol>\n")
                        : QStringLiteral("</ul>\n");
        openList = 0;
        openLevel = -1;
    };
    for (const Block &block : d->blocks) {
        if (block.listStyle != openList || (block.listStyle != 0 && block.listLevel != openLevel))
            closeList();
        if (block.rule) {
            html += QStringLiteral("<hr/>\n");
            continue;
        }
        QStringList style;
        const QTextBlockFormat &format = block.format;
        if (format.alignment() & Qt::AlignHCenter)
            style << QStringLiteral("text-align:center");
        else if (format.alignment() & Qt::AlignRight)
            style << QStringLiteral("text-align:right");
        const int indent = format.indent() + (block.listStyle != 0 ? 0 : block.listLevel);
        if (format.hasProperty(QTextFormat::BlockTopMargin))
            style << QStringLiteral("margin-top:") + QString::number(format.topMargin()) + QStringLiteral("px");
        if (format.hasProperty(QTextFormat::BlockBottomMargin))
            style << QStringLiteral("margin-bottom:") + QString::number(format.bottomMargin()) + QStringLiteral("px");
        if (format.leftMargin() + indent * kIndentStep > 0)
            style << QStringLiteral("margin-left:") + QString::number(format.leftMargin() + indent * kIndentStep) + QStringLiteral("px");
        if (format.rightMargin() > 0)
            style << QStringLiteral("margin-right:") + QString::number(format.rightMargin()) + QStringLiteral("px");
        if (format.textIndent() > 0)
            style << QStringLiteral("text-indent:") + QString::number(format.textIndent()) + QStringLiteral("px");
        const QString attributes = style.isEmpty() ? QString() : QStringLiteral(" style=\"") + style.join(QStringLiteral("; ")) + QChar('"');
        if (block.listStyle != 0) {
            if (openList == 0) {
                const bool numbered = block.listStyle == QTextListFormat::ListDecimal ||
                                      block.listStyle == QTextListFormat::ListLowerAlpha ||
                                      block.listStyle == QTextListFormat::ListUpperAlpha;
                html += numbered ? QStringLiteral("<ol") : QStringLiteral("<ul");
                if (block.listStyle == QTextListFormat::ListLowerAlpha)
                    html += QStringLiteral(" type=\"a\"");
                else if (block.listStyle == QTextListFormat::ListUpperAlpha)
                    html += QStringLiteral(" type=\"A\"");
                if (numbered && block.listNumber != 1)
                    html += QStringLiteral(" start=\"") + QString::number(block.listNumber) + QChar('"');
                html += QStringLiteral(">\n");
                openList = block.listStyle;
                openLevel = block.listLevel;
            }
            html += QStringLiteral("<li") + attributes + QChar('>') + d->blockHtml(block) + QStringLiteral("</li>\n");
            continue;
        }
        const int level = format.headingLevel();
        const QString tag = level >= 1 && level <= 6 ? QStringLiteral("h") + QString::number(level) : QStringLiteral("p");
        html += QChar('<') + tag + attributes + QChar('>') + d->blockHtml(block) + QStringLiteral("</") + tag + QStringLiteral(">\n");
    }
    closeList();
    return html + QStringLiteral("</body></html>\n");
}

/*!
    Sets what the document knows about itself for \a info to \a string.

    \sa metaInformation()
*/
void QTextDocument::setMetaInformation(MetaInformation info, const QString &string) {
    if (info == DocumentTitle)
        d->title = string;
    else
        d->url = string;
}

/*!
    Returns what the document knows about itself for \a info: its title,
    which setHtml() reads from the \c title element, or its URL.

    \sa setMetaInformation()
*/
QString QTextDocument::metaInformation(MetaInformation info) const { return info == DocumentTitle ? d->title : d->url; }

/*!
    Sets the font of the text that has no font of its own to \a font.

    \sa defaultFont()
*/
void QTextDocument::setDefaultFont(const QFont &font) {
    d->font = font;
    d->laidOut = false;
}

/*!
    Returns the font of the text that has no font of its own.

    \sa setDefaultFont()
*/
QFont QTextDocument::defaultFont() const { return d->font; }

/*!
    Sets how the lines of the document are broken to what \a option says.
    The alignment of a paragraph comes from its block format, not from
    \a option.

    \sa defaultTextOption()
*/
void QTextDocument::setDefaultTextOption(const QTextOption &option) {
    d->option = option;
    d->laidOut = false;
}

/*!
    Returns the text option of the document.

    \sa setDefaultTextOption()
*/
QTextOption QTextDocument::defaultTextOption() const { return d->option; }

/*!
    Sets the width that the document is laid out for to \a width pixels:
    lines are broken to fit into it. With a width that is not positive,
    which is the default, lines are only broken where the text has line
    breaks, and the document is as wide as its longest line.

    \sa textWidth(), size(), idealWidth()
*/
void QTextDocument::setTextWidth(qreal width) {
    if (d->textWidth == width)
        return;
    d->textWidth = width;
    d->laidOut = false;
}

/*!
    Returns the width that the document is laid out for, or -1 if none
    is set.

    \sa setTextWidth()
*/
qreal QTextDocument::textWidth() const { return d->textWidth; }

/*!
    Returns the width that the document takes when no line is broken
    that does not have to be: that of its longest line, with the margins.

    \sa setTextWidth(), size()
*/
qreal QTextDocument::idealWidth() const {
    d->ensureLayout();
    qreal widest = 0;
    for (const Block &block : d->blocks) {
        if (block.layout)
            widest = std::max(widest, block.left - d->margin + block.format.textIndent() + block.layout->maximumWidth() +
                                          block.format.rightMargin());
    }
    return std::ceil(widest) + 2 * d->margin;
}

/*!
    Returns the size of the document as it is laid out: textWidth() wide,
    or as wide as its longest line if no text width is set, and as high
    as its lines and margins.

    \sa setTextWidth()
*/
QSizeF QTextDocument::size() const {
    d->ensureLayout();
    return d->laidOutSize;
}

/*!
    Returns the margin around the document, in pixels. The default is 4.

    \sa setDocumentMargin()
*/
qreal QTextDocument::documentMargin() const { return d->margin; }

/*!
    Sets the margin around the document to \a margin pixels.

    \sa documentMargin()
*/
void QTextDocument::setDocumentMargin(qreal margin) {
    d->margin = std::max<qreal>(0, margin);
    d->laidOut = false;
}

/*!
    Returns the number of blocks, that is of paragraphs, of the document.
    An empty document has one, which is empty.
*/
int QTextDocument::blockCount() const { return static_cast<int>(d->blocks.size()); }

/*!
    Returns the number of lines of the document as it is laid out for its
    text width.
*/
int QTextDocument::lineCount() const {
    d->ensureLayout();
    int lines = 0;
    for (const Block &block : d->blocks)
        lines += block.layout ? block.layout->lineCount() : 0;
    return lines;
}

/*!
    Returns the number of characters of the document: those of its
    blocks, and one for each block, for the paragraph separator.
*/
int QTextDocument::characterCount() const {
    int count = 0;
    for (const Block &block : d->blocks)
        count += static_cast<int>(block.text.size()) + 1;
    return count;
}

/*!
    Returns the character at the position \a pos of the document. The
    paragraph separator at the end of a block is U+2029. Returns a null
    character if \a pos is out of range.
*/
QChar QTextDocument::characterAt(int pos) const {
    if (pos < 0)
        return QChar();
    for (const Block &block : d->blocks) {
        const int length = static_cast<int>(block.text.size());
        if (pos < length)
            return block.text.at(pos);
        if (pos == length)
            return QChar(0x2029);
        pos -= length + 1;
    }
    return QChar();
}

/*!
    Returns the first block of the document.

    \sa end(), QTextBlock::next()
*/
QTextBlock QTextDocument::begin() const { return QTextBlock(this, 0); }

/*!
    Returns an invalid block, which is what QTextBlock::next() of the
    last block returns.

    \sa begin()
*/
QTextBlock QTextDocument::end() const { return QTextBlock(); }

/*!
    Returns the first block of the document.
*/
QTextBlock QTextDocument::firstBlock() const { return QTextBlock(this, 0); }

/*!
    Returns the last block of the document.
*/
QTextBlock QTextDocument::lastBlock() const { return QTextBlock(this, blockCount() - 1); }

/*!
    Returns the block that has the position \a pos of the document, or
    an invalid block if \a pos is out of range.

    \sa findBlockByNumber()
*/
QTextBlock QTextDocument::findBlock(int pos) const {
    if (pos < 0)
        return QTextBlock();
    int start = 0;
    for (std::size_t i = 0; i < d->blocks.size(); ++i) {
        const int length = static_cast<int>(d->blocks[i].text.size()) + 1;
        if (pos < start + length)
            return QTextBlock(this, static_cast<int>(i));
        start += length;
    }
    return QTextBlock();
}

/*!
    Returns the block at position \a blockNumber of the blocks, counted
    from 0, or an invalid block if there is none.

    \sa findBlock()
*/
QTextBlock QTextDocument::findBlockByNumber(int blockNumber) const {
    return blockNumber >= 0 && blockNumber < blockCount() ? QTextBlock(this, blockNumber) : QTextBlock();
}

/*!
    Draws the document with \a painter, with its top left corner at the
    origin of the painter. The text has the color of the pen of the
    painter where it has none of its own. With a valid \a rect, only the
    part of the document inside it is drawn.
*/
void QTextDocument::drawContents(QPainter *painter, const QRectF &rect) {
    if (!painter)
        return;
    d->ensureLayout();
    painter->save();
    const bool clip = rect.isValid();
    if (clip)
        painter->setClipRect(QRect(qRound(rect.x()), qRound(rect.y()), qRound(rect.width()), qRound(rect.height())));
    const QPen pen = painter->pen();
    for (const Block &block : d->blocks) {
        if (clip && (block.y + block.height < rect.y() || block.y > rect.y() + rect.height()))
            continue;
        if (block.rule) {
            const int y = qRound(block.y) + 4;
            painter->setPen(QColor(128, 128, 128));
            painter->drawLine(qRound(block.left), y, qRound(d->laidOutSize.width() - d->margin) - 1, y);
            painter->setPen(pen);
            continue;
        }
        if (!block.layout)
            continue;
        if (block.listStyle != 0 && block.layout->lineCount() > 0) {
            // The mark of a list item, right-aligned in the indentation before its first line.
            const QString mark = marker(block);
            const QFontMetrics metrics(d->font);
            const QTextLine first = block.layout->lineAt(0);
            painter->setFont(d->font);
            painter->setPen(pen);
            painter->drawText(QPoint(qRound(block.left) - metrics.horizontalAdvance(mark) - 6,
                                     qRound(block.y + first.y() + first.ascent())),
                              mark);
        }
        block.layout->draw(painter, QPointF(0, 0), QList<QTextLayout::FormatRange>(), rect);
    }
    // The borders of the tables: around them, between the rows and between the columns.
    for (const TableInfo &table : d->tables) {
        const qreal border = table.format.border();
        if (border <= 0 || table.rowTops.empty())
            continue;
        const QBrush brush = table.format.borderBrush();
        painter->setPen(QPen(brush.style() != Qt::NoBrush ? brush.color() : QColor(128, 128, 128), border));
        painter->setBrush(Qt::NoBrush);
        const qreal top = table.rowTops.front();
        const qreal width = table.cell * table.columns;
        painter->drawRect(QRectF(table.x, top, width, table.bottom - top));
        for (std::size_t i = 1; i < table.rowTops.size(); ++i)
            painter->drawLine(QPointF(table.x, table.rowTops[i]), QPointF(table.x + width, table.rowTops[i]));
        for (int c = 1; c < table.columns; ++c)
            painter->drawLine(QPointF(table.x + c * table.cell, top), QPointF(table.x + c * table.cell, table.bottom));
    }
    painter->restore();
}

/*!
    Returns the object that tells where things are in the document as it
    is laid out. The document owns it.
*/
QAbstractTextDocumentLayout *QTextDocument::documentLayout() const { return &d->layoutObject; }

// ---------------------------------------------------------------------------------------------------------
// Markdown out.

QString QTextDocumentPrivate::markdown() const {
    // The text of a block with its bold, italic, code and link formats as Markdown.
    const auto inlineMarkdown = [](const Block &block) {
        const int n = static_cast<int>(block.text.size());
        std::vector<int> cuts{0, n};
        for (const QTextLayout::FormatRange &range : block.formats) {
            cuts.push_back(std::clamp(range.start, 0, n));
            cuts.push_back(std::clamp(range.start + range.length, 0, n));
        }
        std::sort(cuts.begin(), cuts.end());
        cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
        QString out;
        for (std::size_t i = 0; i + 1 < cuts.size(); ++i) {
            QTextCharFormat format;
            for (const QTextLayout::FormatRange &range : block.formats)
                if (range.start <= cuts[i] && range.start + range.length >= cuts[i + 1])
                    format.merge(range.format);
            QString text = block.text.mid(cuts[i], cuts[i + 1] - cuts[i]);
            text.replace(QChar(0x2028), QStringLiteral("  \n"));
            if (format.fontFixedPitch())
                text = QChar('`') + text + QChar('`');
            if (format.fontItalic())
                text = QChar('*') + text + QChar('*');
            if (format.fontWeight() >= QFont::Bold && block.format.headingLevel() == 0 && block.table < 0)
                text = QStringLiteral("**") + text + QStringLiteral("**");
            if (format.fontStrikeOut())
                text = QStringLiteral("~~") + text + QStringLiteral("~~");
            if (format.isAnchor())
                text = QChar('[') + text + QStringLiteral("](") + format.anchorHref() + QChar(')');
            out += text;
        }
        return out;
    };
    QString md;
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const Block &block = blocks[i];
        if (block.rule) {
            md += QStringLiteral("- - -\n\n");
            continue;
        }
        if (block.table >= 0) {
            const TableInfo &table = tables[static_cast<std::size_t>(block.table)];
            QStringList cells = inlineMarkdown(block).split(QChar('\t'));
            while (cells.size() < table.columns)
                cells.append(QString());
            md += QStringLiteral("|") + cells.join(QChar('|')) + QStringLiteral("|\n");
            if (static_cast<int>(i) == table.first)
                md += QStringLiteral("|") + QStringList(table.columns, QStringLiteral("---")).join(QChar('|')) +
                      QStringLiteral("|\n");
            if (static_cast<int>(i) == table.last)
                md += QChar('\n');
            continue;
        }
        const int heading = block.format.headingLevel();
        QString prefix = heading > 0 ? QString(heading, QChar('#')) + QChar(' ') : QString();
        if (block.listStyle != 0) {
            prefix = QString((block.listLevel - 1) * 2, QChar(' ')) +
                     (block.listStyle == QTextListFormat::ListDecimal ? QString::number(block.listNumber) + QChar('.')
                                                                      : QStringLiteral("-")) +
                     QChar(' ');
        }
        md += prefix + inlineMarkdown(block) + QChar('\n');
        // Items of one list follow each other; anything else is a paragraph of its own.
        const bool nextInList = block.listStyle != 0 && i + 1 < blocks.size() && blocks[i + 1].listStyle != 0;
        if (!nextInList)
            md += QChar('\n');
    }
    return md;
}

// ---------------------------------------------------------------------------------------------------------
// Frames and tables.

/*!
    \class QTextObject
    \inmodule NtGui
    \since 6.0
    \brief The QTextObject class is the base of the objects of a QTextDocument, its frames.

    \differences
    The frames are the only text objects: there are no lists as objects (\c QTextList) and no \c QTextBlockGroup.
    formatIndex() and objectIndex() return -1.
    \enddifferences

    \sa QTextFrame
*/

/*!
    Constructs an object of the document \a doc, which is its parent.
*/
QTextObject::QTextObject(QTextDocument *doc) : QObject(doc), m_document(doc) {}

/*!
    Destroys the object.
*/
QTextObject::~QTextObject() = default;

/*!
    \fn QTextFormat QTextObject::format() const

    Returns the format of the object.
*/

/*!
    \fn int QTextObject::formatIndex() const

    Returns -1: Nt keeps no table of formats.
*/

/*!
    \fn QTextDocument *QTextObject::document() const

    Returns the document of the object.
*/

/*!
    \fn int QTextObject::objectIndex() const

    Returns -1: Nt numbers no objects.
*/

/*!
    Sets the format of the object to \a format.
*/
void QTextObject::setFormat(const QTextFormat &format) { m_format = format; }

/*!
    \class QTextFrame
    \inmodule NtGui
    \since 6.0
    \brief The QTextFrame class is a part of a QTextDocument: the whole document, or a table in it.

    QTextDocument::rootFrame() is the frame of the whole document, and its child frames are its tables:

    \code
    for (QTextFrame *child : document->rootFrame()->childFrames()) {
        if (auto *table = qobject_cast<QTextTable *>(child))
            qDebug() << table->rows() << table->columns();
    }
    \endcode

    \differences
    A frame cannot be made by a program into a document (there is no QTextCursor::insertFrame()), and the frames
    are made again when the contents of the document are set. There are no frame iterators (\c begin(),
    \c end()) and no \c layoutData(). Tables in tables are child frames of the root frame too.
    \enddifferences

    \sa QTextTable, QTextDocument::rootFrame()
*/

/*!
    Constructs a frame of the document \a doc.
*/
QTextFrame::QTextFrame(QTextDocument *doc) : QTextObject(doc) { setFormat(QTextFrameFormat()); }

/*!
    Destroys the frame.
*/
QTextFrame::~QTextFrame() = default;

/*!
    \fn void QTextFrame::setFrameFormat(const QTextFrameFormat &format)

    Sets the format of the frame to \a format.
*/

/*!
    \fn QTextFrameFormat QTextFrame::frameFormat() const

    Returns the format of the frame.
*/

/*!
    Returns the position in the document of the first character of the frame.
*/
int QTextFrame::firstPosition() const {
    const QTextDocument *doc = document();
    return doc ? doc->findBlockByNumber(m_firstBlock).position() : 0;
}

/*!
    Returns the position in the document after the last character of the frame.
*/
int QTextFrame::lastPosition() const {
    const QTextDocument *doc = document();
    if (!doc)
        return 0;
    const QTextBlock last = m_lastBlock < 0 ? doc->lastBlock() : doc->findBlockByNumber(m_lastBlock);
    return last.position() + last.length() - 1;
}

/*!
    Returns the frames in this frame, in their order in the document.
*/
QList<QTextFrame *> QTextFrame::childFrames() const {
    QList<QTextFrame *> frames;
    for (QObject *child : children())
        if (auto *frame = qobject_cast<QTextFrame *>(child))
            frames.append(frame);
    return frames;
}

/*!
    Returns the frame that this frame is in, or \c nullptr for the root frame.
*/
QTextFrame *QTextFrame::parentFrame() const { return qobject_cast<QTextFrame *>(parent()); }

/*!
    \class QTextTable
    \inmodule NtGui
    \since 6.0
    \brief The QTextTable class is a table of a QTextDocument.

    A table comes from the HTML or Markdown of the document. Its format sets the border, which is drawn around the
    table and between its cells, and the padding of the cells:

    \code
    QTextTableFormat format = table->format();
    format.setBorder(1);
    format.setBorderBrush(Qt::gray);
    format.setCellPadding(5);
    table->setFormat(format);
    \endcode

    \differences
    The cells are not objects: there is no \c cellAt(), and rows and columns cannot be inserted, removed, merged
    or split. All columns are as wide as the widest cell, cells do not wrap, and a row is one paragraph of the
    document whose cells are parted by tabs.
    \enddifferences

    \sa QTextTableFormat, QTextFrame
*/

/*!
    Constructs an empty table of the document \a doc.
*/
QTextTable::QTextTable(QTextDocument *doc) : QTextFrame(doc) { QTextObject::setFormat(QTextTableFormat()); }

/*!
    Destroys the table.
*/
QTextTable::~QTextTable() = default;

/*!
    \fn int QTextTable::rows() const

    Returns the number of rows.
*/

/*!
    \fn int QTextTable::columns() const

    Returns the number of columns.
*/

/*!
    \fn QTextTableFormat QTextTable::format() const

    Returns the format of the table.
*/

/*!
    Sets the format of the table to \a format, and lays the document out again with it. The number of columns stays.
*/
void QTextTable::setFormat(const QTextTableFormat &format) {
    QTextTableFormat kept = format;
    kept.setColumns(m_columns);
    QTextObject::setFormat(kept);
    if (QTextDocument *doc = document())
        QTextDocumentPrivate::get(doc)->tableFormatChanged(this);
}

#include "moc_qtextdocument.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qtextobject.cpp"   // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qtexttable.cpp"    // NOLINT(bugprone-suspicious-include): generated by ntmoc
