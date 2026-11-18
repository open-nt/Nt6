// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qstring.h>

namespace QtPrivate {

// Matches the texts of items against a text the way the Qt::MatchFlags of findItems() say. Matching ignores case
// unless the flags have Qt::MatchCaseSensitive or are Qt::MatchExactly.
class ItemTextMatcher {
public:
    ItemTextMatcher(const QString &text, Qt::MatchFlags flags)
        : m_text(text), m_type(static_cast<int>(flags) & 0x0f),
          m_cs((flags & Qt::MatchCaseSensitive) || m_type == Qt::MatchExactly ? Qt::CaseSensitive
                                                                               : Qt::CaseInsensitive) {
        if (m_type == Qt::MatchRegularExpression || m_type == Qt::MatchWildcard) {
            // A wildcard has to match the whole text; a regular expression matches anywhere unless it is anchored.
            const QString pattern =
                m_type == Qt::MatchWildcard ? QRegularExpression::wildcardToRegularExpression(text) : text;
            m_regex = QRegularExpression(pattern, m_cs == Qt::CaseInsensitive
                                                      ? QRegularExpression::CaseInsensitiveOption
                                                      : QRegularExpression::NoPatternOption);
        }
    }

    bool operator()(const QString &candidate) const {
        switch (m_type) {
        case Qt::MatchContains:
            return candidate.contains(m_text, m_cs);
        case Qt::MatchStartsWith:
            return candidate.startsWith(m_text, m_cs);
        case Qt::MatchEndsWith:
            return candidate.endsWith(m_text, m_cs);
        case Qt::MatchRegularExpression:
        case Qt::MatchWildcard:
            return m_regex.match(candidate).hasMatch();
        default: // MatchExactly and MatchFixedString
            return candidate.compare(m_text, m_cs) == 0;
        }
    }

private:
    QString m_text;
    int m_type;
    Qt::CaseSensitivity m_cs;
    QRegularExpression m_regex;
};

} // namespace QtPrivate
