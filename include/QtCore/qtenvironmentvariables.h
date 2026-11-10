// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qbytearrayview.h>

class QByteArrayView;
class QString;

// The environment of the process. The functions take a lock, so they are safe to call from several threads as long
// as nothing else in the process changes the environment behind them.

// The value of the variable as the bytes the system has, or a null byte array if it is not set.
QByteArray qgetenv(const char *varName);
// The value of the variable decoded from the local 8-bit encoding, or a null string (or defaultValue) if not set.
QString qEnvironmentVariable(const char *varName);
QString qEnvironmentVariable(const char *varName, const QString &defaultValue);
// Sets the variable; an empty value sets it to the empty string. Returns true on success.
bool qputenv(const char *varName, QByteArrayView value);
// Removes the variable. Returns true on success.
bool qunsetenv(const char *varName);
bool qEnvironmentVariableIsEmpty(const char *varName) noexcept;
bool qEnvironmentVariableIsSet(const char *varName) noexcept;
// The value as a number, in base 10, 16 with a 0x prefix or 8 with a 0 prefix; 0 if it is not one or does not fit.
int qEnvironmentVariableIntValue(const char *varName, bool *ok = nullptr) noexcept;
