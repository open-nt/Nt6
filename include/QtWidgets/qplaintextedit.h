// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qabstractscrollarea.h>
#include <QtWidgets/qtextcursor.h>

#include <vector>

class QPlainTextEdit : public QAbstractScrollArea {
    Q_OBJECT
    Q_PROPERTY(QString plainText READ toPlainText WRITE setPlainText NOTIFY textChanged)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText)
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
    Q_PROPERTY(LineWrapMode lineWrapMode READ lineWrapMode WRITE setLineWrapMode)
    Q_PROPERTY(bool tabChangesFocus READ tabChangesFocus WRITE setTabChangesFocus)
    Q_PROPERTY(qreal tabStopDistance READ tabStopDistance WRITE setTabStopDistance)
    Q_PROPERTY(bool undoRedoEnabled READ isUndoRedoEnabled WRITE setUndoRedoEnabled)
public:
    enum LineWrapMode { NoWrap, WidgetWidth };
    Q_ENUM(LineWrapMode)

    explicit QPlainTextEdit(QWidget *parent = nullptr);
    explicit QPlainTextEdit(const QString &text, QWidget *parent = nullptr);
    ~QPlainTextEdit() override;

    QString toPlainText() const { return m_text; }
    QString placeholderText() const { return m_placeholder; }
    void setPlaceholderText(const QString &text);
    bool isReadOnly() const { return m_readOnly; }
    void setReadOnly(bool readOnly);
    LineWrapMode lineWrapMode() const { return m_wrap; }
    void setLineWrapMode(LineWrapMode mode);
    bool tabChangesFocus() const { return m_tabChangesFocus; }
    void setTabChangesFocus(bool enable) { m_tabChangesFocus = enable; }
    qreal tabStopDistance() const { return m_tabStop; }
    void setTabStopDistance(qreal distance);
    bool isUndoRedoEnabled() const { return m_undoEnabled; }
    void setUndoRedoEnabled(bool enable);
    int blockCount() const;

    QTextCursor textCursor() const;
    void setTextCursor(const QTextCursor &cursor);
    QTextCursor cursorForPosition(const QPoint &pos) const;
    QRect cursorRect() const;
    QRect cursorRect(const QTextCursor &cursor) const;
    void moveCursor(QTextCursor::MoveOperation operation, QTextCursor::MoveMode mode = QTextCursor::MoveAnchor);
    void ensureCursorVisible();

public slots:
    void setPlainText(const QString &text);
    void appendPlainText(const QString &text);
    void insertPlainText(const QString &text);
    void clear();
    void selectAll();
    void undo();
    void redo();

signals:
    /*!
        This signal is emitted when the text changes.
    */
    void textChanged() { emitSignal(&QPlainTextEdit::textChanged); }
    /*!
        This signal is emitted when the position of the cursor changes.
    */
    void cursorPositionChanged() { emitSignal(&QPlainTextEdit::cursorPositionChanged); }
    /*!
        This signal is emitted when the selection changes.
    */
    void selectionChanged() { emitSignal(&QPlainTextEdit::selectionChanged); }
    /*!
        This signal is emitted when undo() becomes possible, with \a available \c true, or impossible.
    */
    void undoAvailable(bool available) { emitSignal(&QPlainTextEdit::undoAvailable, available); }
    /*!
        This signal is emitted when redo() becomes possible, with \a available \c true, or impossible.
    */
    void redoAvailable(bool available) { emitSignal(&QPlainTextEdit::redoAvailable, available); }
    /*!
        This signal is emitted when the number of blocks (lines separated by a new line) changes to \a newBlockCount.
    */
    void blockCountChanged(int newBlockCount) { emitSignal(&QPlainTextEdit::blockCountChanged, newBlockCount); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool focusNextPrevChild(bool next) override;

private:
    friend class QTextCursor;
    struct VLine {
        int start;
        int length;
        int block;
    };
    struct Snapshot {
        QString text;
        int position;
        int anchor;
    };

    // Text and its layout
    void layoutText();
    int advance(int start, int length, int x0 = 0) const;
    int lineHeight() const;
    int margin() const { return 4; }
    int vlineOf(int position) const;
    int xOf(int position) const;
    int offsetAtX(const VLine &line, int x) const;
    int positionAt(const QPoint &viewportPos) const;
    int blockStart(int block) const;
    int blockEnd(int block) const;
    int blockOf(int position) const;
    int wordStart(int position) const;
    int wordEnd(int position) const;
    int moved(int position, QTextCursor::MoveOperation operation, int n, int *goalX) const;

    // Editing
    void applyEdit(int from, int to, const QString &text, bool typing = false, int cursorAfter = -1);
    void setCursor(int position, bool extend);
    void setSelection(int anchor, int position);
    void pushUndo();
    void restore(const Snapshot &snapshot);
    void notifyUndo();
    void didChange(bool textDidChange, int oldPosition, int oldAnchor);

    QString m_text;
    QString m_placeholder;
    int m_position = 0;
    int m_anchor = 0;
    int m_goalX = -1;
    int m_blocks = 1;
    std::vector<VLine> m_lines;
    std::vector<int> m_blockStarts;
    std::vector<Snapshot> m_undo;
    std::vector<Snapshot> m_redo;
    int m_editBlock = 0;
    int m_lastTypingEnd = -1;
    int m_pressClicks = 0;
    bool m_readOnly = false;
    LineWrapMode m_wrap = WidgetWidth;
    bool m_tabChangesFocus = false;
    bool m_undoEnabled = true;
    qreal m_tabStop = 80;
    bool m_selecting = false;
    bool m_laying = false;
    bool m_layAgain = false;
    int m_scrollPosition = -1;
    bool m_undoAvailable = false;
    bool m_redoAvailable = false;
};
