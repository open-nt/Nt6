// MIT License - see LICENSE
//
// Tests for the rich text classes of Nt GUI: the formats, QTextLayout and its lines, and QTextDocument with
// the HTML that it reads and writes. They run on the offscreen platform and draw into images.
#include <QtCore>
#include <QtGui/qguiapplication.h>
#include <QtGui/qimage.h>
#include <QtGui/qpainter.h>
#include <QtGui/qtextdocument.h>
#include <QtGui/qtextformat.h>
#include <QtGui/qtextlayout.h>
#include <QtGui/qtextoption.h>

#include <cstdio>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace {

// The number of pixels of `image` that are not white.
int inked(const QImage &image) {
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if ((image.pixel(x, y) & 0x00ffffffU) != 0x00ffffffU)
                ++count;
        }
    }
    return count;
}

// The number of pixels of `image` that are mostly of one channel: 0 red, 1 green, 2 blue.
int tinted(const QImage &image, int channel) {
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QRgb p = image.pixel(x, y);
            const int c[3] = {qRed(p), qGreen(p), qBlue(p)};
            const int others = std::max(c[(channel + 1) % 3], c[(channel + 2) % 3]);
            if (c[channel] > 150 && others < 100)
                ++count;
        }
    }
    return count;
}

// Whether text is drawn at all: without a font (a bare container, say) the glyphs are not, while underlines and
// backgrounds still are, so the checks of the ink of letters are left out then.
bool fontsDraw() {
    static const bool draws = [] {
        QImage image(60, 40, QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        painter.drawText(QPoint(2, 28), QStringLiteral("MWQ"));
        painter.end();
        return inked(image) > 0;
    }();
    return draws;
}

QImage render(QTextDocument &document, int width = 400, int height = 300) {
    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    document.drawContents(&painter);
    painter.end();
    return image;
}

void testFormats() {
    QTextFormat none;
    CHECK(!none.isValid());
    CHECK(none.isEmpty());
    CHECK(none.propertyCount() == 0);

    QTextCharFormat bold;
    CHECK(bold.isValid());
    CHECK(bold.isCharFormat());
    CHECK(bold.isEmpty());
    CHECK(bold.fontWeight() == QFont::Normal);
    bold.setFontWeight(QFont::Bold);
    CHECK(!bold.isEmpty());
    CHECK(bold.hasProperty(QTextFormat::FontWeight));
    CHECK(bold.fontWeight() == QFont::Bold);
    CHECK(bold.intProperty(QTextFormat::FontWeight) == QFont::Bold);

    QTextCharFormat italic;
    italic.setFontItalic(true);
    italic.setForeground(QBrush(QColor(255, 0, 0)));
    QTextCharFormat both = bold;
    both.merge(italic);
    CHECK(both.fontItalic());
    CHECK(both.fontWeight() == QFont::Bold);
    CHECK(both.foreground().color() == QColor(255, 0, 0));
    CHECK(both != bold);
    both.clearProperty(QTextFormat::FontItalic);
    both.clearForeground();
    CHECK(both == bold);

    QTextCharFormat link;
    link.setAnchor(true);
    link.setAnchorHref(QStringLiteral("page.html"));
    link.setFontUnderline(true);
    link.setFontStrikeOut(true);
    link.setFontPointSize(14);
    link.setFontFamilies(QStringList{QStringLiteral("Serif")});
    CHECK(link.isAnchor());
    CHECK(link.anchorHref() == QStringLiteral("page.html"));
    CHECK(link.fontUnderline() && link.fontStrikeOut());
    CHECK(link.fontPointSize() == 14);
    CHECK(link.fontFamilies() == QStringList{QStringLiteral("Serif")});
    const QFont font = link.font();
    CHECK(font.pointSizeF() == 14);

    QFont source;
    source.setBold(true);
    source.setItalic(true);
    QTextCharFormat fromFont;
    fromFont.setFont(source);
    CHECK(fromFont.fontWeight() == QFont::Bold);
    CHECK(fromFont.fontItalic());

    QTextBlockFormat block;
    CHECK(block.isBlockFormat());
    CHECK(block.alignment() == Qt::AlignLeft);
    CHECK(block.indent() == 0);
    block.setAlignment(Qt::AlignHCenter);
    block.setTopMargin(3);
    block.setBottomMargin(4);
    block.setLeftMargin(5);
    block.setRightMargin(6);
    block.setTextIndent(7);
    block.setIndent(2);
    block.setHeadingLevel(3);
    CHECK(block.alignment() == Qt::AlignHCenter);
    CHECK(block.topMargin() == 3 && block.bottomMargin() == 4);
    CHECK(block.leftMargin() == 5 && block.rightMargin() == 6);
    CHECK(block.textIndent() == 7);
    CHECK(block.indent() == 2);
    CHECK(block.headingLevel() == 3);
    CHECK(block.toBlockFormat() == block);
    CHECK(!block.toCharFormat().isAnchor());

    QTextListFormat list;
    CHECK(list.isListFormat());
    CHECK(list.style() == QTextListFormat::ListStyleUndefined);
    list.setStyle(QTextListFormat::ListDecimal);
    list.setIndent(3);
    CHECK(list.style() == QTextListFormat::ListDecimal);
    CHECK(list.indent() == 3);

    QTextOption option(Qt::AlignRight);
    CHECK(option.alignment() == Qt::AlignRight);
    CHECK(option.wrapMode() == QTextOption::WrapAtWordBoundaryOrAnywhere);
    option.setWrapMode(QTextOption::NoWrap);
    option.setTabStopDistance(40);
    CHECK(option.wrapMode() == QTextOption::NoWrap);
    CHECK(option.tabStopDistance() == 40);
}

void testLayout() {
    const QString text = QStringLiteral("The quick brown fox jumps over the lazy dog and runs away from the farm");
    QTextLayout layout(text);
    CHECK(layout.text() == text);
    CHECK(layout.lineCount() == 0);

    // One line when there is room.
    layout.beginLayout();
    QTextLine line = layout.createLine();
    CHECK(line.isValid());
    line.setLineWidth(10000);
    CHECK(!layout.createLine().isValid());
    layout.endLayout();
    CHECK(layout.lineCount() == 1);
    line = layout.lineAt(0);
    CHECK(line.textStart() == 0);
    CHECK(line.textLength() == static_cast<int>(text.size()));
    const qreal whole = line.naturalTextWidth();
    CHECK(whole > 100);
    CHECK(line.height() > 5);
    CHECK(line.ascent() > 0 && line.descent() >= 0);
    CHECK(qAbs(layout.maximumWidth() - whole) < 1);

    // The cursor positions grow with the text, and a point finds its position again.
    CHECK(line.cursorToX(0) == 0);
    CHECK(line.cursorToX(5) > line.cursorToX(4));
    CHECK(qAbs(line.cursorToX(static_cast<int>(text.size())) - whole) < 1);
    for (const int position : {0, 3, 10, 25, static_cast<int>(text.size())})
        CHECK(line.xToCursor(line.cursorToX(position)) == position);
    CHECK(line.xToCursor(-50) == 0);
    CHECK(line.xToCursor(whole + 500) == static_cast<int>(text.size()));

    // Lines are broken at words for a width.
    layout.beginLayout();
    qreal y = 0;
    for (QTextLine l = layout.createLine(); l.isValid(); l = layout.createLine()) {
        l.setLineWidth(whole / 3);
        l.setPosition(QPointF(0, y));
        y += l.height();
    }
    layout.endLayout();
    CHECK(layout.lineCount() >= 3);
    int covered = 0;
    for (int i = 0; i < layout.lineCount(); ++i) {
        const QTextLine l = layout.lineAt(i);
        CHECK(l.lineNumber() == i);
        CHECK(l.textStart() == covered);
        CHECK(l.naturalTextWidth() <= whole / 3 + 1);
        if (i > 0) {
            CHECK(l.y() > layout.lineAt(i - 1).y());
            CHECK(text.at(l.textStart() - 1) == QChar(' ')); // broken behind a space
        }
        covered += l.textLength();
    }
    CHECK(covered == static_cast<int>(text.size()));
    CHECK(layout.lineForTextPosition(0).lineNumber() == 0);
    CHECK(layout.lineForTextPosition(static_cast<int>(text.size()) - 1).lineNumber() == layout.lineCount() - 1);
    CHECK(qAbs(layout.boundingRect().height() - y) < 1);
    CHECK(!layout.lineAt(99).isValid());

    // A line separator breaks a line.
    QTextLayout two(QStringLiteral("one") + QChar(0x2028) + QStringLiteral("two"));
    two.beginLayout();
    for (QTextLine l = two.createLine(); l.isValid(); l = two.createLine())
        l.setLineWidth(10000);
    two.endLayout();
    CHECK(two.lineCount() == 2);
    CHECK(two.lineAt(1).textStart() == 4);

    // A format makes its part of the text bigger and of another color.
    QTextLayout formatted(QStringLiteral("plain BIG plain"));
    formatted.beginLayout();
    formatted.createLine().setLineWidth(10000);
    formatted.endLayout();
    const qreal before = formatted.lineAt(0).naturalTextWidth();
    const qreal heightBefore = formatted.lineAt(0).height();
    QTextLayout::FormatRange range;
    range.start = 6;
    range.length = 3;
    range.format.setFontPointSize(30);
    range.format.setForeground(QBrush(QColor(255, 0, 0)));
    formatted.setFormats(QList<QTextLayout::FormatRange>{range});
    CHECK(formatted.formats().size() == 1);
    formatted.beginLayout();
    formatted.createLine().setLineWidth(10000);
    formatted.endLayout();
    CHECK(formatted.lineAt(0).naturalTextWidth() > before + 10);
    CHECK(formatted.lineAt(0).height() > heightBefore + 5);

    QImage image(400, 80, QImage::Format_ARGB32);
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        formatted.draw(&painter, QPointF(4, 4));
    }
    if (fontsDraw()) {
        CHECK(tinted(image, 0) > 20);
        CHECK(inked(image) > tinted(image, 0));
    }

    // Alignment moves the text in the line.
    QTextLayout right(QStringLiteral("right"));
    right.setTextOption(QTextOption(Qt::AlignRight));
    right.beginLayout();
    right.createLine().setLineWidth(300);
    right.endLayout();
    const QRectF natural = right.lineAt(0).naturalTextRect();
    CHECK(qAbs(natural.x() + natural.width() - 300) < 1.5);
    CHECK(right.lineAt(0).cursorToX(0) > 200);

    formatted.clearFormats();
    CHECK(formatted.formats().isEmpty());
    formatted.clearLayout();
    CHECK(formatted.lineCount() == 0);
}

void testPlainDocument() {
    QTextDocument document;
    CHECK(document.isEmpty());
    CHECK(document.blockCount() == 1);
    CHECK(document.characterCount() == 1);
    CHECK(document.toPlainText().isEmpty());
    CHECK(document.documentMargin() == 4);
    CHECK(document.textWidth() == -1);

    int changes = 0;
    QObject::connect(&document, &QTextDocument::contentsChanged, [&changes] { ++changes; });
    document.setPlainText(QStringLiteral("first line\nsecond\n\nfourth"));
    CHECK(changes == 1);
    CHECK(!document.isEmpty());
    CHECK(document.blockCount() == 4);
    CHECK(document.toPlainText() == QStringLiteral("first line\nsecond\n\nfourth"));
    CHECK(document.characterCount() == 26);
    CHECK(document.characterAt(0) == QChar('f'));
    CHECK(document.characterAt(10) == QChar(0x2029));
    CHECK(document.characterAt(11) == QChar('s'));
    CHECK(document.characterAt(500).isNull());

    QTextBlock block = document.begin();
    CHECK(block.isValid());
    CHECK(block == document.firstBlock());
    CHECK(block.text() == QStringLiteral("first line"));
    CHECK(block.position() == 0 && block.length() == 11);
    CHECK(block.blockNumber() == 0);
    CHECK(block.document() == &document);
    CHECK(!block.previous().isValid());
    block = block.next();
    CHECK(block.text() == QStringLiteral("second"));
    CHECK(block.position() == 11);
    CHECK(block.contains(11) && block.contains(17) && !block.contains(18));
    CHECK(block.previous() == document.firstBlock());
    CHECK(document.lastBlock().text() == QStringLiteral("fourth"));
    CHECK(!document.lastBlock().next().isValid());
    CHECK(document.lastBlock().next() == document.end());
    CHECK(document.findBlock(12).blockNumber() == 1);
    CHECK(document.findBlock(18).blockNumber() == 2);
    CHECK(!document.findBlock(999).isValid());
    CHECK(document.findBlockByNumber(3).text() == QStringLiteral("fourth"));
    CHECK(!document.findBlockByNumber(4).isValid());
    int count = 0;
    for (QTextBlock b = document.begin(); b != document.end(); b = b.next())
        ++count;
    CHECK(count == 4);

    // Without a text width the document is as wide as its longest line.
    const QSizeF natural = document.size();
    CHECK(natural.width() > 30);
    CHECK(natural.height() > 30);
    CHECK(document.lineCount() == 4);
    CHECK(qAbs(document.idealWidth() - natural.width()) <= 1);
    CHECK(document.documentLayout()->documentSize() == natural);
    CHECK(document.documentLayout()->document() == &document);

    // A narrow text width breaks lines, and the document gets higher.
    document.setTextWidth(40);
    CHECK(document.textWidth() == 40);
    CHECK(document.size().width() == 40);
    CHECK(document.size().height() > natural.height());
    CHECK(document.lineCount() > 4);
    CHECK(document.idealWidth() >= natural.width() - 1);
    document.setTextWidth(-1);
    CHECK(document.size() == natural);

    document.setDocumentMargin(20);
    CHECK(document.documentMargin() == 20);
    CHECK(qAbs(document.size().width() - (natural.width() + 32)) <= 1);
    document.setDocumentMargin(4);

    const QRectF second = document.documentLayout()->blockBoundingRect(document.findBlockByNumber(1));
    const QRectF first = document.documentLayout()->blockBoundingRect(document.firstBlock());
    CHECK(first.y() == 4);
    CHECK(second.y() >= first.y() + first.height());
    CHECK(document.firstBlock().layout() != nullptr);
    CHECK(document.firstBlock().layout()->lineCount() == 1);

    QFont big = document.defaultFont();
    big.setPointSizeF(big.pointSizeF() > 0 ? big.pointSizeF() * 2 : 24);
    document.setDefaultFont(big);
    CHECK(document.defaultFont() == big);
    CHECK(document.size().height() > natural.height() * 1.5);

    QImage image = render(document);
    CHECK(!fontsDraw() || inked(image) > 100);

    document.clear();
    CHECK(document.isEmpty());
    CHECK(document.blockCount() == 1);
    CHECK(changes == 2);

    QTextDocument with(QStringLiteral("text"));
    CHECK(with.toPlainText() == QStringLiteral("text"));

    document.setMetaInformation(QTextDocument::DocumentTitle, QStringLiteral("Title"));
    document.setMetaInformation(QTextDocument::DocumentUrl, QStringLiteral("file:///x"));
    CHECK(document.metaInformation(QTextDocument::DocumentTitle) == QStringLiteral("Title"));
    CHECK(document.metaInformation(QTextDocument::DocumentUrl) == QStringLiteral("file:///x"));

    QTextOption option;
    option.setWrapMode(QTextOption::NoWrap);
    document.setDefaultTextOption(option);
    CHECK(document.defaultTextOption().wrapMode() == QTextOption::NoWrap);
}

void testHtml() {
    QTextDocument document;
    document.setHtml(QStringLiteral(
        "<html><head><title>My  page</title><style>p { color: red }</style></head><body>"
        "<h1>Heading</h1>\n"
        "<p>Some <b>bold</b>, <i>italic</i> and\n   <u>underlined</u> text &amp; an entity&#33;</p>"
        "<p align=\"center\">A <a href=\"http://example.com/a?b=1&amp;c=2\">link</a> here<br>second line</p>"
        "<!-- a comment --><script>var x = '<p>no</p>';</script>"
        "</body></html>"));
    CHECK(document.metaInformation(QTextDocument::DocumentTitle) == QStringLiteral("My page"));
    CHECK(document.blockCount() == 3);
    CHECK(document.toPlainText() ==
          QStringLiteral("Heading\nSome bold, italic and underlined text & an entity!\nA link here\nsecond line"));

    QTextBlock heading = document.firstBlock();
    CHECK(heading.blockFormat().headingLevel() == 1);
    CHECK(heading.charFormat().fontWeight() == QFont::Bold);
    CHECK(heading.charFormat().fontPointSize() > 0);

    QTextBlock paragraph = heading.next();
    CHECK(paragraph.blockFormat().headingLevel() == 0);
    CHECK(paragraph.charFormat().isEmpty());
    const QList<QTextLayout::FormatRange> formats = paragraph.textFormats();
    CHECK(formats.size() == 3);
    if (formats.size() == 3) {
        CHECK(paragraph.text().mid(formats.at(0).start, formats.at(0).length) == QStringLiteral("bold"));
        CHECK(formats.at(0).format.fontWeight() == QFont::Bold);
        CHECK(paragraph.text().mid(formats.at(1).start, formats.at(1).length) == QStringLiteral("italic"));
        CHECK(formats.at(1).format.fontItalic());
        CHECK(paragraph.text().mid(formats.at(2).start, formats.at(2).length) == QStringLiteral("underlined"));
        CHECK(formats.at(2).format.fontUnderline());
    }

    QTextBlock linked = paragraph.next();
    CHECK(linked.blockFormat().alignment() == Qt::AlignHCenter);
    CHECK(linked.text().contains(QChar(0x2028)));
    bool found = false;
    for (const QTextLayout::FormatRange &range : linked.textFormats()) {
        if (range.format.isAnchor()) {
            found = true;
            CHECK(range.format.anchorHref() == QStringLiteral("http://example.com/a?b=1&c=2"));
            CHECK(linked.text().mid(range.start, range.length) == QStringLiteral("link"));
        }
    }
    CHECK(found);

    // The link is found where it is drawn, and nowhere else.
    document.setTextWidth(400);
    QTextLayout *layout = linked.layout();
    CHECK(layout && layout->lineCount() == 2);
    if (layout && layout->lineCount() == 2) {
        const QTextLine line = layout->lineAt(0);
        const qreal x = (line.cursorToX(2) + line.cursorToX(6)) / 2;
        const QPointF inside = layout->position() + QPointF(x, line.y() + line.height() / 2);
        CHECK(document.documentLayout()->anchorAt(inside) == QStringLiteral("http://example.com/a?b=1&c=2"));
        const QPointF beside = layout->position() + QPointF(line.cursorToX(0) + 1, line.y() + line.height() / 2);
        CHECK(document.documentLayout()->anchorAt(beside).isEmpty());
        CHECK(line.cursorToX(0) > 50); // centered
    }
    CHECK(document.documentLayout()->anchorAt(QPointF(390, 5)).isEmpty());
    CHECK(document.documentLayout()->anchorAt(QPointF(-5, -5)).isEmpty());

    // The heading is bigger than a paragraph line, and links are blue.
    CHECK(heading.layout()->lineAt(0).height() > paragraph.layout()->lineAt(0).height() * 1.4);
    QImage image = render(document);
    CHECK(tinted(image, 2) > 10);
    CHECK(tinted(image, 0) == 0); // the style sheet in the head is not used, and not shown

    // Colors, fonts and sizes.
    document.setHtml(QStringLiteral(
        "<p><font color=\"red\" size=\"5\">red</font> <span style=\"color:#00ff00; background-color: rgb(0, 0, 255);"
        " font-weight:600; font-size:20pt; font-family:'Mono', serif; text-decoration: underline line-through\">"
        "green</span> <tt>code</tt> <s>gone</s> <big>big</big> <small>small</small></p>"));
    CHECK(document.blockCount() == 1);
    CHECK(document.toPlainText() == QStringLiteral("red green code gone big small"));
    const QList<QTextLayout::FormatRange> ranges = document.firstBlock().textFormats();
    CHECK(ranges.size() == 6);
    if (ranges.size() == 6) {
        CHECK(ranges.at(0).format.foreground().color() == QColor(255, 0, 0));
        CHECK(ranges.at(0).format.fontPointSize() > document.defaultFont().pointSizeF());
        const QTextCharFormat green = ranges.at(1).format;
        CHECK(green.foreground().color() == QColor(0, 255, 0));
        CHECK(green.background().color() == QColor(0, 0, 255));
        CHECK(green.fontWeight() == QFont::Bold);
        CHECK(green.fontPointSize() == 20);
        CHECK(green.fontFamilies() == (QStringList{QStringLiteral("Mono"), QStringLiteral("serif")}));
        CHECK(green.fontUnderline() && green.fontStrikeOut());
        CHECK(ranges.at(2).format.fontFixedPitch());
        CHECK(ranges.at(3).format.fontStrikeOut());
        CHECK(ranges.at(4).format.fontPointSize() > ranges.at(5).format.fontPointSize());
    }
    image = render(document);
    CHECK(!fontsDraw() || tinted(image, 0) > 10);
    CHECK(tinted(image, 2) > 50); // the background of the span

    // Lists.
    document.setHtml(QStringLiteral("<p>Intro</p><ul><li>one<li>two</li><li>three <ol start=\"4\"><li>nested</li>"
                                    "<li>more</li></ol></li></ul><p>Outro</p>"));
    CHECK(document.toPlainText() == QStringLiteral("Intro\none\ntwo\nthree\nnested\nmore\nOutro"));
    document.setTextWidth(300);
    const QAbstractTextDocumentLayout *docLayout = document.documentLayout();
    const qreal introX = docLayout->blockBoundingRect(document.findBlockByNumber(0)).x();
    const qreal itemX = docLayout->blockBoundingRect(document.findBlockByNumber(1)).x();
    const qreal nestedX = docLayout->blockBoundingRect(document.findBlockByNumber(4)).x();
    CHECK(itemX > introX + 10);
    CHECK(nestedX > itemX + 10);
    CHECK(docLayout->blockBoundingRect(document.findBlockByNumber(6)).x() == introX);
    // The marks are drawn in the indentation.
    image = render(document);
    const QRectF item = docLayout->blockBoundingRect(document.findBlockByNumber(1));
    int marks = 0;
    for (int y = qRound(item.y()); y < qRound(item.y() + item.height()); ++y) {
        for (int x = qRound(introX); x < qRound(itemX) - 1; ++x) {
            if ((image.pixel(x, y) & 0x00ffffffU) != 0x00ffffffU)
                ++marks;
        }
    }
    CHECK(!fontsDraw() || marks > 0);

    // Preformatted text keeps its spaces and lines; other text does not.
    document.setHtml(QStringLiteral("<pre>\na  b\n  c</pre><div>x \n  y</div>"));
    CHECK(document.toPlainText() == QStringLiteral("a  b\n  c\nx y"));
    CHECK(document.blockCount() == 2);

    // A rule, a table, an image and things that are not tags.
    document.setHtml(QStringLiteral("before<hr>after"));
    CHECK(document.blockCount() == 3);
    CHECK(document.toPlainText() == QStringLiteral("before\n\nafter"));
    CHECK(inked(render(document)) > 0);
    document.setHtml(QStringLiteral("<table><tr><td>a</td><td>b</td></tr><tr><th>c</th><td>d</td></tr></table>"));
    CHECK(document.toPlainText() == QStringLiteral("a\tb\nc\td"));
    document.setHtml(QStringLiteral("<p>1 < 2 &unknown; &#x41;&#66; <img src=\"x.png\" alt=\"picture\"> <unknown>kept</unknown></p>"));
    CHECK(document.toPlainText() == QStringLiteral("1 < 2 &unknown; AB picture kept"));
    document.setHtml(QStringLiteral("<p>unclosed <b>bold<p>next"));
    CHECK(document.toPlainText() == QStringLiteral("unclosed bold\nnext"));
    CHECK(document.findBlockByNumber(1).textFormats().isEmpty());
    document.setHtml(QString());
    CHECK(document.isEmpty());
    CHECK(document.blockCount() == 1);
    document.setHtml(QStringLiteral("<p style=\"margin-top:30px; margin-left: 25px; text-align: right; text-indent:10px\">m</p>"));
    CHECK(document.firstBlock().blockFormat().topMargin() == 30);
    CHECK(document.firstBlock().blockFormat().leftMargin() == 25);
    CHECK(document.firstBlock().blockFormat().textIndent() == 10);
    CHECK(document.firstBlock().blockFormat().alignment() == Qt::AlignRight);
    CHECK(document.documentLayout()->blockBoundingRect(document.firstBlock()).y() == 34);
    document.setHtml(QStringLiteral("<blockquote>quoted</blockquote><center>mid</center>"));
    CHECK(document.firstBlock().blockFormat().indent() == 1);
    CHECK(document.lastBlock().blockFormat().alignment() == Qt::AlignHCenter);
}

void testHtmlRoundTrip() {
    QTextDocument document;
    document.setHtml(QStringLiteral(
        "<title>T &amp; t</title><h2>Head</h2><p align=\"right\">a <b>b</b> <i style=\"color:#123456\">c</i> "
        "<a href=\"x.html?a=1&amp;b=2\">d &lt;e&gt;</a><br>f</p><ol type=\"a\" start=\"3\"><li>one</li><li>two</li></ol>"
        "<ul><li><u>three</u></li></ul><hr><p style=\"margin-left:20px\"><tt>g</tt> <s>h</s> "
        "<span style=\"font-size:18pt; background-color:#ffff00\">i</span></p>"));
    const QString html = document.toHtml();
    CHECK(html.contains(QStringLiteral("<title>T &amp; t</title>")));
    CHECK(html.contains(QStringLiteral("<h2")));
    CHECK(html.contains(QStringLiteral("href=\"x.html?a=1&amp;b=2\"")));
    CHECK(html.contains(QStringLiteral("d &lt;e&gt;")));
    CHECK(html.contains(QStringLiteral("<ol type=\"a\" start=\"3\">")));
    CHECK(html.contains(QStringLiteral("<hr/>")));
    CHECK(Qt::mightBeRichText(html));

    QTextDocument again;
    again.setHtml(html);
    CHECK(again.toPlainText() == document.toPlainText());
    CHECK(again.blockCount() == document.blockCount());
    CHECK(again.metaInformation(QTextDocument::DocumentTitle) == QStringLiteral("T & t"));
    QTextBlock a = document.begin();
    QTextBlock b = again.begin();
    for (; a.isValid() && b.isValid(); a = a.next(), b = b.next()) {
        CHECK(a.text() == b.text());
        CHECK(a.blockFormat().alignment() == b.blockFormat().alignment());
        CHECK(a.blockFormat().headingLevel() == b.blockFormat().headingLevel());
        const QList<QTextLayout::FormatRange> fa = a.textFormats();
        const QList<QTextLayout::FormatRange> fb = b.textFormats();
        CHECK(fa.size() == fb.size());
        for (qsizetype i = 0; i < fa.size() && i < fb.size(); ++i) {
            CHECK(fa.at(i).start == fb.at(i).start && fa.at(i).length == fb.at(i).length);
            CHECK(fa.at(i).format.fontWeight() == fb.at(i).format.fontWeight());
            CHECK(fa.at(i).format.fontItalic() == fb.at(i).format.fontItalic());
            CHECK(fa.at(i).format.fontUnderline() == fb.at(i).format.fontUnderline());
            CHECK(fa.at(i).format.fontStrikeOut() == fb.at(i).format.fontStrikeOut());
            CHECK(fa.at(i).format.anchorHref() == fb.at(i).format.anchorHref());
            CHECK(fa.at(i).format.foreground() == fb.at(i).format.foreground());
            CHECK(fa.at(i).format.background() == fb.at(i).format.background());
            CHECK(qAbs(fa.at(i).format.fontPointSize() - fb.at(i).format.fontPointSize()) < 0.01);
        }
    }
    CHECK(!a.isValid() && !b.isValid());
    document.setTextWidth(350);
    again.setTextWidth(350);
    CHECK(again.size() == document.size());
    CHECK(render(again) == render(document));
    CHECK(again.toHtml() == html);
}

void testHelpers() {
    CHECK(Qt::mightBeRichText(QStringLiteral("<b>bold</b>")));
    CHECK(Qt::mightBeRichText(QStringLiteral("  some <i>text</i>")));
    CHECK(Qt::mightBeRichText(QStringLiteral("line<br/>line")));
    CHECK(Qt::mightBeRichText(QStringLiteral("<!DOCTYPE html><html></html>")));
    CHECK(Qt::mightBeRichText(QStringLiteral("<a href=\"x\">x</a>")));
    CHECK(!Qt::mightBeRichText(QString()));
    CHECK(!Qt::mightBeRichText(QStringLiteral("plain text")));
    CHECK(!Qt::mightBeRichText(QStringLiteral("a < b and c > d")));
    CHECK(!Qt::mightBeRichText(QStringLiteral("std::vector<int> v")));
    CHECK(!Qt::mightBeRichText(QStringLiteral("first line\n<b>second</b>")));
    CHECK(!Qt::mightBeRichText(QStringLiteral("<unknown>")));

    const QString plain = QStringLiteral("a <b> & c\nnext  line\n\nparagraph");
    const QString html = Qt::convertFromPlainText(plain);
    CHECK(html.contains(QStringLiteral("a&nbsp;&lt;b&gt;&nbsp;&amp;&nbsp;c")) ||
          html.contains(QStringLiteral("&lt;b&gt;")));
    QTextDocument document;
    document.setHtml(html);
    CHECK(document.blockCount() == 2);
    QString text = document.toPlainText();
    text.replace(QChar(0xa0), QChar(' '));
    CHECK(text == QStringLiteral("a <b> & c\nnext  line\nparagraph"));
    document.setHtml(Qt::convertFromPlainText(QStringLiteral("a  b"), Qt::WhiteSpaceNormal));
    CHECK(document.toPlainText() == QStringLiteral("a b"));
}

} // namespace

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    testFormats();
    testLayout();
    testPlainDocument();
    testHtml();
    testHtmlRoundTrip();
    testHelpers();
    if (g_failures == 0)
        std::printf("all rich text tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
