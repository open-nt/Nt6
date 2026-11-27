// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtGui/qvalidator.h>
#include <QtWidgets/qcompleter.h>
#include <QtWidgets/qwidget.h>

#include <vector>

class QMenu;
class QStyleOptionFrame;

class QLineEdit : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged USER true)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText)
    Q_PROPERTY(int maxLength READ maxLength WRITE setMaxLength)
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
    Q_PROPERTY(bool frame READ hasFrame WRITE setFrame)
    Q_PROPERTY(bool clearButtonEnabled READ isClearButtonEnabled WRITE setClearButtonEnabled)
    Q_PROPERTY(int cursorPosition READ cursorPosition WRITE setCursorPosition)
    Q_PROPERTY(bool acceptableInput READ hasAcceptableInput)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment)
    Q_PROPERTY(bool modified READ isModified WRITE setModified)
    Q_PROPERTY(QString inputMask READ inputMask WRITE setInputMask)
    Q_PROPERTY(bool dragEnabled READ dragEnabled WRITE setDragEnabled)
    Q_PROPERTY(bool undoAvailable READ isUndoAvailable)
    Q_PROPERTY(bool redoAvailable READ isRedoAvailable)
    Q_PROPERTY(QString selectedText READ selectedText)
    Q_PROPERTY(QString displayText READ displayText)
public:
    enum EchoMode { Normal, NoEcho, Password };
    enum ActionPosition { LeadingPosition, TrailingPosition };

    explicit QLineEdit(QWidget *parent = nullptr);
    explicit QLineEdit(const QString &text, QWidget *parent = nullptr);
    ~QLineEdit() override;

    QString text() const;
    QString displayText() const;
    QString placeholderText() const { return m_placeholder; }
    void setCompleter(QCompleter *completer);
    QCompleter *completer() const { return m_completer.data(); }
    void setValidator(const QValidator *validator);
    const QValidator *validator() const { return m_validator.data(); }
    bool hasAcceptableInput() const;
    void setPlaceholderText(const QString &text);
    int maxLength() const { return m_maxLength; }
    void setMaxLength(int length);
    bool isReadOnly() const { return m_readOnly; }
    void setReadOnly(bool readOnly);
    bool isClearButtonEnabled() const { return m_clearButton; }
    using QWidget::addAction;
    void addAction(QAction *action, ActionPosition position);
    QAction *addAction(const QIcon &icon, ActionPosition position);
    void setClearButtonEnabled(bool enable);
    bool hasFrame() const { return m_frame; }
    void setFrame(bool frame);
    EchoMode echoMode() const { return m_echoMode; }
    void setEchoMode(EchoMode mode);
    int cursorPosition() const { return m_cursor; }
    void setCursorPosition(int position);
    int cursorPositionAt(const QPoint &pos) const;
    bool hasSelectedText() const { return m_cursor != m_anchor; }
    QString selectedText() const;
    int selectionStart() const;
    int selectionEnd() const;
    void setSelection(int start, int length);
    int selectionLength() const { return selectionEnd() - selectionStart(); }
    Qt::Alignment alignment() const { return m_alignment; }
    void setAlignment(Qt::Alignment flag);
    bool isModified() const { return m_modified; }
    void setModified(bool modified) { m_modified = modified; }
    bool isUndoAvailable() const { return !m_readOnly && !m_undo.empty(); }
    bool isRedoAvailable() const { return !m_readOnly && !m_redo.empty(); }
    QString inputMask() const { return m_inputMask; }
    void setInputMask(const QString &inputMask);
    bool dragEnabled() const { return m_dragEnabled; }
    void setDragEnabled(bool b) { m_dragEnabled = b; }
    Qt::CursorMoveStyle cursorMoveStyle() const { return m_cursorMoveStyle; }
    void setCursorMoveStyle(Qt::CursorMoveStyle style) { m_cursorMoveStyle = style; }
    void setTextMargins(int left, int top, int right, int bottom) { setTextMargins(QMargins(left, top, right, bottom)); }
    void setTextMargins(const QMargins &margins);
    QMargins textMargins() const { return m_textMargins; }
    void cursorForward(bool mark, int steps = 1);
    void cursorBackward(bool mark, int steps = 1);
    void cursorWordForward(bool mark);
    void cursorWordBackward(bool mark);
    QMenu *createStandardContextMenu();
    QVariant inputMethodQuery(Qt::InputMethodQuery property) const override;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setText(const QString &text);
    void clear();
    void selectAll();
    void deselect();
    void insert(const QString &text);
    void backspace();
    void del();
    void home(bool mark = false);
    void end(bool mark = false);
    void cut();
    void copy() const;
    void paste();
    void undo();
    void redo();

signals:
    /*!
        This signal is emitted when the text changes to \a text, whether the user edited it or setText() did.
    */
    void textChanged(const QString &text) { emitSignal(&QLineEdit::textChanged, text); }
    /*!
        This signal is emitted when the user edited the text, and it changed to \a text. It is not emitted for
        changes that setText() makes.
    */
    void textEdited(const QString &text) { emitSignal(&QLineEdit::textEdited, text); }
    /*!
        This signal is emitted when Return or Enter is pressed.
    */
    void returnPressed() { emitSignal(&QLineEdit::returnPressed); }
    /*!
        This signal is emitted when Return or Enter is pressed, or the line edit loses the focus.
    */
    void editingFinished() { emitSignal(&QLineEdit::editingFinished); }
    /*!
        This signal is emitted when the cursor moves from \a oldPos to \a newPos.
    */
    void cursorPositionChanged(int oldPos, int newPos) {
        emitSignal(&QLineEdit::cursorPositionChanged, oldPos, newPos);
    }
    /*!
        This signal is emitted when the selection changes.
    */
    void selectionChanged() { emitSignal(&QLineEdit::selectionChanged); }
    /*!
        \since 6.0

        This signal is emitted when the validator() rejects a key the user typed, or an edit.
    */
    void inputRejected() { emitSignal(&QLineEdit::inputRejected); }

protected:
    void initStyleOption(QStyleOptionFrame *option) const;
    QRect cursorRect() const;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void actionEvent(QActionEvent *event) override;

private:
    friend class QMaterialStyle; // which puts the label of a text field after its leading icons
    QRect contentsRect() const;
    // The actions at the ends that are shown, each with the room it takes, from the outside in.
    struct Side {
        QAction *action;
        ActionPosition position;
        QRect slot; // where a press triggers it
        QRect icon; // where its icon is, against the outer side of the slot
    };
    std::vector<Side> sides() const;
    QRect textArea() const; // the style's area for the text, before the ends take their room
    int sideShift() const;  // how far the icons go out of that area, toward the edges
    QString affix(bool suffix) const;
    QRect clearButtonRect() const;
    bool clearButtonShown() const;
    int xOf(int position) const;
    void moveCursor(int position, bool mark);
    void changeText(const QString &newText, int cursor, bool userEdit);
    enum ChangeFlag { UserEdit = 1, Validate = 2, Record = 4 };
    void applyText(const QString &newText, int cursor, int flags);
    int textOrigin() const; // x of the start of the text, in the widget
    int wordStart(int from) const;
    int wordEnd(int from) const;
    // Input masks
    struct MaskChar {
        QChar ch;               // the mask character, or the literal of a separator
        bool separator = false; // a literal that the user does not type
        int caseMode = 0;       // 1 upper, 2 lower
    };
    QString maskedText(const QString &plain) const;
    QString strippedText(const QString &shown) const;
    bool maskAccepts(qsizetype position, QChar c) const;
    void scrollToCursor();
    bool finishEditing(); // fixes the text up if needed; true if it is acceptable

    QString m_text;
    QString m_placeholder;
    int m_cursor = 0;
    int m_anchor = 0;
    int m_scroll = 0;
    int m_maxLength = 32767;
    bool m_readOnly = false;
    bool m_frame = true;
    bool m_dragging = false;
    bool m_clearButton = false;
    bool m_clearPressed = false;
    std::vector<std::pair<QPointer<QAction>, ActionPosition>> m_sideActions;
    QPointer<QAction> m_sidePressed;
    EchoMode m_echoMode = Normal;
    QPointer<QCompleter> m_completer;
    QPointer<const QValidator> m_validator;
    bool m_deleting = false; // the key being handled deletes text
    struct Edit {
        QString text;
        int cursor = 0;
    };
    std::vector<Edit> m_undo;
    std::vector<Edit> m_redo;
    bool m_modified = false;
    bool m_dragEnabled = false;
    Qt::Alignment m_alignment = Qt::AlignLeft | Qt::AlignVCenter;
    Qt::CursorMoveStyle m_cursorMoveStyle = Qt::LogicalMoveStyle;
    QMargins m_textMargins;
    QString m_inputMask;
    std::vector<MaskChar> m_mask;
    QChar m_blank = QLatin1Char(' ');
};
