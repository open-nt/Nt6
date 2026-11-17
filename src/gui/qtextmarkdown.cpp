// MIT License - see LICENSE
#include "qtextmarkdown_p.h"

#include <QtCore/qregularexpression.h>
#include <QtCore/qstringlist.h>

namespace {

struct Options {
    bool github;
    bool noHtml;
};

QString escape(const QString &text) { return text.toHtmlEscaped(); }

bool isPunctuation(QChar c) { return c.unicode() < 128 && c.isPunct(); }

int leadingSpaces(const QString &line) {
    int n = 0;
    while (n < line.size() && line.at(n) == QChar(' '))
        ++n;
    return n;
}

bool isBlank(const QString &line) { return line.trimmed().isEmpty(); }

// ---- Inline ----

QString inlineHtml(const QString &text, const Options &options);

// The index of the closing `delimiter` for one that opens at `from`, or -1: it must not follow white space, and
// for '_' it must not be inside a word.
qsizetype closingDelimiter(const QString &text, qsizetype from, const QString &delimiter) {
    qsizetype at = from;
    while ((at = text.indexOf(delimiter, at)) >= 0) {
        const bool afterSpace = at > 0 && text.at(at - 1).isSpace();
        const qsizetype end = at + delimiter.size();
        const bool intraword = delimiter.startsWith(QChar('_')) && end < text.size() && text.at(end).isLetterOrNumber();
        // `**` does not close a single `*`.
        const bool longer = end < text.size() && text.at(end) == delimiter.at(0) && delimiter.size() == 1;
        if (!afterSpace && !intraword && !longer && at > from)
            return at;
        at = end;
    }
    return -1;
}

// Reads a link destination and title in parentheses at `pos` (at the '('). Returns the index after ')' or -1.
qsizetype linkTarget(const QString &text, qsizetype pos, QString *url) {
    if (pos >= text.size() || text.at(pos) != QChar('('))
        return -1;
    int depth = 0;
    for (qsizetype i = pos; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QChar('\\')) {
            ++i;
            continue;
        }
        if (c == QChar('('))
            ++depth;
        else if (c == QChar(')') && --depth == 0) {
            QString inside = text.mid(pos + 1, i - pos - 1).trimmed();
            // A title in quotes after the destination is dropped.
            const qsizetype space = inside.indexOf(QChar(' '));
            if (space > 0 && (inside.endsWith(QChar('"')) || inside.endsWith(QChar('\''))))
                inside = inside.left(space);
            if (inside.startsWith(QChar('<')) && inside.endsWith(QChar('>')))
                inside = inside.mid(1, inside.size() - 2);
            *url = inside;
            return i + 1;
        }
    }
    return -1;
}

// The index of the ']' that closes the '[' at `pos`, or -1.
qsizetype closingBracket(const QString &text, qsizetype pos) {
    int depth = 0;
    for (qsizetype i = pos; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == QChar('\\')) {
            ++i;
        } else if (c == QChar('`')) {
            const qsizetype end = text.indexOf(QChar('`'), i + 1);
            if (end > 0)
                i = end;
        } else if (c == QChar('[')) {
            ++depth;
        } else if (c == QChar(']') && --depth == 0) {
            return i;
        }
    }
    return -1;
}

QString inlineHtml(const QString &text, const Options &options) {
    static const QRegularExpression autolink(
        QStringLiteral("^<((?:https?|ftp|mailto):[^\\s<>]+|[^\\s<>@]+@[^\\s<>@]+)>"));
    static const QRegularExpression htmlTag(
        QStringLiteral("^</"
                       "?[A-Za-z][A-Za-z0-9-]*(?:\\s+[A-Za-z_:][-A-Za-z0-9_:.]*(?:\\s*=\\s*(?:\"[^\"]*\"|'[^']*'|[^"
                       "\\s\"'=<>`]+))?)*\\s*/?>"));
    static const QRegularExpression bareLink(QStringLiteral("^(?:https?://|www\\.)[^\\s<]*[^\\s<?!.,:*_~)]"));
    QString out;
    const qsizetype n = text.size();
    qsizetype i = 0;
    while (i < n) {
        const QChar c = text.at(i);
        if (c == QChar('\\') && i + 1 < n) {
            const QChar next = text.at(i + 1);
            if (next == QChar('\n')) {
                out += QStringLiteral("<br/>");
                i += 2;
                continue;
            }
            if (isPunctuation(next)) {
                out += escape(QString(next));
                i += 2;
                continue;
            }
        }
        if (c == QChar('\n')) {
            // Two spaces before a line end make a line break; otherwise it is a space.
            if (out.endsWith(QStringLiteral("  "))) {
                while (out.endsWith(QChar(' ')))
                    out.chop(1);
                out += QStringLiteral("<br/>");
            } else {
                out += QChar(' ');
            }
            ++i;
            continue;
        }
        if (c == QChar('`')) {
            qsizetype run = 0;
            while (i + run < n && text.at(i + run) == QChar('`'))
                ++run;
            const QString fence(run, QChar('`'));
            qsizetype end = i + run;
            while ((end = text.indexOf(fence, end)) >= 0) {
                if (end + run >= n || text.at(end + run) != QChar('`'))
                    break;
                while (end < n && text.at(end) == QChar('`'))
                    ++end;
            }
            if (end > 0) {
                QString code = text.mid(i + run, end - i - run);
                code.replace(QChar('\n'), QChar(' '));
                if (code.size() > 2 && code.startsWith(QChar(' ')) && code.endsWith(QChar(' ')))
                    code = code.mid(1, code.size() - 2);
                out += QStringLiteral("<code>") + escape(code) + QStringLiteral("</code>");
                i = end + run;
                continue;
            }
            out += fence;
            i += run;
            continue;
        }
        if (c == QChar('<')) {
            const QString rest = text.mid(i);
            const QRegularExpressionMatch link = autolink.match(rest);
            if (link.hasMatch()) {
                QString target = link.captured(1);
                const QString href = target.contains(QChar('@')) && !target.contains(QChar(':'))
                                         ? QStringLiteral("mailto:") + target
                                         : target;
                out += QStringLiteral("<a href=\"") + escape(href) + QStringLiteral("\">") + escape(target) +
                       QStringLiteral("</a>");
                i += link.capturedLength(0);
                continue;
            }
            if (!options.noHtml) {
                const QRegularExpressionMatch tag = htmlTag.match(rest);
                if (tag.hasMatch()) {
                    out += tag.captured(0);
                    i += tag.capturedLength(0);
                    continue;
                }
            }
        }
        if ((c == QChar('!') && i + 1 < n && text.at(i + 1) == QChar('[')) || c == QChar('[')) {
            const bool image = c == QChar('!');
            const qsizetype open = image ? i + 1 : i;
            const qsizetype close = closingBracket(text, open);
            QString url;
            const qsizetype after = close > 0 ? linkTarget(text, close + 1, &url) : -1;
            if (after > 0) {
                const QString label = text.mid(open + 1, close - open - 1);
                if (image)
                    out += QStringLiteral("<img src=\"") + escape(url) + QStringLiteral("\" alt=\"") + escape(label) +
                           QStringLiteral("\"/>");
                else
                    out += QStringLiteral("<a href=\"") + escape(url) + QStringLiteral("\">") +
                           inlineHtml(label, options) + QStringLiteral("</a>");
                i = after;
                continue;
            }
        }
        if (c == QChar('*') || c == QChar('_') || (options.github && c == QChar('~'))) {
            qsizetype run = 0;
            while (i + run < n && text.at(i + run) == c)
                ++run;
            const bool opensWord = i + run < n && !text.at(i + run).isSpace();
            const bool intraword = c == QChar('_') && i > 0 && text.at(i - 1).isLetterOrNumber();
            if (opensWord && !intraword) {
                // The longest emphasis that closes: strong, then emphasis; ~~ is strikethrough.
                bool done = false;
                for (qsizetype len = std::min<qsizetype>(run, c == QChar('~') ? 2 : 3); len >= 1 && !done; --len) {
                    if (c == QChar('~') && len != 2)
                        continue;
                    const QString delimiter(len, c);
                    const qsizetype close = closingDelimiter(text, i + len, delimiter);
                    if (close < 0)
                        continue;
                    const QString inner = inlineHtml(text.mid(i + len, close - i - len), options);
                    if (c == QChar('~'))
                        out += QStringLiteral("<del>") + inner + QStringLiteral("</del>");
                    else if (len == 3)
                        out += QStringLiteral("<em><strong>") + inner + QStringLiteral("</strong></em>");
                    else if (len == 2)
                        out += QStringLiteral("<strong>") + inner + QStringLiteral("</strong>");
                    else
                        out += QStringLiteral("<em>") + inner + QStringLiteral("</em>");
                    i = close + len;
                    done = true;
                }
                if (done)
                    continue;
            }
            out += QString(run, c);
            i += run;
            continue;
        }
        if (options.github && (c == QChar('h') || c == QChar('w')) && (i == 0 || !text.at(i - 1).isLetterOrNumber())) {
            const QRegularExpressionMatch link = bareLink.match(text.mid(i));
            if (link.hasMatch()) {
                const QString target = link.captured(0);
                const QString href =
                    target.startsWith(QStringLiteral("www.")) ? QStringLiteral("http://") + target : target;
                out += QStringLiteral("<a href=\"") + escape(href) + QStringLiteral("\">") + escape(target) +
                       QStringLiteral("</a>");
                i += link.capturedLength(0);
                continue;
            }
        }
        out += escape(QString(c));
        ++i;
    }
    return out;
}

// ---- Blocks ----

QString blocksHtml(const QStringList &lines, const Options &options);

const QRegularExpression &fenceRe() {
    static const QRegularExpression re(QStringLiteral("^ {0,3}(`{3,}|~{3,})\\s*([^`]*)$"));
    return re;
}
const QRegularExpression &headingRe() {
    static const QRegularExpression re(QStringLiteral("^ {0,3}(#{1,6})(?:[ \\t]+(.*?))?(?:[ \\t]+#+)?[ \\t]*$"));
    return re;
}
const QRegularExpression &ruleRe() {
    static const QRegularExpression re(
        QStringLiteral("^ {0,3}(?:(?:\\*[ \\t]*){3,}|(?:-[ \\t]*){3,}|(?:_[ \\t]*){3,})$"));
    return re;
}
const QRegularExpression &listRe() {
    static const QRegularExpression re(QStringLiteral("^( {0,3})([-*+]|(\\d{1,9})[.)])(?:([ \\t]+)(.*)|$)"));
    return re;
}
const QRegularExpression &delimiterRowRe() {
    static const QRegularExpression re(
        QStringLiteral("^ {0,3}\\|?[ \\t]*:?-+:?[ \\t]*(?:\\|[ \\t]*:?-+:?[ \\t]*)*\\|?[ \\t]*$"));
    return re;
}

QStringList tableCells(QString row) {
    row = row.trimmed();
    if (row.startsWith(QChar('|')))
        row.remove(0, 1);
    if (row.endsWith(QChar('|')) && !row.endsWith(QStringLiteral("\\|")))
        row.chop(1);
    QStringList cells;
    QString cell;
    for (qsizetype i = 0; i < row.size(); ++i) {
        const QChar c = row.at(i);
        if (c == QChar('\\') && i + 1 < row.size() && row.at(i + 1) == QChar('|')) {
            cell += QChar('|');
            ++i;
        } else if (c == QChar('|')) {
            cells.append(cell.trimmed());
            cell.clear();
        } else {
            cell += c;
        }
    }
    cells.append(cell.trimmed());
    return cells;
}

// Whether `line` starts a block of HTML: an element of the block kind, a comment, or a tag alone on its line.
bool startsHtmlBlock(const QString &line) {
    static const QStringList blockTags{
        QStringLiteral("address"), QStringLiteral("article"), QStringLiteral("aside"),   QStringLiteral("blockquote"),
        QStringLiteral("body"),    QStringLiteral("center"),  QStringLiteral("dd"),      QStringLiteral("details"),
        QStringLiteral("div"),     QStringLiteral("dl"),      QStringLiteral("dt"),      QStringLiteral("figure"),
        QStringLiteral("footer"),  QStringLiteral("h1"),      QStringLiteral("h2"),      QStringLiteral("h3"),
        QStringLiteral("h4"),      QStringLiteral("h5"),      QStringLiteral("h6"),      QStringLiteral("header"),
        QStringLiteral("hr"),      QStringLiteral("html"),    QStringLiteral("li"),      QStringLiteral("ol"),
        QStringLiteral("p"),       QStringLiteral("pre"),     QStringLiteral("section"), QStringLiteral("table"),
        QStringLiteral("tbody"),   QStringLiteral("td"),      QStringLiteral("th"),      QStringLiteral("thead"),
        QStringLiteral("tr"),      QStringLiteral("ul"),
    };
    static const QRegularExpression tagAlone(QStringLiteral("^</?[A-Za-z][A-Za-z0-9-]*(?:\\s[^<>]*)?/?>\\s*$"));
    static const QRegularExpression tagName(QStringLiteral("^</?([A-Za-z][A-Za-z0-9-]*)"));
    const QString trimmed = line.trimmed();
    if (trimmed.startsWith(QStringLiteral("<!--")))
        return true;
    const QRegularExpressionMatch name = tagName.match(trimmed);
    if (!name.hasMatch() || leadingSpaces(line) > 3)
        return false;
    return blockTags.contains(name.captured(1).toLower()) || tagAlone.match(trimmed).hasMatch();
}

bool startsBlock(const QString &line, const Options &options) {
    if (fenceRe().match(line).hasMatch() || headingRe().match(line).hasMatch() || ruleRe().match(line).hasMatch())
        return true;
    const QString trimmed = line.trimmed();
    if (trimmed.startsWith(QChar('>')))
        return true;
    const QRegularExpressionMatch list = listRe().match(line);
    // A numbered list breaks into a paragraph only when it starts at 1, as in CommonMark.
    if (list.hasMatch() && !list.captured(5).trimmed().isEmpty() &&
        (list.captured(3).isEmpty() || list.captured(3) == QStringLiteral("1")))
        return true;
    return !options.noHtml && startsHtmlBlock(line);
}

QString blocksHtml(const QStringList &lines, const Options &options) {
    QString html;
    const qsizetype n = lines.size();
    qsizetype i = 0;
    while (i < n) {
        const QString &line = lines.at(i);
        if (isBlank(line)) {
            ++i;
            continue;
        }
        // A fenced code block.
        const QRegularExpressionMatch fence = fenceRe().match(line);
        if (fence.hasMatch()) {
            const QString marker = fence.captured(1);
            const int indent = leadingSpaces(line);
            QStringList code;
            ++i;
            while (i < n) {
                const QString &l = lines.at(i);
                const QString t = l.trimmed();
                if (t.startsWith(marker) && t.count(marker.at(0)) == t.size() && leadingSpaces(l) < 4) {
                    ++i;
                    break;
                }
                code.append(l.mid(std::min(indent, leadingSpaces(l))));
                ++i;
            }
            html += QStringLiteral("<pre><code>") + escape(code.join(QChar('\n'))) + QStringLiteral("</code></pre>\n");
            continue;
        }
        const QRegularExpressionMatch heading = headingRe().match(line);
        if (heading.hasMatch()) {
            const QString level = QString::number(heading.captured(1).size());
            html += QStringLiteral("<h") + level + QChar('>') + inlineHtml(heading.captured(2).trimmed(), options) +
                    QStringLiteral("</h") + level + QStringLiteral(">\n");
            ++i;
            continue;
        }
        if (ruleRe().match(line).hasMatch()) {
            html += QStringLiteral("<hr/>\n");
            ++i;
            continue;
        }
        // A block quote: the lines that start with '>', without it.
        if (line.trimmed().startsWith(QChar('>'))) {
            QStringList inner;
            while (i < n && lines.at(i).trimmed().startsWith(QChar('>'))) {
                QString l = lines.at(i).trimmed().mid(1);
                if (l.startsWith(QChar(' ')))
                    l.remove(0, 1);
                inner.append(l);
                ++i;
            }
            html += QStringLiteral("<blockquote>") + blocksHtml(inner, options) + QStringLiteral("</blockquote>\n");
            continue;
        }
        // A list: items of the same kind, each with the lines indented under it.
        const QRegularExpressionMatch item = listRe().match(line);
        if (item.hasMatch()) {
            const bool ordered = !item.captured(3).isEmpty();
            const QChar kind = item.captured(2).back();
            QString list = ordered ? QStringLiteral("<ol") : QStringLiteral("<ul>");
            if (ordered) {
                const int start = item.captured(3).toInt();
                list += start != 1 ? QStringLiteral(" start=\"") + QString::number(start) + QStringLiteral("\">")
                                   : QStringLiteral(">");
            }
            bool loose = false;
            QStringList items;
            while (i < n) {
                const QRegularExpressionMatch m = listRe().match(lines.at(i));
                if (!m.hasMatch() || m.captured(2).back() != kind || m.captured(3).isEmpty() == ordered)
                    break;
                const int contentIndent = static_cast<int>(m.captured(1).size() + m.captured(2).size() +
                                                           std::max<qsizetype>(1, m.captured(4).size()));
                QStringList content{m.captured(5)};
                ++i;
                while (i < n) {
                    const QString &l = lines.at(i);
                    if (isBlank(l)) {
                        // A blank line goes on with the item only if an indented line follows.
                        qsizetype next = i + 1;
                        while (next < n && isBlank(lines.at(next)))
                            ++next;
                        if (next < n && leadingSpaces(lines.at(next)) >= contentIndent) {
                            loose = true;
                            content.append(QString());
                            ++i;
                            continue;
                        }
                        break;
                    }
                    if (leadingSpaces(l) >= contentIndent) {
                        content.append(l.mid(contentIndent));
                    } else if (!startsBlock(l, options) && !listRe().match(l).hasMatch() && !content.isEmpty() &&
                               !content.back().isEmpty()) {
                        content.append(l.trimmed()); // a lazy continuation of the paragraph
                    } else {
                        break;
                    }
                    ++i;
                }
                QString first = content.isEmpty() ? QString() : content.first();
                QString box;
                if (options.github &&
                    (first.startsWith(QStringLiteral("[ ] ")) || first.startsWith(QStringLiteral("[x] ")) ||
                     first.startsWith(QStringLiteral("[X] ")))) {
                    box = first.at(1) == QChar(' ') ? QString(QChar(0x2610)) + QChar(' ')
                                                    : QString(QChar(0x2611)) + QChar(' ');
                    content.first() = first.mid(4);
                }
                items.append(box + QChar('\x01') + blocksHtml(content, options));
                // A blank line between items makes the list loose.
                if (i < n && isBlank(lines.at(i))) {
                    qsizetype next = i;
                    while (next < n && isBlank(lines.at(next)))
                        ++next;
                    const QRegularExpressionMatch following =
                        next < n ? listRe().match(lines.at(next)) : QRegularExpressionMatch();
                    if (following.hasMatch() && following.captured(2).back() == kind) {
                        loose = true;
                        i = next;
                    }
                }
            }
            for (const QString &entry : std::as_const(items)) {
                const qsizetype mark = entry.indexOf(QChar('\x01'));
                const QString box = entry.left(mark);
                QString body = entry.mid(mark + 1).trimmed();
                // In a tight list the paragraph of an item is no paragraph.
                if (!loose && body.startsWith(QStringLiteral("<p>"))) {
                    const qsizetype end = body.indexOf(QStringLiteral("</p>"));
                    body = body.mid(3, end - 3) + body.mid(end + 4);
                }
                list += QStringLiteral("<li>") + escape(box) + body + QStringLiteral("</li>\n");
            }
            html += list + (ordered ? QStringLiteral("</ol>\n") : QStringLiteral("</ul>\n"));
            continue;
        }
        // Indented code.
        if (leadingSpaces(line) >= 4) {
            QStringList code;
            while (i < n && (leadingSpaces(lines.at(i)) >= 4 || isBlank(lines.at(i)))) {
                code.append(lines.at(i).mid(4));
                ++i;
            }
            while (!code.isEmpty() && isBlank(code.back()))
                code.removeLast();
            html += QStringLiteral("<pre><code>") + escape(code.join(QChar('\n'))) + QStringLiteral("</code></pre>\n");
            continue;
        }
        // A table: a header row, a row of dashes, and the rows that follow.
        if (options.github && line.contains(QChar('|')) && i + 1 < n &&
            delimiterRowRe().match(lines.at(i + 1)).hasMatch()) {
            const QStringList header = tableCells(line);
            const QStringList delimiters = tableCells(lines.at(i + 1));
            if (header.size() == delimiters.size()) {
                QStringList aligns;
                for (const QString &d : delimiters) {
                    const bool left = d.startsWith(QChar(':'));
                    const bool right = d.endsWith(QChar(':'));
                    aligns.append(left && right ? QStringLiteral(" align=\"center\"")
                                  : right       ? QStringLiteral(" align=\"right\"")
                                  : left        ? QStringLiteral(" align=\"left\"")
                                                : QString());
                }
                QString table = QStringLiteral("<table border=\"1\" cellspacing=\"0\" cellpadding=\"4\">\n<tr>");
                for (qsizetype c = 0; c < header.size(); ++c)
                    table += QStringLiteral("<th") + aligns.at(c) + QChar('>') + inlineHtml(header.at(c), options) +
                             QStringLiteral("</th>");
                table += QStringLiteral("</tr>\n");
                i += 2;
                while (i < n && !isBlank(lines.at(i)) && !startsBlock(lines.at(i), options)) {
                    const QStringList cells = tableCells(lines.at(i));
                    table += QStringLiteral("<tr>");
                    for (qsizetype c = 0; c < header.size(); ++c)
                        table += QStringLiteral("<td") + aligns.at(c) + QChar('>') +
                                 inlineHtml(cells.value(c), options) + QStringLiteral("</td>");
                    table += QStringLiteral("</tr>\n");
                    ++i;
                }
                html += table + QStringLiteral("</table>\n");
                continue;
            }
        }
        // HTML that stands on its own lines, up to a blank line.
        if (!options.noHtml && startsHtmlBlock(line)) {
            while (i < n && !isBlank(lines.at(i)))
                html += lines.at(i++) + QChar('\n');
            continue;
        }
        // A paragraph, which a line of = or - under it makes a heading.
        QStringList paragraph{line.mid(leadingSpaces(line))};
        ++i;
        int setext = 0;
        while (i < n && !isBlank(lines.at(i))) {
            const QString t = lines.at(i).trimmed();
            if (!t.isEmpty() && leadingSpaces(lines.at(i)) < 4 && t.count(QChar('=')) == t.size()) {
                setext = 1;
                ++i;
                break;
            }
            if (!t.isEmpty() && leadingSpaces(lines.at(i)) < 4 && t.count(QChar('-')) == t.size()) {
                setext = 2;
                ++i;
                break;
            }
            if (startsBlock(lines.at(i), options))
                break;
            paragraph.append(lines.at(i).mid(leadingSpaces(lines.at(i))));
            ++i;
        }
        const QString body = inlineHtml(paragraph.join(QChar('\n')), options);
        if (setext)
            html += QStringLiteral("<h") + QString::number(setext) + QChar('>') + body + QStringLiteral("</h") +
                    QString::number(setext) + QStringLiteral(">\n");
        else
            html += QStringLiteral("<p>") + body + QStringLiteral("</p>\n");
    }
    return html;
}

} // namespace

QString QtPrivate::markdownToHtml(const QString &markdown, bool github, bool noHtml) {
    QString text = markdown;
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QChar('\r'), QChar('\n'));
    QStringList lines = text.split(QChar('\n'));
    // Tabs at the start of a line count as four spaces.
    for (QString &line : lines) {
        int i = 0;
        QString lead;
        while (i < line.size() && (line.at(i) == QChar(' ') || line.at(i) == QChar('\t'))) {
            lead += line.at(i) == QChar('\t') ? QString(4 - lead.size() % 4, QChar(' ')) : QStringLiteral(" ");
            ++i;
        }
        if (i > 0)
            line = lead + line.mid(i);
    }
    return blocksHtml(lines, Options{github, noHtml});
}
