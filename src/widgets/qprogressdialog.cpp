// MIT License - see LICENSE
#include <QtWidgets/qprogressdialog.h>

#include <QtCore/qcoreapplication.h>
#include <QtGui/qevent.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qprogressbar.h>
#include <QtWidgets/qpushbutton.h>

#include <algorithm>

/*!
    \class QProgressDialog
    \brief The QProgressDialog class shows the progress of a long task and lets the user cancel it.

    A progress dialog has a label, a progress bar and a Cancel button. It shows itself only if the task takes longer
    than minimumDuration(), four seconds by default, so that short tasks do not flash a dialog. Call setValue() as
    the task goes on, and check wasCanceled():

    \code
    QProgressDialog progress("Copying files...", "Abort", 0, files.size(), window);
    progress.setWindowModality(Qt::WindowModal);
    for (int i = 0; i < files.size(); ++i) {
        progress.setValue(i);
        if (progress.wasCanceled())
            break;
        copy(files[i]);
    }
    progress.setValue(files.size());
    \endcode

    A window-modal progress dialog processes the events of the application in setValue(), so that it is painted and
    its Cancel button works while the task runs in the same thread.

    When the value reaches the maximum the dialog resets and hides, unless autoReset() or autoClose() are turned off.

    \differences
    There is no \c setBar(), \c setLabel() or \c setCancelButton() to replace the parts, and no \c open() with a
    receiver. The texts are not translated.
    \enddifferences

    \sa QProgressBar, QDialog
*/

/*!
    Constructs a progress dialog with the given \a parent and window \a flags, a Cancel button and the range 0 to
    100.
*/
QProgressDialog::QProgressDialog(QWidget *parent, Qt::WindowFlags flags) : QDialog(parent, flags) {
    init(QString(), QStringLiteral("Cancel"), 0, 100);
}

/*!
    Constructs a progress dialog with the given \a labelText, \a cancelButtonText, range \a minimum to \a maximum,
    \a parent and window \a flags. An empty \a cancelButtonText gives a dialog without a Cancel button.
*/
QProgressDialog::QProgressDialog(const QString &labelText, const QString &cancelButtonText, int minimum, int maximum,
                                 QWidget *parent, Qt::WindowFlags flags)
    : QDialog(parent, flags) {
    init(labelText, cancelButtonText, minimum, maximum);
}

/*!
    Destroys the progress dialog.
*/
QProgressDialog::~QProgressDialog() = default;

void QProgressDialog::init(const QString &labelText, const QString &cancelText, int minimum, int maximum) {
    auto *layout = new QVBoxLayout(this);
    m_label = new QLabel(labelText, this);
    m_bar = new QProgressBar(this);
    m_bar->setRange(minimum, maximum);
    m_bar->reset();
    layout->addWidget(m_label);
    layout->addWidget(m_bar);
    m_cancel = new QPushButton(cancelText, this);
    auto *row = new QHBoxLayout;
    row->addStretch();
    row->addWidget(m_cancel);
    layout->addLayout(row);
    m_cancel->setVisible(!cancelText.isEmpty());
    QObject::connect(m_cancel, &QPushButton::clicked, this, &QProgressDialog::canceled);
    QObject::connect(this, &QProgressDialog::canceled, this, &QProgressDialog::cancel);
    m_forceTimer.setSingleShot(true);
    QObject::connect(&m_forceTimer, &QTimer::timeout, this, [this] { forceShow(); });
}

/*!
    \fn bool QProgressDialog::wasCanceled() const

    Returns whether the dialog was canceled, by the user or by cancel().
*/

/*!
    Returns the smallest value, 0 by default.

    \sa setMinimum()
*/
int QProgressDialog::minimum() const { return m_bar->minimum(); }

/*!
    Returns the largest value, 100 by default.

    \sa setMaximum()
*/
int QProgressDialog::maximum() const { return m_bar->maximum(); }

/*!
    Returns the progress, or minimum() - 1 if the dialog was reset.

    \sa setValue()
*/
int QProgressDialog::value() const { return m_bar->value(); }

/*!
    Returns the text of the label.

    \sa setLabelText()
*/
QString QProgressDialog::labelText() const { return m_label->text(); }

/*!
    \fn int QProgressDialog::minimumDuration() const

    Returns how long, in milliseconds, the task must take before the dialog is shown. The default is 4000.

    \sa setMinimumDuration()
*/

/*!
    \fn void QProgressDialog::setAutoReset(bool reset)

    Sets whether the dialog resets when the value reaches the maximum to \a reset.

    \sa autoReset()
*/

/*!
    \fn bool QProgressDialog::autoReset() const

    Returns whether the dialog resets when the value reaches the maximum. The default is \c true.
*/

/*!
    \fn void QProgressDialog::setAutoClose(bool close)

    Sets whether the dialog hides when it resets to \a close.

    \sa autoClose()
*/

/*!
    \fn bool QProgressDialog::autoClose() const

    Returns whether the dialog hides when it resets. The default is \c true.
*/

/*!
    \reimp
*/
QSize QProgressDialog::sizeHint() const {
    const QSize hint = QDialog::sizeHint();
    return QSize(std::max(hint.width(), fontMetrics().horizontalAdvance(QLatin1Char('M')) * 25), hint.height());
}

/*!
    Cancels the dialog: it hides and resets, and wasCanceled() is \c true until it is reset again.

    \sa canceled(), reset()
*/
void QProgressDialog::cancel() {
    const bool close = m_autoClose;
    m_autoClose = true;
    reset();
    m_autoClose = close;
    m_canceled = true;
}

/*!
    Resets the dialog: the progress bar is emptied, wasCanceled() is \c false again, and the dialog hides if
    autoClose() is \c true.

    \sa autoClose(), autoReset()
*/
void QProgressDialog::reset() {
    if (m_autoClose)
        hide();
    m_bar->reset();
    m_canceled = false;
    m_shownOnce = false;
    m_setValueCalled = false;
    m_forceTimer.stop();
}

/*!
    Sets the largest value to \a maximum.

    \sa maximum(), setRange()
*/
void QProgressDialog::setMaximum(int maximum) { m_bar->setMaximum(maximum); }

/*!
    Sets the smallest value to \a minimum.

    \sa minimum(), setRange()
*/
void QProgressDialog::setMinimum(int minimum) { m_bar->setMinimum(minimum); }

/*!
    Sets the range to \a minimum to \a maximum.
*/
void QProgressDialog::setRange(int minimum, int maximum) { m_bar->setRange(minimum, maximum); }

/*!
    Sets the progress to \a progress. The first value at the minimum starts the clock; later values show the dialog
    once the task looks like it will take longer than minimumDuration(). At the maximum the dialog resets if
    autoReset() is \c true. A window-modal dialog processes the pending events.

    \sa value(), minimumDuration()
*/
void QProgressDialog::setValue(int progress) {
    if (m_setValueCalled && progress == m_bar->value())
        return;
    m_bar->setValue(progress);
    if (m_shownOnce) {
        if (isModal() || windowModality() == Qt::WindowModal)
            QCoreApplication::processEvents();
    } else if ((!m_setValueCalled && progress == 0) || progress == m_bar->minimum()) {
        m_started.start();
        m_forceTimer.start(m_minimumDuration);
        m_setValueCalled = true;
        return;
    } else {
        if (!m_setValueCalled) {
            m_started.start();
            m_setValueCalled = true;
        }
        const qint64 elapsed = m_started.elapsed();
        bool needShow = elapsed >= m_minimumDuration;
        if (!needShow && elapsed > 50 && progress > m_bar->minimum()) {
            // Estimate the whole time from the time so far.
            const qint64 total = elapsed * (static_cast<qint64>(m_bar->maximum()) - m_bar->minimum()) /
                                 (static_cast<qint64>(progress) - m_bar->minimum());
            needShow = total >= m_minimumDuration;
        }
        if (needShow)
            forceShow();
    }
    if (progress == m_bar->maximum() && m_autoReset)
        reset();
}

/*!
    Sets the text of the label to \a text.

    \sa labelText()
*/
void QProgressDialog::setLabelText(const QString &text) {
    m_label->setText(text);
    adjustSize();
}

/*!
    Sets the text of the Cancel button to \a text. An empty text hides the button.
*/
void QProgressDialog::setCancelButtonText(const QString &text) {
    m_cancel->setText(text);
    m_cancel->setVisible(!text.isEmpty());
}

/*!
    Sets how long, in milliseconds, the task must take before the dialog is shown to \a ms.

    \sa minimumDuration()
*/
void QProgressDialog::setMinimumDuration(int ms) {
    m_minimumDuration = ms;
    if (m_bar->value() == m_bar->minimum()) {
        m_forceTimer.stop();
        m_forceTimer.start(ms);
    }
}

void QProgressDialog::forceShow() {
    m_forceTimer.stop();
    if (m_shownOnce || m_canceled)
        return;
    show();
    m_shownOnce = true;
}

/*!
    \reimp

    Closing the window cancels the dialog.
*/
void QProgressDialog::closeEvent(QCloseEvent *event) {
    emit canceled();
    event->accept();
}

/*!
    \reimp

    Escape cancels the dialog.
*/
void QProgressDialog::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape && event->modifiers() == Qt::NoModifier) {
        emit canceled();
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

#include "moc_qprogressdialog.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
