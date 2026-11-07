// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <cstdio>
#include <memory>
#include <string>

namespace QTestPrivate {

enum class Event { Fail, XFail, XPass, Skip, Debug, Info, Warning, Critical, Fatal, Note };
enum class Outcome { Pass, Fail, Skip };
enum class Verbosity { Silent, Normal, Verbose };

struct Totals {
    int passed = 0;
    int failed = 0;
    int skipped = 0;
    qint64 elapsedMs = 0;
};

// Receives the results of a run in order. `label` is `Class::function(tag)`.
class Logger {
public:
    virtual ~Logger() = default;
    virtual void begin(const std::string &className) = 0;
    virtual void event(Event event, const std::string &label, const std::string &message,
                       const char *file, int line) = 0;
    virtual void rowFinished(Outcome outcome, const std::string &label, qint64 elapsedMs) = 0;
    virtual void end(const std::string &className, const Totals &totals) = 0;
};

// `format` is one of txt, junitxml or tap; returns nullptr for any other. Takes ownership of
// `output` unless it is stdout.
std::unique_ptr<Logger> createLogger(const std::string &format, std::FILE *output, Verbosity verbosity);

} // namespace QTestPrivate
