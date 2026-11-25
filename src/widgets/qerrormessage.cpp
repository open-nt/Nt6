// MIT License - see LICENSE
#include <QtWidgets/qerrormessage.h>

#include <QtCore/qlogging.h>
#include <QtCore/qpointer.h>

#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qcheckbox.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qpushbutton.h>

/*!
    \class QErrorMessage
    \brief The QErrorMessage class shows error messages that the user can choose not to see again.

    An error message dialog shows one message at a time, with a "Show this message again" check box. A message that
    the user unchecks is not shown again; messages that come while one is shown wait their turn:

    \code
    auto *errors = new QErrorMessage(window);
    errors->showMessage("The printer is out of paper.");
    \endcode

    With a type, the check box applies to all the messages of that type.

    An error message with the check box that lets the user hide it next time.

    \image widgets/qerrormessage.png QErrorMessage in the light color scheme

    \differences
    There is no \c qtHandler(), which would show the messages of qDebug(), qWarning() and the others. The texts
    are not translated, and the message is plain text.
    \enddifferences

    \sa QMessageBox
*/

/*!
    Constructs an error message dialog with the given \a parent.
*/
QErrorMessage::QErrorMessage(QWidget *parent) : QDialog(parent) {
    auto *layout = new QVBoxLayout(this);
    m_label = new QLabel(this);
    m_label->setWordWrap(true);
    m_again = new QCheckBox(QStringLiteral("Show this message again"), this);
    m_again->setChecked(true);
    m_ok = new QPushButton(QStringLiteral("OK"), this);
    m_ok->setDefault(true);
    auto *row = new QHBoxLayout;
    row->addWidget(m_again);
    row->addStretch();
    row->addWidget(m_ok);
    layout->addWidget(m_label);
    layout->addLayout(row);
    QObject::connect(m_ok, &QPushButton::clicked, this, &QDialog::accept);
}

/*!
    Destroys the dialog.
*/
QErrorMessage::~QErrorMessage() = default;

/*!
    Shows \a message, unless the user asked not to see it again. If a message is shown already, \a message waits until
    it is closed; the same message is not queued twice.
*/
void QErrorMessage::showMessage(const QString &message) { showMessage(message, QString()); }

/*!
    \overload

    Shows \a message of the given \a type. Unchecking "Show this message again" hides all the messages of the type.
*/
void QErrorMessage::showMessage(const QString &message, const QString &type) {
    if (m_doNotShow.count(type.isEmpty() ? message : QLatin1Char('\t') + type))
        return;
    for (const auto &pending : m_pending)
        if (pending.first == message && pending.second == type)
            return;
    if (isVisible() && m_current == message && m_currentType == type)
        return;
    m_pending.emplace_back(message, type);
    if (!isVisible())
        showNext();
}

// Shows the next message that waits; false if there is none.
bool QErrorMessage::showNext() {
    while (!m_pending.empty()) {
        const auto next = m_pending.front();
        m_pending.erase(m_pending.begin());
        if (m_doNotShow.count(next.second.isEmpty() ? next.first : QLatin1Char('\t') + next.second))
            continue;
        m_current = next.first;
        m_currentType = next.second;
        m_label->setText(m_current);
        m_again->setChecked(true);
        adjustSize();
        show();
        return true;
    }
    m_current.clear();
    m_currentType.clear();
    return false;
}

/*!
    \reimp

    Closing a message remembers whether to show it again, and shows the next one that waits.
*/
void QErrorMessage::done(int result) {
    if (!m_again->isChecked() && !m_current.isEmpty())
        m_doNotShow.insert(m_currentType.isEmpty() ? m_current : QLatin1Char('\t') + m_currentType);
    QDialog::done(result);
    showNext();
}

namespace {
QPointer<QErrorMessage> g_qtHandler;
QtMessageHandler g_previousHandler = nullptr;

// Shows the warnings, criticals and fatal messages of Qt in the error message of qtHandler().
void showQtMessage(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    if (g_previousHandler)
        g_previousHandler(type, context, message);
    if (!g_qtHandler || type == QtDebugMsg || type == QtInfoMsg)
        return;
    const QString kind = type == QtWarningMsg    ? QErrorMessage::tr("Warning:")
                         : type == QtCriticalMsg ? QErrorMessage::tr("Critical Error:")
                                                 : QErrorMessage::tr("Fatal Error:");
    g_qtHandler->showMessage(kind + QLatin1Char(' ') + message);
}
} // namespace

/*!
    \since 6.0

    Returns an error message that shows the warnings and errors that the program reports with qWarning() and the
    other message functions, made and installed as the message handler the first time it is called. Messages also
    go to the handler that was installed before.
*/
QErrorMessage *QErrorMessage::qtHandler() {
    if (!g_qtHandler) {
        g_qtHandler = new QErrorMessage;
        g_qtHandler->setWindowTitle(tr("Qt Message"));
        g_previousHandler = qInstallMessageHandler(showQtMessage);
    }
    return g_qtHandler.data();
}

#include "moc_qerrormessage.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
