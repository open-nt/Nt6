// MIT License - see LICENSE
#pragma once

#include <QtCore/qregularexpression.h>
#include <QtCore/qurl.h>
#include <QtGui/qtextdocument.h>
#include <QtGui/qtextformat.h>
#include <QtGui/qtextoption.h>
#include <QtWidgets/qabstractscrollarea.h>
#include <QtWidgets/qtextcursor.h>

#include <vector>

class QMenu;
class QMimeData;
class QPagedPaintDevice;

class QPlainTextEdit : public QAbstractScrollArea {
    Q_OBJECT
    Q_PROPERTY(QString plainText READ toPlainText WRITE setPlainText NOTIFY textChanged USER true)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText)
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
    Q_PROPERTY(LineWrapMode lineWrapMode READ lineWrapMode WRITE setLineWrapMode)
    Q_PROPERTY(bool tabChangesFocus READ tabChangesFocus WRITE setTabChangesFocus)
    Q_PROPERTY(qreal tabStopDistance READ tabStopDistance WRITE setTabStopDistance)
    Q_PROPERTY(bool undoRedoEnabled READ isUndoRedoEnabled WRITE setUndoRedoEnabled)
    Q_PROPERTY(bool overwriteMode READ overwriteMode WRITE setOverwriteMode)
    Q_PROPERTY(int cursorWidth READ cursorWidth WRITE setCursorWidth)
    Q_PROPERTY(Qt::TextInteractionFlags textInteractionFlags READ textInteractionFlags WRITE setTextInteractionFlags)
    Q_PROPERTY(bool backgroundVisible READ backgroundVisible WRITE setBackgroundVisible)
    Q_PROPERTY(bool centerOnScroll READ centerOnScroll WRITE setCenterOnScroll)
    Q_PROPERTY(int maximumBlockCount READ maximumBlockCount WRITE setMaximumBlockCount)
    Q_PROPERTY(int blockCount READ blockCount)
public:
    enum LineWrapMode { NoWrap, WidgetWidth, FixedPixelWidth, FixedColumnWidth };
    Q_ENUM(LineWrapMode)

    // A part of the text drawn with a format of its own, such as the line of the cursor; see setExtraSelections().
    struct ExtraSelection {
        QTextCursor cursor;
        QTextCharFormat format;
    };

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
    int maximumBlockCount() const { return m_maxBlocks; }
    void setMaximumBlockCount(int maximum);
    QTextDocument *document() const;

    QTextCursor textCursor() const;
    void setTextCursor(const QTextCursor &cursor);
    QTextCursor cursorForPosition(const QPoint &pos) const;
    QRect cursorRect() const;
    QRect cursorRect(const QTextCursor &cursor) const;
    void moveCursor(QTextCursor::MoveOperation operation, QTextCursor::MoveMode mode = QTextCursor::MoveAnchor);
    void ensureCursorVisible();
    bool find(const QString &exp, QTextDocument::FindFlags options = QTextDocument::FindFlags());
    bool find(const QRegularExpression &exp, QTextDocument::FindFlags options = QTextDocument::FindFlags());

    bool overwriteMode() const { return m_overwrite; }
    void setOverwriteMode(bool overwrite) { m_overwrite = overwrite; }
    int cursorWidth() const { return m_cursorWidth; }
    void setCursorWidth(int width);
    Qt::TextInteractionFlags textInteractionFlags() const;
    void setTextInteractionFlags(Qt::TextInteractionFlags flags);
    QTextOption::WrapMode wordWrapMode() const { return m_wordWrap; }
    void setWordWrapMode(QTextOption::WrapMode policy);
    int lineWrapColumnOrWidth() const { return m_wrapColumnOrWidth; }
    void setLineWrapColumnOrWidth(int w);
    bool backgroundVisible() const { return m_backgroundVisible; }
    void setBackgroundVisible(bool visible);
    bool centerOnScroll() const { return m_centerOnScroll; }
    void setCenterOnScroll(bool enabled) { m_centerOnScroll = enabled; }
    QString documentTitle() const { return m_title; }
    void setDocumentTitle(const QString &title) { m_title = title; }
    QString anchorAt(const QPoint &pos) const;
    bool canPaste() const;
    QMenu *createStandardContextMenu();
    QMenu *createStandardContextMenu(const QPoint &position);
    QTextCharFormat currentCharFormat() const { return m_charFormat; }
    void setCurrentCharFormat(const QTextCharFormat &format) { m_charFormat = format; }
    void mergeCurrentCharFormat(const QTextCharFormat &modifier) { m_charFormat.merge(modifier); }
    void setExtraSelections(const QList<ExtraSelection> &selections);
    QList<ExtraSelection> extraSelections() const { return m_extraSelections; }
    virtual QVariant loadResource(int type, const QUrl &name);
    QVariant inputMethodQuery(Qt::InputMethodQuery property) const override;
    void print(QPagedPaintDevice *printer) const;

public slots:
    void setPlainText(const QString &text);
    void appendPlainText(const QString &text);
    void insertPlainText(const QString &text);
    void clear();
    void selectAll();
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    void appendHtml(const QString &html);
    void centerCursor();
    void zoomIn(int range = 1);
    void zoomOut(int range = 1);

public:
    void zoomInF(float range);

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
    /*!
        \since 6.0
        This signal is emitted when text is selected, with \a yes \c true, or no longer is: when copy() and cut()
        start or stop having something to do.
    */
    void copyAvailable(bool yes) { emitSignal(&QPlainTextEdit::copyAvailable, yes); }
    /*!
        \since 6.0
        This signal is emitted when the text becomes modified or unmodified, with \a changed \c true for modified;
        see QTextDocument::isModified().
    */
    void modificationChanged(bool changed) { emitSignal(&QPlainTextEdit::modificationChanged, changed); }
    /*!
        \since 6.0
        This signal is emitted when the part \a rect of the viewport needs to be painted again, and when the text
        scrolls, by \a dy pixels down. A line number area at the side of the editor follows the text with it.
    */
    void updateRequest(const QRect &rect, int dy) { emitSignal(&QPlainTextEdit::updateRequest, rect, dy); }

protected:
    QTextBlock firstVisibleBlock() const;
    QRectF blockBoundingGeometry(const QTextBlock &block) const;
    QRectF blockBoundingRect(const QTextBlock &block) const;
    QPointF contentOffset() const;
    QAbstractTextDocumentLayout::PaintContext getPaintContext() const;
    virtual QMimeData *createMimeDataFromSelection() const;
    virtual bool canInsertFromMimeData(const QMimeData *source) const;
    virtual void insertFromMimeData(const QMimeData *source);
    virtual void doSetTextCursor(const QTextCursor &cursor);
    void contextMenuEvent(QContextMenuEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
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
    friend class QTextEdit;
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
    void setModifiedFlag(bool modified);
    void selectionDidChange(); // emits selectionChanged(), and copyAvailable() if there is a selection now or no more
    int wrapLimit() const;     // the width at which lines wrap
    QString selectedText() const;

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
    int m_maxBlocks = 0;                    // setMaximumBlockCount(); 0 for no limit
    mutable QTextDocument *m_plainDocument = nullptr; // document(): a child that follows the text
    mutable bool m_documentStale = true;
    bool m_syncing = false;
    bool trimBlocks(); // drops the first blocks beyond maximumBlockCount(); true if it did
    bool m_overwrite = false;
    int m_cursorWidth = 1;
    Qt::TextInteractionFlags m_interaction = Qt::TextEditorInteraction;
    QTextOption::WrapMode m_wordWrap = QTextOption::WrapAtWordBoundaryOrAnywhere;
    int m_wrapColumnOrWidth = 0;
    bool m_backgroundVisible = false;
    bool m_centerOnScroll = false;
    QString m_title;
    QTextCharFormat m_charFormat;
    QList<ExtraSelection> m_extraSelections;
    bool m_modified = false;
    bool m_hadSelection = false; // for copyAvailable()
};
