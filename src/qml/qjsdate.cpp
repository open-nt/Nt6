// MIT License - see LICENSE
//
// Date and JSON of the JavaScript runtime. A Date holds milliseconds since the epoch, NaN when invalid;
// QDateTime does the calendar and time zone arithmetic.
#include "qjsbuiltins_p.h"

#include <QtCore/qdatetime.h>

#include <cmath>
#include <limits>

namespace QtJSPrivate {

namespace {

constexpr double kMaxTime = 8.64e15;
const double kNaN = std::numeric_limits<double>::quiet_NaN();

double timeClip(double t) {
    if (!std::isfinite(t) || std::fabs(t) > kMaxTime)
        return kNaN;
    return std::trunc(t) + 0.0;
}

QDateTime toDateTime(double ms, bool utc) {
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(ms), utc ? Qt::UTC : Qt::LocalTime);
}

struct Parts {
    double year, month, day, hour, minute, second, ms;
};

Parts split(double t, bool utc) {
    const QDateTime dt = toDateTime(t, utc);
    const QDate d = dt.date();
    const QTime tm = dt.time();
    return {static_cast<double>(d.year()),  static_cast<double>(d.month() - 1), static_cast<double>(d.day()),
            static_cast<double>(tm.hour()), static_cast<double>(tm.minute()),   static_cast<double>(tm.second()),
            static_cast<double>(tm.msec())};
}

// Milliseconds for a date and time given in parts that may overflow: month 13 is January of the next year.
double compose(const Parts &p, bool utc) {
    for (const double v : {p.year, p.month, p.day, p.hour, p.minute, p.second, p.ms})
        if (!std::isfinite(v))
            return kNaN;
    const double year = std::trunc(p.year) + std::floor(std::trunc(p.month) / 12.0);
    const double month = std::fmod(std::trunc(p.month), 12.0) + (std::fmod(std::trunc(p.month), 12.0) < 0 ? 12 : 0);
    if (std::fabs(year) > 400000)
        return kNaN;
    const QDate first(static_cast<int>(year), static_cast<int>(month) + 1, 1);
    if (!first.isValid())
        return kNaN;
    const double dayOffset = std::trunc(p.day) - 1;
    if (std::fabs(dayOffset) > 1e9)
        return kNaN;
    const QDate date = first.addDays(static_cast<qint64>(dayOffset));
    const QDateTime midnight(date, QTime(0, 0), utc ? Qt::UTC : Qt::LocalTime);
    const double t = static_cast<double>(midnight.toMSecsSinceEpoch()) + std::trunc(p.hour) * 3600000.0 +
                     std::trunc(p.minute) * 60000.0 + std::trunc(p.second) * 1000.0 + std::trunc(p.ms);
    return timeClip(t);
}

double parseDate(const QString &input) {
    const QString s = input.trimmed();
    QDateTime dt = QDateTime::fromString(s, Qt::ISODateWithMs);
    if (dt.isValid()) {
        // A date without a time is UTC; a date and time without an offset is local.
        if (s.size() <= 10 || (!s.contains(u'T') && !s.contains(u' ')))
            dt = QDateTime(dt.date(), dt.time(), Qt::UTC);
        return static_cast<double>(dt.toMSecsSinceEpoch());
    }
    dt = QDateTime::fromString(s, Qt::RFC2822Date);
    if (dt.isValid())
        return static_cast<double>(dt.toMSecsSinceEpoch());
    dt = QDateTime::fromString(s, Qt::TextDate);
    if (dt.isValid())
        return static_cast<double>(dt.toMSecsSinceEpoch());
    // What Date.prototype.toString() writes: "Mon Mar 02 2026 08:00:00 GMT+0100".
    const qsizetype gmt = s.indexOf(QLatin1String(" GMT"));
    if (gmt > 0) {
        const QDateTime local = QDateTime::fromString(s.left(gmt), QStringLiteral("ddd MMM dd yyyy hh:mm:ss"));
        const QString offset = s.mid(gmt + 4, 5);
        if (local.isValid() && offset.size() == 5) {
            bool ok = false;
            const int hhmm = offset.mid(1).toInt(&ok);
            if (ok) {
                const int seconds = ((hhmm / 100) * 3600 + (hhmm % 100) * 60) * (offset.at(0) == u'-' ? -1 : 1);
                const QDateTime utc(local.date(), local.time(), Qt::UTC);
                return static_cast<double>(utc.toMSecsSinceEpoch()) - seconds * 1000.0;
            }
        }
    }
    return kNaN;
}

double thisTime(Runtime &runtime, const Value &self) {
    if (self.isObject() && self.asObject()->cls == Object::Class::Date)
        return self.asObject()->primitive.asNumber();
    runtime.throwTypeError(QStringLiteral("this is not a Date object"));
}

QString offsetString(const QDateTime &dt) {
    const int offset = dt.offsetFromUtc() / 60;
    const int a = std::abs(offset);
    return QStringLiteral("GMT%1%2%3")
        .arg(offset < 0 ? QLatin1Char('-') : QLatin1Char('+'))
        .arg(a / 60, 2, 10, QLatin1Char('0'))
        .arg(a % 60, 2, 10, QLatin1Char('0'));
}

QString isoString(double t) {
    const QDateTime dt = toDateTime(t, true);
    const int year = dt.date().year();
    QString y = year >= 0 && year <= 9999
                    ? QStringLiteral("%1").arg(year, 4, 10, QLatin1Char('0'))
                    : QStringLiteral("%1%2").arg(year < 0 ? u'-' : u'+').arg(std::abs(year), 6, 10, QLatin1Char('0'));
    return y + dt.toString(QStringLiteral("-MM-ddTHH:mm:ss.zzz")) + u'Z';
}

void installDateImpl(Runtime &runtime) {
    Object *proto = runtime.newObject();
    runtime.dateProto = proto;
    const auto make = [](Runtime &rt, double t) {
        Object *d = rt.heap.make<Object>(rt.dateProto, Object::Class::Date);
        d->primitive = Value::number(timeClip(t));
        return d;
    };
    Function *ctor = addConstructor(
        runtime, QStringLiteral("Date"), 7, proto,
        [make](Runtime &rt, const Value &, const Values &args, Object *newTarget) {
            const double now = static_cast<double>(QDateTime::currentMSecsSinceEpoch());
            if (!newTarget)
                return Value::string(toDateTime(now, false).toString(QStringLiteral("ddd MMM dd yyyy hh:mm:ss ")) +
                                     offsetString(toDateTime(now, false)));
            double t = now;
            if (args.size() == 1) {
                Value v = args[0];
                if (v.isObject() && v.asObject()->cls == Object::Class::Date)
                    t = v.asObject()->primitive.asNumber();
                else {
                    v = rt.toPrimitive(v, false);
                    t = v.isString() ? parseDate(v.asString()) : rt.toNumber(v);
                }
            } else if (args.size() > 1) {
                Parts p{rt.toNumber(args[0]), rt.toNumber(args[1]), 1, 0, 0, 0, 0};
                double *fields[] = {&p.day, &p.hour, &p.minute, &p.second, &p.ms};
                for (std::size_t i = 2; i < args.size() && i < 7; ++i)
                    *fields[i - 2] = rt.toNumber(args[i]);
                if (std::isfinite(p.year) && std::trunc(p.year) >= 0 && std::trunc(p.year) <= 99)
                    p.year = 1900 + std::trunc(p.year);
                t = compose(p, false);
            }
            Object *d = make(rt, t);
            const Value prototype = rt.get(Value::object(newTarget), QStringLiteral("prototype"));
            if (prototype.isObject())
                d->proto = prototype.asObject();
            return Value::object(d);
        });
    addMethod(runtime, ctor, QStringLiteral("now"), 0, [](Runtime &, const Value &, const Values &, Object *) {
        return Value::number(static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
    });
    addMethod(runtime, ctor, QStringLiteral("parse"), 1, [](Runtime &rt, const Value &, const Values &args, Object *) {
        return Value::number(timeClip(parseDate(rt.toString(argument(args, 0)))));
    });
    addMethod(runtime, ctor, QStringLiteral("UTC"), 7, [](Runtime &rt, const Value &, const Values &args, Object *) {
        Parts p{rt.toNumber(argument(args, 0)), args.size() > 1 ? rt.toNumber(args[1]) : 0, 1, 0, 0, 0, 0};
        double *fields[] = {&p.day, &p.hour, &p.minute, &p.second, &p.ms};
        for (std::size_t i = 2; i < args.size() && i < 7; ++i)
            *fields[i - 2] = rt.toNumber(args[i]);
        if (std::isfinite(p.year) && std::trunc(p.year) >= 0 && std::trunc(p.year) <= 99)
            p.year = 1900 + std::trunc(p.year);
        return Value::number(compose(p, true));
    });

    const auto getter = [&](const char *name, bool utc, double Parts::*field) {
        addMethod(runtime, proto, QLatin1String(name), 0,
                  [utc, field](Runtime &rt, const Value &self, const Values &, Object *) {
                      const double t = thisTime(rt, self);
                      if (std::isnan(t))
                          return Value::number(kNaN);
                      return Value::number(split(t, utc).*field);
                  });
    };
    getter("getFullYear", false, &Parts::year);
    getter("getMonth", false, &Parts::month);
    getter("getDate", false, &Parts::day);
    getter("getHours", false, &Parts::hour);
    getter("getMinutes", false, &Parts::minute);
    getter("getSeconds", false, &Parts::second);
    getter("getMilliseconds", false, &Parts::ms);
    getter("getUTCFullYear", true, &Parts::year);
    getter("getUTCMonth", true, &Parts::month);
    getter("getUTCDate", true, &Parts::day);
    getter("getUTCHours", true, &Parts::hour);
    getter("getUTCMinutes", true, &Parts::minute);
    getter("getUTCSeconds", true, &Parts::second);
    getter("getUTCMilliseconds", true, &Parts::ms);
    const auto weekday = [](bool utc) {
        return [utc](Runtime &rt, const Value &self, const Values &, Object *) {
            const double t = thisTime(rt, self);
            if (std::isnan(t))
                return Value::number(kNaN);
            return Value::number(toDateTime(t, utc).date().dayOfWeek() % 7);
        };
    };
    addMethod(runtime, proto, QStringLiteral("getDay"), 0, weekday(false));
    addMethod(runtime, proto, QStringLiteral("getUTCDay"), 0, weekday(true));
    const auto time = [](Runtime &rt, const Value &self, const Values &, Object *) {
        return Value::number(thisTime(rt, self));
    };
    addMethod(runtime, proto, QStringLiteral("getTime"), 0, time);
    addMethod(runtime, proto, QStringLiteral("valueOf"), 0, time);
    addMethod(runtime, proto, QStringLiteral("getTimezoneOffset"), 0,
              [](Runtime &rt, const Value &self, const Values &, Object *) {
                  const double t = thisTime(rt, self);
                  if (std::isnan(t))
                      return Value::number(kNaN);
                  return Value::number(-toDateTime(t, false).offsetFromUtc() / 60.0);
              });
    addMethod(runtime, proto, QStringLiteral("setTime"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  thisTime(rt, self);
                  const double t = timeClip(rt.toNumber(argument(args, 0)));
                  self.asObject()->primitive = Value::number(t);
                  return Value::number(t);
              });
    // Setters take the first field they change and optionally the ones after it.
    const auto setter = [&](const char *name, bool utc, int first, int count) {
        addMethod(runtime, proto, QLatin1String(name), count,
                  [utc, first, count](Runtime &rt, const Value &self, const Values &args, Object *) {
                      double t = thisTime(rt, self);
                      if (std::isnan(t)) {
                          if (first != 0)
                              return Value::number(kNaN);
                          t = 0;
                      }
                      Parts p = split(t, utc);
                      double *fields[] = {&p.year, &p.month, &p.day, &p.hour, &p.minute, &p.second, &p.ms};
                      for (int i = 0; i < count && static_cast<std::size_t>(i) < args.size(); ++i)
                          *fields[first + i] = rt.toNumber(args[static_cast<std::size_t>(i)]);
                      if (args.empty())
                          *fields[first] = kNaN;
                      const double r = compose(p, utc);
                      self.asObject()->primitive = Value::number(r);
                      return Value::number(r);
                  });
    };
    setter("setFullYear", false, 0, 3);
    setter("setMonth", false, 1, 2);
    setter("setDate", false, 2, 1);
    setter("setHours", false, 3, 4);
    setter("setMinutes", false, 4, 3);
    setter("setSeconds", false, 5, 2);
    setter("setMilliseconds", false, 6, 1);
    setter("setUTCFullYear", true, 0, 3);
    setter("setUTCMonth", true, 1, 2);
    setter("setUTCDate", true, 2, 1);
    setter("setUTCHours", true, 3, 4);
    setter("setUTCMinutes", true, 4, 3);
    setter("setUTCSeconds", true, 5, 2);
    setter("setUTCMilliseconds", true, 6, 1);

    const auto format = [&](const char *name, const std::function<QString(double)> &fn) {
        addMethod(runtime, proto, QLatin1String(name), 0,
                  [fn](Runtime &rt, const Value &self, const Values &, Object *) {
                      const double t = thisTime(rt, self);
                      if (std::isnan(t))
                          return Value::string(QStringLiteral("Invalid Date"));
                      return Value::string(fn(t));
                  });
    };
    format("toString", [](double t) {
        const QDateTime dt = toDateTime(t, false);
        return dt.toString(QStringLiteral("ddd MMM dd yyyy hh:mm:ss ")) + offsetString(dt);
    });
    format("toDateString", [](double t) { return toDateTime(t, false).toString(QStringLiteral("ddd MMM dd yyyy")); });
    format("toTimeString", [](double t) {
        const QDateTime dt = toDateTime(t, false);
        return dt.toString(QStringLiteral("hh:mm:ss ")) + offsetString(dt);
    });
    format("toUTCString", [](double t) {
        return toDateTime(t, true).toString(QStringLiteral("ddd, dd MMM yyyy hh:mm:ss")) + QStringLiteral(" GMT");
    });
    format("toLocaleString", [](double t) { return toDateTime(t, false).toString(Qt::TextDate); });
    format("toLocaleDateString", [](double t) { return toDateTime(t, false).date().toString(Qt::TextDate); });
    format("toLocaleTimeString", [](double t) { return toDateTime(t, false).time().toString(Qt::TextDate); });
    addMethod(runtime, proto, QStringLiteral("toISOString"), 0,
              [](Runtime &rt, const Value &self, const Values &, Object *) {
                  const double t = thisTime(rt, self);
                  if (std::isnan(t))
                      rt.throwError(ErrorType::RangeError, QStringLiteral("invalid time value"));
                  return Value::string(isoString(t));
              });
    addMethod(runtime, proto, QStringLiteral("toJSON"), 1,
              [](Runtime &rt, const Value &self, const Values &, Object *) {
                  const Value p = rt.toPrimitive(self, false);
                  if (p.isNumber() && !std::isfinite(p.asNumber()))
                      return Value::null();
                  return rt.call(rt.get(self, QStringLiteral("toISOString")), self, {});
              });
}

// ---- JSON ----

class JsonParser {
public:
    JsonParser(Runtime &r, const QString &t) : runtime(r), text(t) {}

    Value parse() {
        const Value v = value(0);
        space();
        if (pos != text.size())
            fail();
        return v;
    }

private:
    [[noreturn]] void fail() {
        runtime.throwError(ErrorType::SyntaxError, QStringLiteral("JSON.parse: unexpected %1 at position %2")
                                                       .arg(pos < text.size() ? QStringLiteral("'%1'").arg(text.at(pos))
                                                                              : QStringLiteral("end of input"))
                                                       .arg(pos));
    }
    void space() {
        while (pos < text.size() &&
               (text.at(pos) == u' ' || text.at(pos) == u'\t' || text.at(pos) == u'\n' || text.at(pos) == u'\r'))
            ++pos;
    }
    bool literal(const char *word) {
        const QLatin1String w(word);
        if (text.mid(pos, w.size()) != w)
            return false;
        pos += w.size();
        return true;
    }
    QString string() {
        ++pos; // the quote
        QString out;
        while (true) {
            if (pos >= text.size())
                fail();
            const QChar c = text.at(pos++);
            if (c == u'"')
                return out;
            if (c.unicode() < 0x20)
                fail();
            if (c != u'\\') {
                out += c;
                continue;
            }
            if (pos >= text.size())
                fail();
            const QChar e = text.at(pos++);
            switch (e.unicode()) {
            case u'"':
                out += u'"';
                break;
            case u'\\':
                out += u'\\';
                break;
            case u'/':
                out += u'/';
                break;
            case u'b':
                out += u'\b';
                break;
            case u'f':
                out += u'\f';
                break;
            case u'n':
                out += u'\n';
                break;
            case u'r':
                out += u'\r';
                break;
            case u't':
                out += u'\t';
                break;
            case u'u': {
                bool ok = false;
                const uint code = text.mid(pos, 4).toUInt(&ok, 16);
                if (!ok || text.mid(pos, 4).size() != 4)
                    fail();
                pos += 4;
                out += QChar(static_cast<char16_t>(code));
                break;
            }
            default:
                fail();
            }
        }
    }
    Value value(int depth) {
        if (depth > 500)
            runtime.throwError(ErrorType::RangeError, QStringLiteral("JSON.parse: nested too deeply"));
        space();
        if (pos >= text.size())
            fail();
        const QChar c = text.at(pos);
        if (c == u'{') {
            ++pos;
            Object *o = runtime.newObject();
            space();
            if (pos < text.size() && text.at(pos) == u'}') {
                ++pos;
                return Value::object(o);
            }
            while (true) {
                space();
                if (pos >= text.size() || text.at(pos) != u'"')
                    fail();
                const QString key = string();
                space();
                if (pos >= text.size() || text.at(pos) != u':')
                    fail();
                ++pos;
                const Value v = value(depth + 1);
                if (key == QLatin1String("__proto__"))
                    o->defineOwn(key).value = v;
                else
                    o->setOwn(runtime, key, v, Value::object(o));
                space();
                if (pos < text.size() && text.at(pos) == u',') {
                    ++pos;
                    continue;
                }
                if (pos < text.size() && text.at(pos) == u'}') {
                    ++pos;
                    return Value::object(o);
                }
                fail();
            }
        }
        if (c == u'[') {
            ++pos;
            Values items;
            space();
            if (pos < text.size() && text.at(pos) == u']') {
                ++pos;
                return Value::object(runtime.newArray());
            }
            while (true) {
                items.push_back(value(depth + 1));
                space();
                if (pos < text.size() && text.at(pos) == u',') {
                    ++pos;
                    continue;
                }
                if (pos < text.size() && text.at(pos) == u']') {
                    ++pos;
                    return Value::object(runtime.newArray(items));
                }
                fail();
            }
        }
        if (c == u'"')
            return Value::string(string());
        if (literal("true"))
            return Value::boolean(true);
        if (literal("false"))
            return Value::boolean(false);
        if (literal("null"))
            return Value::null();
        // A number: -?(0|[1-9]\d*)(\.\d+)?([eE][+-]?\d+)?
        const qsizetype start = pos;
        if (text.at(pos) == u'-')
            ++pos;
        const auto digits = [this] {
            const qsizetype b = pos;
            while (pos < text.size() && text.at(pos) >= u'0' && text.at(pos) <= u'9')
                ++pos;
            return pos - b;
        };
        if (pos < text.size() && text.at(pos) == u'0')
            ++pos;
        else if (digits() == 0)
            fail();
        if (pos < text.size() && text.at(pos) == u'.') {
            ++pos;
            if (digits() == 0)
                fail();
        }
        if (pos < text.size() && (text.at(pos) == u'e' || text.at(pos) == u'E')) {
            ++pos;
            if (pos < text.size() && (text.at(pos) == u'+' || text.at(pos) == u'-'))
                ++pos;
            if (digits() == 0)
                fail();
        }
        return Value::number(Runtime::stringToNumber(text.mid(start, pos - start)));
    }

    Runtime &runtime;
    const QString &text;
    qsizetype pos = 0;
};

Value revive(Runtime &runtime, const Value &holder, const QString &key, const Value &reviver, int depth) {
    const Value v = runtime.get(holder, key);
    if (depth < 500 && v.isObject()) {
        Object *o = v.asObject();
        for (const QString &k : o->enumerableKeys(runtime)) {
            const Value r = revive(runtime, v, k, reviver, depth + 1);
            if (r.isUndefined())
                o->deleteOwn(runtime, k);
            else
                o->setOwn(runtime, k, r, v);
        }
    }
    return runtime.call(reviver, holder, {Value::string(key), v});
}

QString quote(const QString &s) {
    QString out = QStringLiteral("\"");
    for (const QChar c : s) {
        switch (c.unicode()) {
        case u'"':
            out += QLatin1String("\\\"");
            break;
        case u'\\':
            out += QLatin1String("\\\\");
            break;
        case u'\b':
            out += QLatin1String("\\b");
            break;
        case u'\f':
            out += QLatin1String("\\f");
            break;
        case u'\n':
            out += QLatin1String("\\n");
            break;
        case u'\r':
            out += QLatin1String("\\r");
            break;
        case u'\t':
            out += QLatin1String("\\t");
            break;
        default:
            if (c.unicode() < 0x20 || c.isSurrogate())
                out += QStringLiteral("\\u%1").arg(c.unicode(), 4, 16, QLatin1Char('0'));
            else
                out += c;
        }
    }
    out += u'"';
    return out;
}

class JsonWriter {
public:
    JsonWriter(Runtime &r, const Value &replacerFn, const QStringList &allow, bool useAllow, const QString &gap)
        : runtime(r), replacer(replacerFn), allowed(allow), filter(useAllow), indent(gap) {}

    // Returns false for values that are left out (undefined, functions).
    bool write(const Value &holder, const QString &key, Value v, const QString &current, QString *out) {
        if (v.isObject()) {
            const Value toJson = runtime.get(v, QStringLiteral("toJSON"));
            if (runtime.isCallable(toJson))
                v = runtime.call(toJson, v, {Value::string(key)});
        }
        if (runtime.isCallable(replacer))
            v = runtime.call(replacer, holder, {Value::string(key), v});
        if (v.isObject()) {
            Object *o = v.asObject();
            if (o->cls == Object::Class::Number)
                v = Value::number(runtime.toNumber(v));
            else if (o->cls == Object::Class::String)
                v = Value::string(runtime.toString(v));
            else if (o->cls == Object::Class::Boolean)
                v = o->primitive;
        }
        switch (v.type()) {
        case Value::Null:
            *out += QLatin1String("null");
            return true;
        case Value::Boolean:
            *out += v.asBoolean() ? QLatin1String("true") : QLatin1String("false");
            return true;
        case Value::String:
            *out += quote(v.asString());
            return true;
        case Value::Number:
            *out += std::isfinite(v.asNumber()) ? Runtime::numberToString(v.asNumber()) : QStringLiteral("null");
            return true;
        case Value::Undefined:
            return false;
        case Value::ObjectType:
            break;
        }
        if (v.asFunction())
            return false;
        Object *o = v.asObject();
        if (stack.contains(o))
            runtime.throwTypeError(QStringLiteral("JSON.stringify: cyclic structure"));
        if (stack.size() > 500)
            runtime.throwError(ErrorType::RangeError, QStringLiteral("JSON.stringify: nested too deeply"));
        stack.append(o);
        const QString inner = current + indent;
        const QString open = indent.isEmpty() ? QString() : u'\n' + inner;
        const QString close = indent.isEmpty() ? QString() : u'\n' + current;
        if (o->cls == Object::Class::Array) {
            *out += u'[';
            for (std::size_t i = 0; i < o->elements.size(); ++i) {
                if (i)
                    *out += u',';
                *out += open;
                if (!write(v, QString::number(i), o->elements[i], inner, out))
                    *out += QLatin1String("null");
            }
            if (!o->elements.empty())
                *out += close;
            *out += u']';
        } else {
            *out += u'{';
            bool any = false;
            const QStringList keys = filter ? allowed : o->enumerableKeys(runtime);
            for (const QString &k : keys) {
                QString item;
                if (!write(v, k, runtime.getFromObject(o, k, v), inner, &item))
                    continue;
                if (any)
                    *out += u',';
                *out += open + quote(k) + (indent.isEmpty() ? QStringLiteral(":") : QStringLiteral(": ")) + item;
                any = true;
            }
            if (any)
                *out += close;
            *out += u'}';
        }
        stack.removeLast();
        return true;
    }

private:
    Runtime &runtime;
    Value replacer;
    QStringList allowed;
    bool filter;
    QString indent;
    QList<Object *> stack;
};

} // namespace

QString jsonStringify(Runtime &runtime, const Value &value, const QString &indent) {
    JsonWriter w(runtime, Value(), {}, false, indent);
    Object *holder = runtime.newObject();
    holder->setOwn(QString(), value);
    QString out;
    if (!w.write(Value::object(holder), QString(), value, QString(), &out))
        return QString();
    return out;
}

void installJson(Runtime &runtime) {
    Object *json = runtime.newObject();
    runtime.global->setOwn(QStringLiteral("JSON"), Value::object(json), false);
    addMethod(runtime, json, QStringLiteral("parse"), 2, [](Runtime &rt, const Value &, const Values &args, Object *) {
        const QString text = rt.toString(argument(args, 0));
        JsonParser parser(rt, text);
        Value v = parser.parse();
        const Value reviver = argument(args, 1);
        if (!rt.isCallable(reviver))
            return v;
        Object *holder = rt.newObject();
        holder->setOwn(QString(), v);
        return revive(rt, Value::object(holder), QString(), reviver, 0);
    });
    addMethod(runtime, json, QStringLiteral("stringify"), 3,
              [](Runtime &rt, const Value &, const Values &args, Object *) {
                  const Value replacer = argument(args, 1);
                  QStringList allowed;
                  bool filter = false;
                  if (replacer.isObject() && replacer.asObject()->cls == Object::Class::Array) {
                      filter = true;
                      for (const Value &v : replacer.asObject()->elements)
                          if (v.isString() || v.isNumber())
                              allowed.append(rt.toString(v));
                  }
                  Value space = argument(args, 2);
                  if (space.isObject())
                      space = rt.toPrimitive(space, space.asObject()->cls == Object::Class::String);
                  QString gap;
                  if (space.isNumber())
                      gap = QString(static_cast<qsizetype>(std::clamp(std::trunc(space.asNumber()), 0.0, 10.0)), u' ');
                  else if (space.isString())
                      gap = space.asString().left(10);
                  JsonWriter w(rt, rt.isCallable(replacer) ? replacer : Value(), allowed, filter, gap);
                  Object *holder = rt.newObject();
                  holder->setOwn(QString(), argument(args, 0));
                  QString out;
                  if (!w.write(Value::object(holder), QString(), argument(args, 0), QString(), &out))
                      return Value();
                  return Value::string(out);
              });
}

void installDate(Runtime &runtime) { installDateImpl(runtime); }

} // namespace QtJSPrivate
