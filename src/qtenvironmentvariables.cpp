// MIT License - see LICENSE
#include <QtCore/qstring.h>
#include <QtCore/qtenvironmentvariables.h>

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <mutex>

namespace {

// getenv() and setenv() are not safe to mix from several threads, so every access takes this lock.
std::mutex &environmentMutex() {
    static std::mutex mutex;
    return mutex;
}

bool isSpace(char c) { return c == ' ' || (c >= '\t' && c <= '\r'); }

} // namespace

QByteArray qgetenv(const char *varName) {
    const std::lock_guard<std::mutex> lock(environmentMutex());
    const char *value = std::getenv(varName);
    return value ? QByteArray(value) : QByteArray();
}

QString qEnvironmentVariable(const char *varName) { return qEnvironmentVariable(varName, QString()); }

QString qEnvironmentVariable(const char *varName, const QString &defaultValue) {
    const std::lock_guard<std::mutex> lock(environmentMutex());
    const char *value = std::getenv(varName);
    return value ? QString::fromLocal8Bit(value) : defaultValue;
}

bool qputenv(const char *varName, QByteArrayView value) {
    // The view need not end with a zero.
    const QByteArray copy(value.data(), value.size());
    const std::lock_guard<std::mutex> lock(environmentMutex());
#ifdef _WIN32
    return _putenv_s(varName, copy.constData()) == 0;
#else
    return setenv(varName, copy.constData(), 1) == 0;
#endif
}

bool qunsetenv(const char *varName) {
    const std::lock_guard<std::mutex> lock(environmentMutex());
#ifdef _WIN32
    return _putenv_s(varName, "") == 0;
#else
    return unsetenv(varName) == 0;
#endif
}

bool qEnvironmentVariableIsEmpty(const char *varName) noexcept {
    const std::lock_guard<std::mutex> lock(environmentMutex());
    const char *value = std::getenv(varName);
    return !value || !*value;
}

bool qEnvironmentVariableIsSet(const char *varName) noexcept {
    const std::lock_guard<std::mutex> lock(environmentMutex());
    return std::getenv(varName) != nullptr;
}

int qEnvironmentVariableIntValue(const char *varName, bool *ok) noexcept {
    if (ok)
        *ok = false;
    const std::lock_guard<std::mutex> lock(environmentMutex());
    const char *value = std::getenv(varName);
    if (!value)
        return 0;
    errno = 0;
    char *end = nullptr;
    const long long number = std::strtoll(value, &end, 0);
    if (end == value || errno == ERANGE || number < INT_MIN || number > INT_MAX)
        return 0;
    while (isSpace(*end))
        ++end;
    if (*end)
        return 0;
    if (ok)
        *ok = true;
    return static_cast<int>(number);
}
