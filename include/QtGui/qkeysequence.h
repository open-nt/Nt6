// MIT License - see LICENSE
#pragma once

#include <QtCore/qkeycombination.h>
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
    QKeySequence(QKeyCombination k1) : QKeySequence(k1.toCombined()) {}
    QKeySequence(StandardKey key);

    int count() const { return m_key != 0 ? 1 : 0; }
    bool isEmpty() const { return m_key == 0; }
    QString toString(SequenceFormat format = PortableText) const;
    static QKeySequence fromString(const QString &str, SequenceFormat format = PortableText);
    SequenceMatch matches(const QKeySequence &seq) const;
    static QList<QKeySequence> keyBindings(StandardKey key);
    static QKeySequence mnemonic(const QString &text);

    QKeyCombination operator[](uint index) const { return QKeyCombination::fromCombined(index == 0 ? m_key : 0); }
    bool operator==(const QKeySequence &other) const { return m_key == other.m_key; }
    bool operator!=(const QKeySequence &other) const { return m_key != other.m_key; }
    bool operator<(const QKeySequence &other) const { return m_key < other.m_key; }

private:
    int m_key = 0;
};

// The hash of `key`, combined with `seed`, so a key sequence can be a key of QHash and QSet.
inline std::size_t qHash(const QKeySequence &key, std::size_t seed = 0) noexcept {
    return seed ^ (static_cast<std::size_t>(static_cast<unsigned>(key[0])) * std::size_t{0x9e3779b9u});
}
