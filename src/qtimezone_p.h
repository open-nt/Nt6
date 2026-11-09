// MIT License - see LICENSE
#pragma once

// The data behind QTimeZone, shared with QDateTime. A zone is UTC, a fixed offset, or a set of
// transitions read from an IANA TZif file, followed by the POSIX TZ rule in the file's footer.

#include <QtCore/qbytearray.h>
#include <QtCore/qglobal.h>
#include <QtCore/qlocale.h>
#include <QtCore/qstring.h>

#include <memory>
#include <string>
#include <vector>

struct QTimeZonePrivate {
    enum class Kind { Local, Utc, Fixed, Tzif };

    // One time type of a zone: an offset from UTC, whether it is daylight time and its abbreviation.
    struct Type {
        int offset = 0;
        bool dst = false;
        std::string abbreviation;
    };

    // A date of a POSIX TZ rule: "Mm.w.d", "Jn" (1..365, no leap day) or "n" (0..365).
    struct RuleDate {
        enum class Form { MonthWeekDay, JulianNoLeap, ZeroBasedDay } form = Form::MonthWeekDay;
        int month = 0, week = 0, weekday = 0, day = 0;
        int time = 7200; // seconds after local midnight
    };

    // The POSIX TZ rule that continues a zone after its last transition.
    struct Rule {
        bool valid = false;
        Type standard;
        bool hasDst = false;
        Type daylight;
        RuleDate start, end; // daylight time starts and ends; the times are in the time then in effect
    };

    // The offsets in effect at an instant.
    struct Offsets {
        int offset = 0;
        int standardOffset = 0;
        bool dst = false;
        std::string abbreviation;
    };

    Kind kind = Kind::Utc;
    QByteArray id;
    int fixedOffset = 0;
    QString name;                 // custom zones: the display name
    QString abbreviationOverride; // custom zones: the abbreviation
    QString comment;
    QLocale::Territory territory = QLocale::AnyTerritory;

    std::vector<qint64> transitions; // seconds since the epoch, sorted
    std::vector<unsigned char> transitionTypes;
    std::vector<Type> types;
    Rule rule;

    // Returns the offsets in effect at utcSecs.
    Offsets offsetsAt(qint64 utcSecs) const;
    // Returns the UTC instant (seconds) of the local time localSecs (seconds since the epoch as if
    // it were UTC). A time in a gap is interpreted with the offset before the gap; in a fold, the
    // earlier instant is chosen.
    qint64 toUtc(qint64 localSecs) const;
    // Returns the transitions in [fromSecs, toSecs], including those that the rule generates.
    std::vector<std::pair<qint64, Offsets>> transitionsBetween(qint64 fromSecs, qint64 toSecs) const;
    bool hasDaylightTime() const;
    bool hasTransitions() const { return kind == Kind::Tzif || kind == Kind::Local; }

    // Loads the zone called id; returns nullptr if it is unknown. Results are cached.
    static std::shared_ptr<const QTimeZonePrivate> load(const QByteArray &id);
    static std::shared_ptr<const QTimeZonePrivate> utc();
    static std::shared_ptr<const QTimeZonePrivate> local();
    static std::shared_ptr<const QTimeZonePrivate> fixed(int offsetSeconds);
    static QByteArray systemId();
    static QString offsetName(int offsetSeconds);
    static QString zoneDirectory();
    // Parses a POSIX TZ rule such as "CET-1CEST,M3.5.0,M10.5.0/3".
    static bool parseRule(const std::string &text, Rule *rule);
};
