// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qstring.h>

class QKeySequence {
public:
    enum StandardKey {
        UnknownKey,
        HelpContents,
        Open,
        Close,
        Save,
        New,
        Delete,
        Cut,
        Copy,
        Paste,
        Undo,
        Redo,
        Back,
        Forward,
        Refresh,
        ZoomIn,
        ZoomOut,
        Print,
        Find,
        FindNext,
        FindPrevious,
        Replace,
        SelectAll,
        Quit,
        SaveAs,
        Cancel,
        FullScreen,
        NextChild,
        PreviousChild
    };
    enum SequenceFormat { NativeText, PortableText };
    enum SequenceMatch { NoMatch, PartialMatch, ExactMatch };

    QKeySequence() = default;
    QKeySequence(const QString &key, SequenceFormat format = NativeText);
    QKeySequence(int k1);
    QKeySequence(StandardKey key);

    int count() const { return m_key != 0 ? 1 : 0; }
    bool isEmpty() const { return m_key == 0; }
    QString toString(SequenceFormat format = PortableText) const;
    static QKeySequence fromString(const QString &str, SequenceFormat format = PortableText);
    SequenceMatch matches(const QKeySequence &seq) const;
    static QList<QKeySequence> keyBindings(StandardKey key);
    static QKeySequence mnemonic(const QString &text);

    int operator[](uint index) const { return index == 0 ? m_key : 0; }
    bool operator==(const QKeySequence &other) const { return m_key == other.m_key; }
    bool operator!=(const QKeySequence &other) const { return m_key != other.m_key; }
    bool operator<(const QKeySequence &other) const { return m_key < other.m_key; }

private:
    int m_key = 0;
};
