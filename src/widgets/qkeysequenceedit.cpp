// MIT License - see LICENSE
#include <QtWidgets/qkeysequenceedit.h>

#include <QtGui/qevent.h>
#include <QtWidgets/qlineedit.h>

#include <algorithm>

namespace {

constexpr int kFinishDelay = 1000; // milliseconds after the last key

bool isModifierKey(int key) {
    return key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt || key == Qt::Key_Meta;
}

} // namespace

/*!
    \class QKeySequenceEdit
    \brief The QKeySequenceEdit class lets the user enter a key sequence, such as a shortcut.

    The edit shows the key sequence. When it has the focus, the next key the user presses, with its modifiers,
    becomes the key sequence, and editingFinished() follows a moment later:

    \code
    auto *edit = new QKeySequenceEdit(action->shortcut(), dialog);
    connect(edit, &QKeySequenceEdit::editingFinished, [=] { action->setShortcut(edit->keySequence()); });
    \endcode

    Key sequence edits: empty, showing its placeholder, and holding two shortcuts.

    \image widgets/qkeysequenceedit.png QKeySequenceEdit in the light color scheme

    \differences
    A key sequence of Nt has one key, so the edit records one key with its modifiers rather than up to four, whatever
    maximumSequenceLength() is. The clear button setting is recorded but no button is shown.
    \enddifferences

    \sa QKeySequence, QAction::setShortcut()
*/

/*!
    Constructs an empty key sequence edit with the given \a parent.
*/
QKeySequenceEdit::QKeySequenceEdit(QWidget *parent) : QWidget(parent), m_edit(new QLineEdit(this)) {
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_edit->setFocusPolicy(Qt::NoFocus);
    m_edit->setReadOnly(true);
    m_edit->setPlaceholderText(QStringLiteral("Press shortcut"));
    m_timer.setSingleShot(true);
    QObject::connect(&m_timer, &QTimer::timeout, this, [this] { finish(); });
}

/*!
    Constructs a key sequence edit with the given \a keySequence and \a parent.
*/
QKeySequenceEdit::QKeySequenceEdit(const QKeySequence &keySequence, QWidget *parent) : QKeySequenceEdit(parent) {
    setKeySequence(keySequence);
}

/*!
    Destroys the edit.
*/
QKeySequenceEdit::~QKeySequenceEdit() = default;

/*!
    \fn QKeySequence QKeySequenceEdit::keySequence() const

    Returns the key sequence.

    \sa setKeySequence()
*/

/*!
    \fn void QKeySequenceEdit::setClearButtonEnabled(bool enable)

    Sets whether the edit shows a button that clears it to \a enable; recorded only.
*/

/*!
    \fn bool QKeySequenceEdit::isClearButtonEnabled() const

    Returns whether the edit shows a clear button. The default is \c false.
*/

/*!
    Sets the key sequence to \a keySequence and emits keySequenceChanged() if it changes.

    \sa keySequence()
*/
void QKeySequenceEdit::setKeySequence(const QKeySequence &keySequence) {
    m_edit->setText(keySequence.toString(QKeySequence::NativeText));
    if (m_sequence == keySequence)
        return;
    m_sequence = keySequence;
    emit keySequenceChanged(keySequence);
}

/*!
    Clears the key sequence.
*/
void QKeySequenceEdit::clear() { setKeySequence(QKeySequence()); }

/*!
    \reimp
*/
QSize QKeySequenceEdit::sizeHint() const { return m_edit->sizeHint(); }

/*!
    \reimp

    Tab and Shift+Tab are recorded too, rather than moving the focus.
*/
bool QKeySequenceEdit::event(QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab) {
            keyPressEvent(key);
            return true;
        }
    }
    return QWidget::event(event);
}

/*!
    \reimp

    A key that is not a modifier, with the modifiers that are held, becomes the key sequence.
*/
void QKeySequenceEdit::keyPressEvent(QKeyEvent *event) {
    const int key = event->key();
    if (key == 0 || key == Qt::Key_unknown || isModifierKey(key)) {
        event->accept();
        return;
    }
    Qt::KeyboardModifiers mods =
        event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    // Shift with a printable character is in the character already, as in Qt.
    if (!event->text().isEmpty() && event->text().at(0).isPrint() && !event->text().at(0).isLetterOrNumber() &&
        mods == Qt::ShiftModifier)
        mods = Qt::NoModifier;
    // A finishing key ends the recording and is not recorded.
    if (m_finishing.contains(QKeyCombination::fromCombined(key | static_cast<int>(mods)))) {
        if (m_recording)
            finish();
        event->accept();
        return;
    }
    m_recording = true;
    setKeySequence(QKeySequence(key | static_cast<int>(mods)));
    m_timer.start(kFinishDelay);
    event->accept();
}

/*!
    \reimp
*/
void QKeySequenceEdit::keyReleaseEvent(QKeyEvent *event) { event->accept(); }

/*!
    \reimp
*/
void QKeySequenceEdit::focusOutEvent(QFocusEvent *event) {
    if (m_recording)
        finish();
    QWidget::focusOutEvent(event);
}

/*!
    \reimp
*/
void QKeySequenceEdit::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    m_edit->setGeometry(rect());
}

void QKeySequenceEdit::finish() {
    m_timer.stop();
    if (!m_recording)
        return;
    m_recording = false;
    emit editingFinished();
}

/*!
    \fn qsizetype QKeySequenceEdit::maximumSequenceLength() const
    \since 6.0

    Returns the most keys a recorded sequence may have. Nt key sequences have one key, so a sequence ends after its
    first key whatever this is; the default is 4.
*/

/*!
    \since 6.0

    Sets the most keys a recorded sequence may have to \a maximum, from 1 to 4.
*/
void QKeySequenceEdit::setMaximumSequenceLength(qsizetype maximum) {
    m_maximumLength = std::clamp<qsizetype>(maximum, 1, 4);
}

/*!
    \fn QList<QKeyCombination> QKeySequenceEdit::finishingKeyCombinations() const
    \since 6.0

    Returns the keys that end the recording without being recorded.
*/

/*!
    \fn void QKeySequenceEdit::setFinishingKeyCombinations(const QList<QKeyCombination> &finishingKeyCombinations)
    \since 6.0

    Makes \a finishingKeyCombinations end the recording, such as Tab, without being recorded.
*/

#include "moc_qkeysequenceedit.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
