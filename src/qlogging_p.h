// MIT License - see LICENSE
#pragma once

#include <QtCore/qlogging.h>

// Delivers a finished message: applies the default category's filter, calls the installed
// message handler or writes to stderr, and aborts for fatal messages.
void qt_message_output(QtMsgType type, const QMessageLogContext &context, const QString &message);
