// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtCore/qstring.h>

class QPlainTextEdit;

class QTextCursor {
public:
    enum MoveMode { MoveAnchor, KeepAnchor };
    enum MoveOperation {
        NoMove,
        Start,
        Up,
        StartOfLine,
        StartOfBlock,
        StartOfWord,
        PreviousBlock,
        PreviousCharacter,
        PreviousWord,
        Left,
        WordLeft,
        End,
        Down,
        EndOfLine,
        EndOfWord,
        EndOfBlock,
        NextBlock,
        NextCharacter,
        NextWord,
        Right,
        WordRight
    };
    enum SelectionType { WordUnderCursor, LineUnderCursor, BlockUnderCursor, Document };

    QTextCursor() = default;

    bool isNull() const { return m_edit.isNull(); }
    int position() const { return m_position; }
    int anchor() const { return m_anchor; }
    void setPosition(int pos, MoveMode mode = MoveAnchor);
    bool movePosition(MoveOperation operation, MoveMode mode = MoveAnchor, int n = 1);
    bool atStart() const;
    bool atEnd() const;
    bool atBlockStart() const;
    bool atBlockEnd() const;
    int blockNumber() const;
    int columnNumber() const;
    int positionInBlock() const { return columnNumber(); }

    bool hasSelection() const { return m_position != m_anchor; }
    int selectionStart() const { return m_position < m_anchor ? m_position : m_anchor; }
    int selectionEnd() const { return m_position < m_anchor ? m_anchor : m_position; }
    QString selectedText() const;
    void clearSelection();
    void select(SelectionType selection);

    void insertText(const QString &text);
    void insertBlock();
    void removeSelectedText();
    void deleteChar();
    void deletePreviousChar();
    void beginEditBlock();
    void endEditBlock();

    bool operator==(const QTextCursor &other) const {
        return m_edit == other.m_edit && m_position == other.m_position && m_anchor == other.m_anchor;
    }
    bool operator!=(const QTextCursor &other) const { return !(*this == other); }

private:
    friend class QPlainTextEdit;
    QTextCursor(QPlainTextEdit *edit, int position, int anchor);

    QPointer<QObject> m_edit;
    int m_position = 0;
    int m_anchor = 0;
};
