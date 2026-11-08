// MIT License - see LICENSE
#include "qtextlayout_p.h"

#include <QtCore/qnamespace.h>

#include <algorithm>

namespace QtPrivate {

int advance64(QFontEngine &engine, char32_t codePoint) {
    if (codePoint == '\t')
        return 8 * engine.glyph(' ').advance64;
    return engine.glyph(codePoint).advance64;
}

int textWidth(QFontEngine &engine, const QString &text) {
    int total = 0;
    forEachCodePoint(text, [&](char32_t cp) {
        if (cp >= 0x20 || cp == '\t')
            total += advance64(engine, cp);
    });
    return (total + 32) >> 6;
}

namespace {

struct Line {
    QString text;
    int width;
};

bool isSpace(QChar c) { return c == QChar(' ') || c == QChar('\t'); }

// Splits one paragraph so that no line is wider than maxWidth, if it can be helped.
void wrapParagraph(QFontEngine &engine, const QString &paragraph, int maxWidth, bool wrapAnywhere,
                   std::vector<Line> &out) {
    QString line;
    qsizetype i = 0;
    const qsizetype n = paragraph.size();
    auto flush = [&] {
        // The spaces at a break are not part of the line.
        while (!line.isEmpty() && isSpace(line.at(line.size() - 1)))
            line.chop(1);
        out.push_back({line, textWidth(engine, line)});
        line.clear();
    };
    while (i < n) {
        // A word, and the spaces that follow it.
        qsizetype end = i;
        while (end < n && !isSpace(paragraph.at(end)))
            ++end;
        const qsizetype wordEnd = end;
        while (end < n && isSpace(paragraph.at(end)))
            ++end;
        const QString word = paragraph.mid(i, wordEnd - i);
        const QString trailing = paragraph.mid(wordEnd, end - wordEnd);
        i = end;
        if (!line.isEmpty() && textWidth(engine, line + word) > maxWidth)
            flush();
        if (textWidth(engine, word) > maxWidth && wrapAnywhere) {
            // Put as much of the word on each line as fits.
            for (qsizetype k = 0; k < word.size(); ++k) {
                if (!line.isEmpty() && textWidth(engine, line + word.at(k)) > maxWidth)
                    flush();
                line += word.at(k);
            }
        } else {
            line += word;
        }
        line += trailing;
    }
    if (!line.isEmpty() || out.empty() || paragraph.isEmpty())
        flush();
}

} // namespace

TextBlock layoutText(QFontEngine &engine, const QRect &rect, int flags, const QString &textIn) {
    QString text = textIn;
    if (flags & Qt::TextSingleLine)
        text.replace(QChar('\n'), QChar(' '));
    const bool wrap = (flags & (Qt::TextWordWrap | Qt::TextWrapAnywhere)) && rect.width() > 0;
    std::vector<Line> lines;
    qsizetype start = 0;
    for (;;) {
        const qsizetype nl = text.indexOf(QChar('\n'), start);
        const QString paragraph = text.mid(start, nl < 0 ? -1 : nl - start);
        if (wrap)
            wrapParagraph(engine, paragraph, rect.width(), flags & Qt::TextWrapAnywhere, lines);
        else
            lines.push_back({paragraph, textWidth(engine, paragraph)});
        if (nl < 0)
            break;
        start = nl + 1;
    }

    const int height = engine.ascent() + engine.descent();
    const int spacing = height + engine.leading();
    const int n = static_cast<int>(lines.size());
    const int totalHeight = n * spacing - engine.leading();
    int top = rect.top();
    if (flags & Qt::AlignBottom)
        top = rect.bottom() + 1 - totalHeight;
    else if (flags & Qt::AlignVCenter)
        top = rect.top() + (rect.height() - totalHeight) / 2;

    TextBlock block;
    for (int i = 0; i < n; ++i) {
        PlacedLine p;
        p.text = lines[static_cast<std::size_t>(i)].text;
        p.width = lines[static_cast<std::size_t>(i)].width;
        if (flags & Qt::AlignRight)
            p.x = rect.right() + 1 - p.width;
        else if (flags & Qt::AlignHCenter)
            p.x = rect.left() + (rect.width() - p.width) / 2;
        else
            p.x = rect.left();
        p.baseline = top + i * spacing + engine.ascent();
        const QRect lineRect(p.x, p.baseline - engine.ascent(), p.width, height);
        block.bounds = block.bounds.isNull() ? lineRect : block.bounds.united(lineRect);
        block.lines.push_back(std::move(p));
    }
    return block;
}

TextBlock layoutAtPoint(QFontEngine &engine, const QPoint &baselineStart, const QString &text) {
    TextBlock block = layoutText(engine, QRect(baselineStart.x(), baselineStart.y() - engine.ascent(), 0, 0), 0, text);
    return block;
}

} // namespace QtPrivate
