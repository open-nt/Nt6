// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>

namespace QtPrivate {

// Turns Markdown into the HTML that QTextDocument reads. `github` adds the GitHub extensions (tables,
// strikethrough, task lists, autolinks); with `noHtml` HTML in the Markdown is shown as text.
QString markdownToHtml(const QString &markdown, bool github, bool noHtml);

} // namespace QtPrivate
