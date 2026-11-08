// MIT License - see LICENSE
#pragma once

#include <QtCore/qtimer.h>
#include <QtGui/qkeysequence.h>
#include <QtWidgets/qwidget.h>

class QLineEdit;

class QKeySequenceEdit : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QKeySequence keySequence READ keySequence WRITE setKeySequence NOTIFY keySequenceChanged)
    Q_PROPERTY(bool clearButtonEnabled READ isClearButtonEnabled WRITE setClearButtonEnabled)
public:
    explicit QKeySequenceEdit(QWidget *parent = nullptr);
    explicit QKeySequenceEdit(const QKeySequence &keySequence, QWidget *parent = nullptr);
    ~QKeySequenceEdit() override;

    QKeySequence keySequence() const { return m_sequence; }
    void setClearButtonEnabled(bool enable) { m_clearButton = enable; }
    bool isClearButtonEnabled() const { return m_clearButton; }

    QSize sizeHint() const override;

public slots:
    void setKeySequence(const QKeySequence &keySequence);
    void clear();

signals:
    /*!
        This signal is emitted when the user has finished entering the key sequence: a moment after the last key, or
        when the edit loses the focus.
    */
    void editingFinished() { emitSignal(&QKeySequenceEdit::editingFinished); }
    /*!
        This signal is emitted when the key sequence changes to \a keySequence.
    */
    void keySequenceChanged(const QKeySequence &keySequence) {
        emitSignal(&QKeySequenceEdit::keySequenceChanged, keySequence);
    }

protected:
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void finish();

    QLineEdit *m_edit = nullptr;
    QKeySequence m_sequence;
    QTimer m_timer;
    bool m_clearButton = false;
    bool m_recording = false;
};
