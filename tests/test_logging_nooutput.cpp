// MIT License - see LICENSE
//
// Compiled with the QT_NO_*_OUTPUT macros, which must remove the statements.
#define QT_NO_DEBUG_OUTPUT
#define QT_NO_INFO_OUTPUT
#define QT_NO_WARNING_OUTPUT
#include <QtCore>

Q_DECLARE_LOGGING_CATEGORY(lcNoOutput)
Q_LOGGING_CATEGORY(lcNoOutput, "no.output")

int noOutputCompiledOut(int *evaluated);

int noOutputCompiledOut(int *evaluated) {
    static int delivered = 0;
    const QtMessageHandler old = qInstallMessageHandler(+[](QtMsgType, const QMessageLogContext &, const QString &) { ++delivered; });
    // The operands of a plain stream are still evaluated, but nothing is written. A
    // category macro skips its statement, operands included.
    qDebug() << "x" << ++*evaluated;
    qInfo() << "x";
    qWarning() << "x";
    qDebug("x %d", 1);
    qInfo("x %d", 1);
    qWarning("x %d", 1);
    qCDebug(lcNoOutput) << "x" << ++*evaluated;
    qCInfo(lcNoOutput, "x");
    qCWarning(lcNoOutput) << "x";
    qInstallMessageHandler(old);
    return delivered;
}
