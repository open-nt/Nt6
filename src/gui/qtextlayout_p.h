// MIT License - see LICENSE
#pragma once

#include "qfontengine_p.h"

#include <QtCore/qrect.h>
#include <QtCore/qstring.h>

#include <vector>

namespace QtPrivate {

// Calls f(codePoint) for each Unicode code point of `text`.
template <typename F> void forEachCodePoint(const QString &text, F f) {
    const qsizetype n = text.size();
    for (qsizetype i = 0; i < n; ++i) {
        char32_t cp = text.at(i).unicode();
        if (cp >= 0xd800 && cp < 0xdc00 && i + 1 < n) {
            const char32_t low = text.at(i + 1).unicode();
            if (low >= 0xdc00 && low < 0xe000) {
                cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
                ++i;
            }
        }
        f(cp);
    }
}

// Advance of one code point in 26.6 pixels; a tab is eight spaces.
int advance64(QFontEngine &engine, char32_t codePoint);
// Width of `text` in pixels, on one line.
int textWidth(QFontEngine &engine, const QString &text);

struct PlacedLine {
    QString text;
    int x; // left edge of the text
    int baseline;
    int width;
};

struct TextBlock {
    std::vector<PlacedLine> lines;
    QRect bounds; // of all lines
};

// Breaks `text` into lines and places them in `rect` as the Qt::AlignmentFlag and Qt::TextFlag
// values in `flags` say.
TextBlock layoutText(QFontEngine &engine, const QRect &rect, int flags, const QString &text);
// The lines of `text` when drawn from a single point: one per new line.
TextBlock layoutAtPoint(QFontEngine &engine, const QPoint &baselineStart, const QString &text);

} // namespace QtPrivate
