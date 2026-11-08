// MIT License - see LICENSE
#include <QtWidgets/qinputdialog.h>

#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qdialogbuttonbox.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qplaintextedit.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qspinbox.h>

/*!
    \class QInputDialog
    \brief The QInputDialog class asks the user for a single value: a text, an item of a list, or a number.

    The static functions show the dialog and wait for the answer; \c ok tells whether the user accepted:

    \code
    bool ok = false;
    const QString name = QInputDialog::getText(window, "Rename", "New name:", QLineEdit::Normal, oldName, &ok);
    if (ok && !name.isEmpty())
        rename(name);
    \endcode

    A QInputDialog can also be set up and shown like any QDialog. The input mode says which field it shows: a line
    edit, a multi-line text, a combo box of items, a QSpinBox or a QDoubleSpinBox.

    \differences
    The combo box of getItem() cannot be edited, whatever \c editable says, and there is no list view
    (\c UseListViewForComboBoxItems shows the combo box). The texts are not translated.
    \enddifferences

    \sa QDialog, QMessageBox
*/

/*!
    \enum QInputDialog::InputDialogOption

    Options for the look of the dialog. InputDialogOptions is a typedef for \c{QFlags<InputDialogOption>}.

    \value NoButtons No OK and Cancel buttons.
    \value UseListViewForComboBoxItems Accepted for compatibility; the items are shown in a combo box.
    \value UsePlainTextEditForTextInput A multi-line text instead of a line edit.
*/

/*!
    \enum QInputDialog::InputMode

    Which value the dialog asks for.

    \value TextInput A text, or an item of comboBoxItems() if there are any.
    \value IntInput An integer.
    \value DoubleInput A floating-point number.
*/

/*!
    Constructs an input dialog for a text with the given \a parent and window \a flags.
*/
QInputDialog::QInputDialog(QWidget *parent, Qt::WindowFlags flags) : QDialog(parent, flags) {
    m_layout = new QVBoxLayout(this);
    m_label = new QLabel(this);
    m_label->setWordWrap(true);
    m_line = new QLineEdit(this);
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_layout->addWidget(m_label);
    m_layout->addWidget(m_line);
    m_layout->addWidget(m_buttons);
    QObject::connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(m_line, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (m_mode == TextInput)
            emit textValueChanged(text);
    });
}

/*!
    Destroys the input dialog.
*/
QInputDialog::~QInputDialog() = default;

/*!
    Sets which value the dialog asks for to \a mode.

    \sa inputMode()
*/
void QInputDialog::setInputMode(InputMode mode) {
    m_mode = mode;
    updateInput();
}

/*!
    \fn QInputDialog::InputMode QInputDialog::inputMode() const

    Returns which value the dialog asks for, TextInput by default.

    \sa setInputMode()
*/

/*!
    Sets the text above the field to \a text.

    \sa labelText()
*/
void QInputDialog::setLabelText(const QString &text) { m_label->setText(text); }

/*!
    Returns the text above the field.

    \sa setLabelText()
*/
QString QInputDialog::labelText() const { return m_label->text(); }

/*!
    Sets \a option if \a on is \c true, and clears it otherwise.

    \sa testOption(), setOptions()
*/
void QInputDialog::setOption(InputDialogOption option, bool on) {
    InputDialogOptions options = m_options;
    options.setFlag(option, on);
    setOptions(options);
}

/*!
    \fn bool QInputDialog::testOption(InputDialogOption option) const

    Returns whether \a option is set.

    \sa setOption()
*/

/*!
    Sets the options of the dialog to \a options.

    \sa options()
*/
void QInputDialog::setOptions(InputDialogOptions options) {
    m_options = options;
    m_buttons->setVisible(!options.testFlag(NoButtons));
    updateInput();
}

/*!
    \fn QInputDialog::InputDialogOptions QInputDialog::options() const

    Returns the options of the dialog.

    \sa setOptions()
*/

/*!
    Sets the text of the field in TextInput mode to \a text, and selects the item with that text if there are items.

    \sa textValue()
*/
void QInputDialog::setTextValue(const QString &text) {
    m_line->setText(text);
    if (m_plain)
        m_plain->setPlainText(text);
    if (m_combo) {
        const int index = m_combo->findText(text);
        if (index >= 0)
            m_combo->setCurrentIndex(index);
    }
}

/*!
    Returns the text of the field in TextInput mode: the text of the line edit or the multi-line text, or the current
    item.

    \sa setTextValue()
*/
QString QInputDialog::textValue() const {
    if (m_combo && !m_items.isEmpty())
        return m_combo->currentText();
    if (m_plain && testOption(UsePlainTextEditForTextInput))
        return m_plain->toPlainText();
    return m_line->text();
}

/*!
    Sets how the line edit shows the text to \a mode, such as QLineEdit::Password.

    \sa textEchoMode()
*/
void QInputDialog::setTextEchoMode(QLineEdit::EchoMode mode) { m_line->setEchoMode(mode); }

/*!
    Returns how the line edit shows the text.

    \sa setTextEchoMode()
*/
QLineEdit::EchoMode QInputDialog::textEchoMode() const { return m_line->echoMode(); }

/*!
    Sets the items to choose from in TextInput mode to \a items. With items the dialog shows a combo box instead of
    a line edit.

    \sa comboBoxItems()
*/
void QInputDialog::setComboBoxItems(const QStringList &items) {
    m_items = items;
    updateInput();
}

/*!
    \fn QStringList QInputDialog::comboBoxItems() const

    Returns the items to choose from.

    \sa setComboBoxItems()
*/

/*!
    \fn void QInputDialog::setComboBoxEditable(bool editable)

    Sets whether the user may type a text that is not an item to \a editable. The combo box of Nt cannot be edited,
    so it only records the setting.

    \sa isComboBoxEditable()
*/

/*!
    \fn bool QInputDialog::isComboBoxEditable() const

    Returns the setting of setComboBoxEditable(), \c true by default.
*/

/*!
    Sets the value in IntInput mode to \a value.

    \sa intValue()
*/
void QInputDialog::setIntValue(int value) {
    updateInput();
    m_int->setValue(value);
}

/*!
    Returns the value in IntInput mode.

    \sa setIntValue()
*/
int QInputDialog::intValue() const { return m_int ? m_int->value() : 0; }

/*!
    Sets the smallest integer to \a min.

    \sa intMinimum(), setIntRange()
*/
void QInputDialog::setIntMinimum(int min) {
    updateInput();
    m_int->setMinimum(min);
}

/*!
    Returns the smallest integer.

    \sa setIntMinimum()
*/
int QInputDialog::intMinimum() const { return m_int ? m_int->minimum() : 0; }

/*!
    Sets the largest integer to \a max.

    \sa intMaximum(), setIntRange()
*/
void QInputDialog::setIntMaximum(int max) {
    updateInput();
    m_int->setMaximum(max);
}

/*!
    Returns the largest integer.

    \sa setIntMaximum()
*/
int QInputDialog::intMaximum() const { return m_int ? m_int->maximum() : 99; }

/*!
    Sets the range of the integer to \a min to \a max.
*/
void QInputDialog::setIntRange(int min, int max) {
    updateInput();
    m_int->setRange(min, max);
}

/*!
    Sets the step of the integer to \a step.

    \sa intStep()
*/
void QInputDialog::setIntStep(int step) {
    updateInput();
    m_int->setSingleStep(step);
}

/*!
    Returns the step of the integer.

    \sa setIntStep()
*/
int QInputDialog::intStep() const { return m_int ? m_int->singleStep() : 1; }

/*!
    Sets the value in DoubleInput mode to \a value.

    \sa doubleValue()
*/
void QInputDialog::setDoubleValue(double value) {
    updateInput();
    m_double->setValue(value);
}

/*!
    Returns the value in DoubleInput mode.

    \sa setDoubleValue()
*/
double QInputDialog::doubleValue() const { return m_double ? m_double->value() : 0.0; }

/*!
    Sets the smallest number to \a min.

    \sa doubleMinimum(), setDoubleRange()
*/
void QInputDialog::setDoubleMinimum(double min) {
    updateInput();
    m_double->setMinimum(min);
}

/*!
    Returns the smallest number.

    \sa setDoubleMinimum()
*/
double QInputDialog::doubleMinimum() const { return m_double ? m_double->minimum() : 0.0; }

/*!
    Sets the largest number to \a max.

    \sa doubleMaximum(), setDoubleRange()
*/
void QInputDialog::setDoubleMaximum(double max) {
    updateInput();
    m_double->setMaximum(max);
}

/*!
    Returns the largest number.

    \sa setDoubleMaximum()
*/
double QInputDialog::doubleMaximum() const { return m_double ? m_double->maximum() : 99.99; }

/*!
    Sets the range of the number to \a min to \a max.
*/
void QInputDialog::setDoubleRange(double min, double max) {
    updateInput();
    m_double->setRange(min, max);
}

/*!
    Sets how many digits after the point the number has to \a decimals.

    \sa doubleDecimals()
*/
void QInputDialog::setDoubleDecimals(int decimals) {
    updateInput();
    m_double->setDecimals(decimals);
}

/*!
    Returns how many digits after the point the number has.

    \sa setDoubleDecimals()
*/
int QInputDialog::doubleDecimals() const { return m_double ? m_double->decimals() : 2; }

/*!
    Sets the step of the number to \a step.

    \sa doubleStep()
*/
void QInputDialog::setDoubleStep(double step) {
    updateInput();
    m_double->setSingleStep(step);
}

/*!
    Returns the step of the number.

    \sa setDoubleStep()
*/
double QInputDialog::doubleStep() const { return m_double ? m_double->singleStep() : 1.0; }

/*!
    Sets the text of the OK button to \a text.

    \sa okButtonText()
*/
void QInputDialog::setOkButtonText(const QString &text) { m_buttons->button(QDialogButtonBox::Ok)->setText(text); }

/*!
    Returns the text of the OK button.

    \sa setOkButtonText()
*/
QString QInputDialog::okButtonText() const { return m_buttons->button(QDialogButtonBox::Ok)->text(); }

/*!
    Sets the text of the Cancel button to \a text.

    \sa cancelButtonText()
*/
void QInputDialog::setCancelButtonText(const QString &text) {
    m_buttons->button(QDialogButtonBox::Cancel)->setText(text);
}

/*!
    Returns the text of the Cancel button.

    \sa setCancelButtonText()
*/
QString QInputDialog::cancelButtonText() const { return m_buttons->button(QDialogButtonBox::Cancel)->text(); }

/*!
    \reimp

    An accepted dialog emits textValueSelected(), intValueSelected() or doubleValueSelected() first.
*/
void QInputDialog::done(int result) {
    if (result == Accepted) {
        switch (m_mode) {
        case TextInput:
            emit textValueSelected(textValue());
            break;
        case IntInput:
            emit intValueSelected(intValue());
            break;
        case DoubleInput:
            emit doubleValueSelected(doubleValue());
            break;
        }
    }
    QDialog::done(result);
}

// Shows the field of the input mode, creating it the first time.
void QInputDialog::updateInput() {
    const bool items = m_mode == TextInput && !m_items.isEmpty();
    const bool plain = m_mode == TextInput && !items && testOption(UsePlainTextEditForTextInput);
    if (items && !m_combo) {
        m_combo = new QComboBox(this);
        m_layout->insertWidget(2, m_combo);
        QObject::connect(m_combo, &QComboBox::currentTextChanged, this, [this](const QString &text) {
            if (m_mode == TextInput)
                emit textValueChanged(text);
        });
    }
    if (m_combo && items) {
        const QString current = m_combo->currentText();
        m_combo->clear();
        m_combo->addItems(m_items);
        const int index = m_combo->findText(current);
        if (index >= 0)
            m_combo->setCurrentIndex(index);
    }
    if (plain && !m_plain) {
        m_plain = new QPlainTextEdit(this);
        m_plain->setPlainText(m_line->text());
        m_layout->insertWidget(2, m_plain);
        QObject::connect(m_plain, &QPlainTextEdit::textChanged, this, [this] {
            if (m_mode == TextInput)
                emit textValueChanged(m_plain->toPlainText());
        });
    }
    if (m_mode == IntInput && !m_int) {
        m_int = new QSpinBox(this);
        m_layout->insertWidget(2, m_int);
        QObject::connect(m_int, &QSpinBox::valueChanged, this, [this](int value) {
            if (m_mode == IntInput)
                emit intValueChanged(value);
        });
    }
    if (m_mode == DoubleInput && !m_double) {
        m_double = new QDoubleSpinBox(this);
        m_layout->insertWidget(2, m_double);
        QObject::connect(m_double, &QDoubleSpinBox::valueChanged, this, [this](double value) {
            if (m_mode == DoubleInput)
                emit doubleValueChanged(value);
        });
    }
    m_line->setVisible(m_mode == TextInput && !items && !plain);
    if (m_combo)
        m_combo->setVisible(items);
    if (m_plain)
        m_plain->setVisible(plain);
    if (m_int)
        m_int->setVisible(m_mode == IntInput);
    if (m_double)
        m_double->setVisible(m_mode == DoubleInput);
}

/*!
    Asks for a text in a dialog with the given \a parent, window \a title, \a label, window \a flags, echo mode
    \a echo and starting \a text, and waits for the user. Returns the text; \a ok, if given, is set to whether the
    user accepted. A rejected dialog returns an empty string.
*/
QString QInputDialog::getText(QWidget *parent, const QString &title, const QString &label, QLineEdit::EchoMode echo,
                              const QString &text, bool *ok, Qt::WindowFlags flags) {
    QInputDialog dialog(parent, flags);
    dialog.setWindowTitle(title);
    dialog.setLabelText(label);
    dialog.setTextValue(text);
    dialog.setTextEchoMode(echo);
    const bool accepted = dialog.exec() == Accepted;
    if (ok)
        *ok = accepted;
    return accepted ? dialog.textValue() : QString();
}

/*!
    Asks for a text of several lines in a dialog with the given \a parent, window \a title, \a label, window
    \a flags and starting \a text, and waits for the user. Returns the text; \a ok, if given, is set to whether the
    user accepted.
*/
QString QInputDialog::getMultiLineText(QWidget *parent, const QString &title, const QString &label, const QString &text,
                                       bool *ok, Qt::WindowFlags flags) {
    QInputDialog dialog(parent, flags);
    dialog.setWindowTitle(title);
    dialog.setLabelText(label);
    dialog.setOption(UsePlainTextEditForTextInput);
    dialog.setTextValue(text);
    const bool accepted = dialog.exec() == Accepted;
    if (ok)
        *ok = accepted;
    return accepted ? dialog.textValue() : QString();
}

/*!
    Asks the user to choose one of \a items in a dialog with the given \a parent, window \a title, \a label and window
    \a flags, starting at the item \a current, and waits. \a editable is recorded but the list cannot be edited.
    Returns the item; \a ok, if given, is set to whether the user accepted. A rejected dialog returns the item at
    \a current.
*/
QString QInputDialog::getItem(QWidget *parent, const QString &title, const QString &label, const QStringList &items,
                              int current, bool editable, bool *ok, Qt::WindowFlags flags) {
    const QString start = current >= 0 && current < items.size() ? items.at(current) : QString();
    QInputDialog dialog(parent, flags);
    dialog.setWindowTitle(title);
    dialog.setLabelText(label);
    dialog.setComboBoxItems(items);
    dialog.setComboBoxEditable(editable);
    dialog.setTextValue(start);
    const bool accepted = dialog.exec() == Accepted;
    if (ok)
        *ok = accepted;
    return accepted ? dialog.textValue() : start;
}

/*!
    Asks for an integer in a dialog with the given \a parent, window \a title, \a label and window \a flags, from
    \a minValue to \a maxValue with the \a step, starting at \a value, and waits for the user. Returns the integer;
    \a ok, if given, is set to whether the user accepted. A rejected dialog returns \a value.
*/
int QInputDialog::getInt(QWidget *parent, const QString &title, const QString &label, int value, int minValue,
                         int maxValue, int step, bool *ok, Qt::WindowFlags flags) {
    QInputDialog dialog(parent, flags);
    dialog.setWindowTitle(title);
    dialog.setLabelText(label);
    dialog.setInputMode(IntInput);
    dialog.setIntRange(minValue, maxValue);
    dialog.setIntValue(value);
    dialog.setIntStep(step);
    const bool accepted = dialog.exec() == Accepted;
    if (ok)
        *ok = accepted;
    return accepted ? dialog.intValue() : value;
}

/*!
    Asks for a number in a dialog with the given \a parent, window \a title, \a label and window \a flags, from
    \a minValue to \a maxValue with \a decimals digits after the point and the \a step, starting at \a value, and
    waits for the user. Returns the number; \a ok, if given, is set to whether the user accepted. A rejected dialog
    returns \a value.
*/
double QInputDialog::getDouble(QWidget *parent, const QString &title, const QString &label, double value,
                               double minValue, double maxValue, int decimals, bool *ok, Qt::WindowFlags flags,
                               double step) {
    QInputDialog dialog(parent, flags);
    dialog.setWindowTitle(title);
    dialog.setLabelText(label);
    dialog.setInputMode(DoubleInput);
    dialog.setDoubleDecimals(decimals);
    dialog.setDoubleRange(minValue, maxValue);
    dialog.setDoubleValue(value);
    dialog.setDoubleStep(step);
    const bool accepted = dialog.exec() == Accepted;
    if (ok)
        *ok = accepted;
    return accepted ? dialog.doubleValue() : value;
}

#include "moc_qinputdialog.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
