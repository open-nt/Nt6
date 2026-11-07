// MIT License - see LICENSE
#include "qtestlog_p.h"

#include <QtCore/qconfig.h>

#include <utility>
#include <vector>

namespace QTestPrivate {

namespace {

struct RecordedEvent {
    Event event;
    std::string message;
    std::string file;
    int line;
};

const char *prefix(Event event) {
    switch (event) {
    case Event::Fail: return "FAIL!  ";
    case Event::XFail: return "XFAIL  ";
    case Event::XPass: return "XPASS  ";
    case Event::Skip: return "SKIP   ";
    case Event::Debug: return "QDEBUG ";
    case Event::Info: return "QINFO  ";
    case Event::Warning: return "QWARN  ";
    case Event::Critical: return "QCRITICAL";
    case Event::Fatal: return "QFATAL ";
    case Event::Note: return "INFO   ";
    }
    return "";
}

bool isFailure(Event event) { return event == Event::Fail || event == Event::XPass; }
bool isDiagnostic(Event event) {
    return event == Event::Debug || event == Event::Info || event == Event::Warning ||
           event == Event::Critical || event == Event::Fatal;
}

std::string location(const std::string &file, int line) {
    return file + "(" + std::to_string(line) + ")";
}

class FileLogger : public Logger {
public:
    explicit FileLogger(std::FILE *output) : m_output(output) {}
    ~FileLogger() override {
        if (m_output != stdout)
            std::fclose(m_output);
    }
    FileLogger(const FileLogger &) = delete;
    FileLogger &operator=(const FileLogger &) = delete;

protected:
    // Written at once and flushed, so that the log is complete up to a crash.
    void write(const std::string &text) {
        std::fwrite(text.data(), 1, text.size(), m_output);
        std::fflush(m_output);
    }

private:
    std::FILE *m_output;
};

class TextLogger final : public FileLogger {
public:
    TextLogger(std::FILE *output, Verbosity verbosity) : FileLogger(output), m_verbosity(verbosity) {}

    void begin(const std::string &className) override {
        write("********* Start testing of " + className + " *********\n");
        write("Config: Using Nt Test " QT_VERSION_STR "\n");
    }

    void event(Event event, const std::string &label, const std::string &message, const char *file,
               int line) override {
        const bool shown = m_verbosity == Verbosity::Verbose
                               ? true
                               : (m_verbosity == Verbosity::Normal ? event != Event::Note
                                                                   : isFailure(event) || event == Event::Fatal);
        if (!shown)
            return;
        std::string text = std::string(prefix(event)) + ": " + label;
        if (!message.empty())
            text += " " + message;
        text += "\n";
        if (file)
            text += "   Loc: [" + location(file, line) + "]\n";
        write(text);
    }

    void rowFinished(Outcome outcome, const std::string &label, qint64) override {
        if (outcome == Outcome::Pass && m_verbosity != Verbosity::Silent)
            write("PASS   : " + label + "\n");
    }

    void end(const std::string &className, const Totals &totals) override {
        write("Totals: " + std::to_string(totals.passed) + " passed, " + std::to_string(totals.failed) +
              " failed, " + std::to_string(totals.skipped) + " skipped, 0 blacklisted, " +
              std::to_string(totals.elapsedMs) + "ms\n");
        write("********* Finished testing of " + className + " *********\n");
    }

private:
    Verbosity m_verbosity;
};

class TapLogger final : public FileLogger {
public:
    using FileLogger::FileLogger;

    void begin(const std::string &className) override { write("TAP version 13\n# " + className + "\n"); }

    void event(Event event, const std::string &, const std::string &message, const char *file,
               int line) override {
        m_events.push_back({event, message, file ? file : "", line});
    }

    void rowFinished(Outcome outcome, const std::string &label, qint64) override {
        ++m_count;
        std::string head = (outcome == Outcome::Fail ? "not ok " : "ok ") + std::to_string(m_count) + " - " + label;
        std::string details;
        for (const RecordedEvent &e : m_events) {
            if (outcome == Outcome::Skip && e.event == Event::Skip)
                head += " # SKIP " + firstLine(e.message);
            else if (e.event == Event::XFail)
                write("# XFAIL " + firstLine(e.message) + "\n");
            else if (isFailure(e.event))
                details += yamlBlock(e);
            else if (isDiagnostic(e.event))
                write("# " + std::string(prefix(e.event)) + ": " + firstLine(e.message) + "\n");
        }
        m_events.clear();
        write(head + "\n");
        if (!details.empty())
            write("  ---\n" + details + "  ...\n");
    }

    void end(const std::string &, const Totals &totals) override {
        write("1.." + std::to_string(m_count) + "\n");
        write("# tests " + std::to_string(m_count) + "\n# pass " + std::to_string(totals.passed) +
              "\n# fail " + std::to_string(totals.failed) + "\n");
    }

private:
    static std::string firstLine(const std::string &text) { return text.substr(0, text.find('\n')); }

    static std::string yamlBlock(const RecordedEvent &e) {
        std::string text = "  message: |\n    ";
        for (char c : e.message) {
            text += c;
            if (c == '\n')
                text += "    ";
        }
        text += "\n";
        if (!e.file.empty())
            text += "  at: " + e.file + ":" + std::to_string(e.line) + "\n";
        return text;
    }

    int m_count = 0;
    std::vector<RecordedEvent> m_events;
};

// JUnit needs the totals in the opening tag, so everything is written at the end.
class JUnitLogger final : public FileLogger {
public:
    using FileLogger::FileLogger;

    void begin(const std::string &) override {}

    void event(Event event, const std::string &, const std::string &message, const char *file,
               int line) override {
        if (isDiagnostic(event) || event == Event::XFail)
            m_systemErr += std::string(prefix(event)) + ": " + message + "\n";
        if (event != Event::Note && !isDiagnostic(event))
            m_events.push_back({event, message, file ? file : "", line});
    }

    void rowFinished(Outcome, const std::string &label, qint64 elapsedMs) override {
        m_body += "  <testcase name=\"" + escape(label) + "\" time=\"" + seconds(elapsedMs) + "\"";
        std::string inner;
        for (const RecordedEvent &e : m_events) {
            if (isFailure(e.event)) {
                const std::string where = e.file.empty() ? std::string() : "\n" + location(e.file, e.line);
                inner += "    <failure type=\"" + std::string(e.event == Event::XPass ? "xpass" : "fail") +
                         "\" message=\"" + escape(e.message.substr(0, e.message.find('\n'))) + "\">" +
                         escape(e.message + where) + "</failure>\n";
            } else if (e.event == Event::Skip) {
                inner += "    <skipped message=\"" + escape(e.message) + "\"/>\n";
            }
        }
        m_events.clear();
        ++m_count;
        m_body += inner.empty() ? "/>\n" : ">\n" + inner + "  </testcase>\n";
    }

    void end(const std::string &className, const Totals &totals) override {
        std::string text = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<testsuite name=\"" + escape(className) +
                           "\" tests=\"" + std::to_string(m_count) + "\" failures=\"" +
                           std::to_string(totals.failed) + "\" errors=\"0\" skipped=\"" +
                           std::to_string(totals.skipped) + "\" time=\"" + seconds(totals.elapsedMs) + "\">\n";
        text += m_body;
        if (!m_systemErr.empty())
            text += "  <system-err>" + escape(m_systemErr) + "</system-err>\n";
        write(text + "</testsuite>\n");
    }

private:
    static std::string seconds(qint64 ms) {
        char text[32];
        std::snprintf(text, sizeof text, "%.3f", static_cast<double>(ms) / 1000.0);
        return text;
    }

    static std::string escape(const std::string &text) {
        std::string out;
        for (char c : text) {
            switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            case '\n': case '\t': case '\r': out += c; break;
            default:
                // Other control characters cannot appear in XML 1.0, even escaped.
                if (static_cast<unsigned char>(c) >= 0x20)
                    out += c;
            }
        }
        return out;
    }

    int m_count = 0;
    std::string m_body;
    std::string m_systemErr;
    std::vector<RecordedEvent> m_events;
};

} // namespace

std::unique_ptr<Logger> createLogger(const std::string &format, std::FILE *output, Verbosity verbosity) {
    if (format == "txt")
        return std::make_unique<TextLogger>(output, verbosity);
    if (format == "tap")
        return std::make_unique<TapLogger>(output);
    if (format == "junitxml")
        return std::make_unique<JUnitLogger>(output);
    return nullptr;
}

} // namespace QTestPrivate
