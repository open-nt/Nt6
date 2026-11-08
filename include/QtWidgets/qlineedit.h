// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtWidgets/qcompleter.h>
#include <QtWidgets/qwidget.h>

class QLineEdit : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText)
    Q_PROPERTY(int maxLength READ maxLength WRITE setMaxLength)
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
    Q_PROPERTY(bool frame READ hasFrame WRITE setFrame)
    Q_PROPERTY(int cursorPosition READ cursorPosition WRITE setCursorPosition)
public:
    enum EchoMode { Normal, NoEcho, Password };

    explicit QLineEdit(QWidget *parent = nullptr);
    explicit QLineEdit(const QString &text, QWidget *parent = nullptr);
    ~QLineEdit() override;

    QString text() const { return m_text; }
    QString displayText() const;
    QString placeholderText() const { return m_placeholder; }
    void setCompleter(QCompleter *completer);
    QCompleter *completer() const { return m_completer.data(); }
    void setPlaceholderText(const QString &text);
    int maxLength() const { return m_maxLength; }
    void setMaxLength(int length);
    bool isReadOnly() const { return m_readOnly; }
    void setReadOnly(bool readOnly);
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

protected:
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

private:
    QRect contentsRect() const;
    int xOf(int position) const;
    void moveCursor(int position, bool mark);
    void changeText(const QString &text, int cursor, bool userEdit);
    void scrollToCursor();

    QString m_text;
    QString m_placeholder;
    int m_cursor = 0;
    int m_anchor = 0;
    int m_scroll = 0;
    int m_maxLength = 32767;
    bool m_readOnly = false;
    bool m_frame = true;
    bool m_dragging = false;
    EchoMode m_echoMode = Normal;
    QPointer<QCompleter> m_completer;
    bool m_deleting = false; // the key being handled deletes text
};
