// MIT License - see LICENSE
//
// String, RegExp and Number of the JavaScript runtime. Regular expressions are run by QRegularExpression,
// whose syntax is close to that of JavaScript for the common cases.
#include "qjsbuiltins_p.h"

#include "qjsinterp_p.h"

#include <QtCore/qregularexpression.h>

#include <cmath>
#include <cstdio>
#include <limits>

namespace QtJSPrivate {

namespace {

class RegExpObject : public Object {
public:
    explicit RegExpObject(Object *prototype) : Object(prototype, Class::RegExp) {}
    QRegularExpression re;
    QString source;
    QString flags;
    bool global() const { return flags.contains(u'g'); }
    bool sticky() const { return flags.contains(u'y'); }
};

RegExpObject *regExpThis(Runtime &runtime, const Value &v, const char *method) {
    if (v.isObject() && v.asObject()->cls == Object::Class::RegExp)
        return static_cast<RegExpObject *>(v.asObject());
    runtime.throwTypeError(
        QStringLiteral("RegExp.prototype.%1 called on something that is not a RegExp").arg(QLatin1String(method)));
}

QString thisString(Runtime &runtime, const Value &self, const char *method) {
    if (self.isNullish())
        runtime.throwTypeError(
            QStringLiteral("String.prototype.%1 called on %2").arg(QLatin1String(method), runtime.toString(self)));
    return runtime.toString(self);
}

qsizetype clampIndex(Runtime &runtime, const Value &v, qsizetype length, qsizetype fallback) {
    if (v.isUndefined())
        return fallback;
    const double d = std::trunc(runtime.toNumber(v));
    if (std::isnan(d))
        return 0;
    return static_cast<qsizetype>(std::clamp(d, 0.0, static_cast<double>(length)));
}

qsizetype relative(Runtime &runtime, const Value &v, qsizetype length, qsizetype fallback) {
    if (v.isUndefined())
        return fallback;
    const double d = std::trunc(runtime.toNumber(v));
    if (std::isnan(d))
        return 0;
    const double r = d < 0 ? std::max(0.0, static_cast<double>(length) + d) : std::min(d, static_cast<double>(length));
    return static_cast<qsizetype>(r);
}

// One match of a regular expression, as exec() returns it.
Value matchResult(Runtime &runtime, RegExpObject *r, const QRegularExpressionMatch &m, const QString &input) {
    Values items;
    for (int i = 0; i <= r->re.captureCount(); ++i)
        items.push_back(m.hasCaptured(i) ? Value::string(m.captured(i)) : Value());
    Object *a = runtime.newArray(items);
    a->setOwn(QStringLiteral("index"), Value::number(static_cast<double>(m.capturedStart(0))));
    a->setOwn(QStringLiteral("input"), Value::string(input));
    const QStringList names = r->re.namedCaptureGroups();
    Value groups;
    for (int i = 1; i < names.size(); ++i) {
        if (names.at(i).isEmpty())
            continue;
        if (groups.isUndefined())
            groups = Value::object(runtime.heap.make<Object>(nullptr));
        groups.asObject()->setOwn(names.at(i), m.hasCaptured(i) ? Value::string(m.captured(i)) : Value());
    }
    a->setOwn(QStringLiteral("groups"), groups);
    return Value::object(a);
}

// exec(): the next match from lastIndex for global and sticky expressions, or the first one.
QRegularExpressionMatch execute(Runtime &runtime, RegExpObject *r, const QString &input, bool *found) {
    const bool useLast = r->global() || r->sticky();
    qsizetype start = 0;
    if (useLast) {
        const double li = std::trunc(runtime.toNumber(runtime.get(Value::object(r), QStringLiteral("lastIndex"))));
        start = std::isnan(li) || li < 0 ? 0 : static_cast<qsizetype>(std::min(li, 1e15));
    }
    *found = false;
    QRegularExpressionMatch m;
    if (start <= input.size()) {
        m = r->re.match(input, start, QRegularExpression::NormalMatch,
                        r->sticky() ? QRegularExpression::AnchorAtOffsetMatchOption
                                    : QRegularExpression::NoMatchOption);
        *found = m.hasMatch();
    }
    if (useLast) {
        qsizetype next = 0;
        if (*found) {
            next = m.capturedEnd(0);
            if (next == m.capturedStart(0) && m.capturedLength(0) == 0)
                next = m.capturedEnd(0);
        }
        runtime.put(Value::object(r), QStringLiteral("lastIndex"), Value::number(static_cast<double>(next)));
    }
    return m;
}

// The replacement text for one match: $$, $&, $`, $', $n and $<name>.
QString expand(const QString &replacement, const QRegularExpressionMatch *m, const QString &matched,
               const QString &input, qsizetype position, int captures) {
    QString out;
    for (qsizetype i = 0; i < replacement.size(); ++i) {
        const QChar c = replacement.at(i);
        if (c != u'$' || i + 1 >= replacement.size()) {
            out += c;
            continue;
        }
        const QChar n = replacement.at(i + 1);
        if (n == u'$') {
            out += u'$';
            ++i;
        } else if (n == u'&') {
            out += matched;
            ++i;
        } else if (n == u'`') {
            out += input.left(position);
            ++i;
        } else if (n == u'\'') {
            out += input.mid(position + matched.size());
            ++i;
        } else if (n.isDigit() && m) {
            int index = n.digitValue();
            qsizetype used = 1;
            if (i + 2 < replacement.size() && replacement.at(i + 2).isDigit()) {
                const int two = index * 10 + replacement.at(i + 2).digitValue();
                if (two >= 1 && two <= captures) {
                    index = two;
                    used = 2;
                }
            }
            if (index >= 1 && index <= captures) {
                out += m->captured(index);
                i += used;
            } else {
                out += c;
            }
        } else if (n == u'<' && m) {
            const qsizetype end = replacement.indexOf(u'>', i + 2);
            if (end < 0) {
                out += c;
                continue;
            }
            out += m->captured(replacement.mid(i + 2, end - i - 2));
            i = end;
        } else {
            out += c;
        }
    }
    return out;
}

Value replace(Runtime &runtime, const Value &self, const Values &args, bool all) {
    const QString s = thisString(runtime, self, all ? "replaceAll" : "replace");
    const Value pattern = argument(args, 0), replacement = argument(args, 1);
    const bool functional = runtime.isCallable(replacement);
    const QString text = functional ? QString() : runtime.toString(replacement);
    if (pattern.isObject() && pattern.asObject()->cls == Object::Class::RegExp) {
        auto *r = static_cast<RegExpObject *>(pattern.asObject());
        if (all && !r->global())
            runtime.throwTypeError(QStringLiteral("replaceAll needs a global regular expression"));
        QString out;
        qsizetype last = 0;
        qsizetype from = 0;
        const bool global = r->global();
        if (global)
            runtime.put(pattern, QStringLiteral("lastIndex"), Value::number(0));
        int guard = 0;
        while (from <= s.size() && ++guard < 10000000) {
            const QRegularExpressionMatch m = global ? r->re.match(s, from) : [&] {
                bool found = false;
                return execute(runtime, r, s, &found);
            }();
            if (!m.hasMatch())
                break;
            const qsizetype start = m.capturedStart(0);
            out += s.mid(last, start - last);
            if (functional) {
                Values call;
                for (int i = 0; i <= r->re.captureCount(); ++i)
                    call.push_back(m.hasCaptured(i) ? Value::string(m.captured(i)) : Value());
                call.push_back(Value::number(static_cast<double>(start)));
                call.push_back(Value::string(s));
                out += runtime.toString(runtime.call(replacement, Value(), call));
            } else {
                out += expand(text, &m, m.captured(0), s, start, r->re.captureCount());
            }
            last = m.capturedEnd(0);
            if (!global)
                break;
            from = m.capturedLength(0) == 0 ? last + 1 : last;
        }
        if (global)
            runtime.put(pattern, QStringLiteral("lastIndex"), Value::number(0));
        out += s.mid(last);
        return Value::string(out);
    }
    const QString needle = runtime.toString(pattern);
    QString out;
    qsizetype last = 0;
    qsizetype at = s.indexOf(needle);
    while (at >= 0) {
        out += s.mid(last, at - last);
        if (functional)
            out += runtime.toString(
                runtime.call(replacement, Value(),
                             {Value::string(needle), Value::number(static_cast<double>(at)), Value::string(s)}));
        else
            out += expand(text, nullptr, needle, s, at, 0);
        last = at + needle.size();
        if (!all)
            break;
        at = s.indexOf(needle, needle.isEmpty() ? last + 1 : last);
        if (needle.isEmpty() && last >= s.size())
            break;
    }
    out += s.mid(last);
    return Value::string(out);
}

void installString(Runtime &runtime) {
    Object *proto = runtime.heap.make<Object>(runtime.objectProto, Object::Class::String);
    proto->primitive = Value::string(QString());
    runtime.stringProto = proto;
    Function *ctor = addConstructor(runtime, QStringLiteral("String"), 1, proto,
                                    [](Runtime &rt, const Value &, const Values &args, Object *newTarget) {
                                        const Value s = Value::string(args.empty() ? QString() : rt.toString(args[0]));
                                        return newTarget ? Value::object(rt.toObject(s)) : s;
                                    });
    addMethod(runtime, ctor, QStringLiteral("fromCharCode"), 1,
              [](Runtime &rt, const Value &, const Values &args, Object *) {
                  QString s;
                  for (const Value &v : args) {
                      const double d = rt.toNumber(v);
                      s += QChar(static_cast<char16_t>(
                          static_cast<quint32>(
                              std::isfinite(d) ? std::fmod(std::trunc(d), 65536.0) + (d < 0 ? 65536.0 : 0) : 0) &
                          0xffff));
                  }
                  return Value::string(s);
              });
    addMethod(runtime, ctor, QStringLiteral("fromCodePoint"), 1,
              [](Runtime &rt, const Value &, const Values &args, Object *) {
                  QString s;
                  for (const Value &v : args) {
                      const double d = rt.toNumber(v);
                      if (d < 0 || d > 0x10ffff || d != std::floor(d))
                          rt.throwError(ErrorType::RangeError, QStringLiteral("invalid code point"));
                      const auto c = static_cast<char32_t>(d);
                      s += QString::fromUcs4(&c, 1);
                  }
                  return Value::string(s);
              });

    const auto unwrap = [](Runtime &rt, const Value &self) {
        if (self.isString())
            return self.asString();
        if (self.isObject() && self.asObject()->cls == Object::Class::String)
            return self.asObject()->primitive.asString();
        rt.throwTypeError(QStringLiteral("not a string"));
    };
    addMethod(
        runtime, proto, QStringLiteral("toString"), 0,
        [unwrap](Runtime &rt, const Value &self, const Values &, Object *) { return Value::string(unwrap(rt, self)); });
    addMethod(
        runtime, proto, QStringLiteral("valueOf"), 0,
        [unwrap](Runtime &rt, const Value &self, const Values &, Object *) { return Value::string(unwrap(rt, self)); });
    addMethod(
        runtime, proto, QStringLiteral("charAt"), 1, [](Runtime &rt, const Value &self, const Values &args, Object *) {
            const QString s = thisString(rt, self, "charAt");
            const double i = std::trunc(rt.toNumber(argument(args, 0)));
            return Value::string(i >= 0 && i < static_cast<double>(s.size()) ? QString(s.at(static_cast<qsizetype>(i)))
                                                                             : QString());
        });
    addMethod(runtime, proto, QStringLiteral("charCodeAt"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const QString s = thisString(rt, self, "charCodeAt");
                  const double d = rt.toNumber(argument(args, 0));
                  const double i = std::isnan(d) ? 0 : std::trunc(d);
                  if (i < 0 || i >= static_cast<double>(s.size()))
                      return Value::number(std::numeric_limits<double>::quiet_NaN());
                  return Value::number(s.at(static_cast<qsizetype>(i)).unicode());
              });
    addMethod(runtime, proto, QStringLiteral("codePointAt"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const QString s = thisString(rt, self, "codePointAt");
                  const double d = rt.toNumber(argument(args, 0));
                  const double i = std::isnan(d) ? 0 : std::trunc(d);
                  if (i < 0 || i >= static_cast<double>(s.size()))
                      return Value();
                  const auto k = static_cast<qsizetype>(i);
                  const QChar c = s.at(k);
                  if (c.isHighSurrogate() && k + 1 < s.size() && s.at(k + 1).isLowSurrogate())
                      return Value::number(QChar::surrogateToUcs4(c, s.at(k + 1)));
                  return Value::number(c.unicode());
              });
    addMethod(runtime, proto, QStringLiteral("at"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const QString s = thisString(rt, self, "at");
                  const double d = std::trunc(rt.toNumber(argument(args, 0)));
                  const double i = std::isnan(d) ? 0 : d < 0 ? static_cast<double>(s.size()) + d : d;
                  if (i < 0 || i >= static_cast<double>(s.size()))
                      return Value();
                  return Value::string(QString(s.at(static_cast<qsizetype>(i))));
              });
    addMethod(runtime, proto, QStringLiteral("indexOf"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const QString s = thisString(rt, self, "indexOf");
                  const qsizetype from = clampIndex(rt, argument(args, 1), s.size(), 0);
                  return Value::number(static_cast<double>(s.indexOf(rt.toString(argument(args, 0)), from)));
              });
    addMethod(runtime, proto, QStringLiteral("lastIndexOf"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const QString s = thisString(rt, self, "lastIndexOf");
                  const double d = rt.toNumber(argument(args, 1));
                  const qsizetype from =
                      std::isnan(d)
                          ? s.size()
                          : static_cast<qsizetype>(std::clamp(std::trunc(d), 0.0, static_cast<double>(s.size())));
                  return Value::number(static_cast<double>(s.lastIndexOf(rt.toString(argument(args, 0)), from)));
              });
    addMethod(runtime, proto, QStringLiteral("includes"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const QString s = thisString(rt, self, "includes");
                  if (argument(args, 0).isObject() && argument(args, 0).asObject()->cls == Object::Class::RegExp)
                      rt.throwTypeError(QStringLiteral("includes() does not take a regular expression"));
                  return Value::boolean(
                      s.indexOf(rt.toString(argument(args, 0)), clampIndex(rt, argument(args, 1), s.size(), 0)) >= 0);
              });
    addMethod(runtime, proto, QStringLiteral("startsWith"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const QString s = thisString(rt, self, "startsWith");
                  const qsizetype from = clampIndex(rt, argument(args, 1), s.size(), 0);
                  return Value::boolean(s.mid(from).startsWith(rt.toString(argument(args, 0))));
              });
    addMethod(runtime, proto, QStringLiteral("endsWith"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const QString s = thisString(rt, self, "endsWith");
                  const qsizetype end = clampIndex(rt, argument(args, 1), s.size(), s.size());
                  return Value::boolean(s.left(end).endsWith(rt.toString(argument(args, 0))));
              });
    addMethod(runtime, proto, QStringLiteral("slice"), 2,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const QString s = thisString(rt, self, "slice");
                  const qsizetype b = relative(rt, argument(args, 0), s.size(), 0);
                  const qsizetype e = relative(rt, argument(args, 1), s.size(), s.size());
                  return Value::string(e > b ? s.mid(b, e - b) : QString());
              });
    addMethod(runtime, proto, QStringLiteral("substring"), 2,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const QString s = thisString(rt, self, "substring");
                  qsizetype b = clampIndex(rt, argument(args, 0), s.size(), 0);
                  qsizetype e = clampIndex(rt, argument(args, 1), s.size(), s.size());
                  if (b > e)
                      std::swap(b, e);
                  return Value::string(s.mid(b, e - b));
              });
    addMethod(runtime, proto, QStringLiteral("substr"), 2,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const QString s = thisString(rt, self, "substr");
                  const qsizetype b = relative(rt, argument(args, 0), s.size(), 0);
                  const Value lv = argument(args, 1);
                  const double l = lv.isUndefined() ? static_cast<double>(s.size()) : std::trunc(rt.toNumber(lv));
                  if (std::isnan(l) || l <= 0)
                      return Value::string(QString());
                  return Value::string(s.mid(b, static_cast<qsizetype>(std::min(l, static_cast<double>(s.size())))));
              });
    const auto caseMethod = [](bool upper) {
        return [upper](Runtime &rt, const Value &self, const Values &, Object *) {
            const QString s = thisString(rt, self, upper ? "toUpperCase" : "toLowerCase");
            return Value::string(upper ? s.toUpper() : s.toLower());
        };
    };
    addMethod(runtime, proto, QStringLiteral("toUpperCase"), 0, caseMethod(true));
    addMethod(runtime, proto, QStringLiteral("toLowerCase"), 0, caseMethod(false));
    addMethod(runtime, proto, QStringLiteral("toLocaleUpperCase"), 0, caseMethod(true));
    addMethod(runtime, proto, QStringLiteral("toLocaleLowerCase"), 0, caseMethod(false));
    const auto trim = [](int which) {
        return [which](Runtime &rt, const Value &self, const Values &, Object *) {
            const QString s = thisString(rt, self, "trim");
            qsizetype b = 0, e = s.size();
            const auto space = [](QChar c) { return c.isSpace() || c == QChar(0xfeff); };
            if (which != 2)
                while (b < e && space(s.at(b)))
                    ++b;
            if (which != 1)
                while (e > b && space(s.at(e - 1)))
                    --e;
            return Value::string(s.mid(b, e - b));
        };
    };
    addMethod(runtime, proto, QStringLiteral("trim"), 0, trim(0));
    addMethod(runtime, proto, QStringLiteral("trimStart"), 0, trim(1));
    addMethod(runtime, proto, QStringLiteral("trimEnd"), 0, trim(2));
    const auto pad = [](bool start) {
        return [start](Runtime &rt, const Value &self, const Values &args, Object *) {
            const QString s = thisString(rt, self, start ? "padStart" : "padEnd");
            const double target = rt.toNumber(argument(args, 0));
            const Value f = argument(args, 1);
            const QString filler = f.isUndefined() ? QStringLiteral(" ") : rt.toString(f);
            if (!(target > static_cast<double>(s.size())) || filler.isEmpty())
                return Value::string(s);
            if (target > 1e8)
                rt.throwError(ErrorType::RangeError, QStringLiteral("string too long"));
            const auto need = static_cast<qsizetype>(target) - s.size();
            QString padding;
            while (padding.size() < need)
                padding += filler;
            padding.truncate(need);
            return Value::string(start ? padding + s : s + padding);
        };
    };
    addMethod(runtime, proto, QStringLiteral("padStart"), 2, pad(true));
    addMethod(runtime, proto, QStringLiteral("padEnd"), 2, pad(false));
    addMethod(runtime, proto, QStringLiteral("repeat"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const QString s = thisString(rt, self, "repeat");
                  const double n = std::trunc(rt.toNumber(argument(args, 0)));
                  if (n < 0 || std::isinf(n))
                      rt.throwError(ErrorType::RangeError, QStringLiteral("invalid count"));
                  if (std::isnan(n) || n == 0 || s.isEmpty())
                      return Value::string(QString());
                  if (n * static_cast<double>(s.size()) > 1e8)
                      rt.throwError(ErrorType::RangeError, QStringLiteral("string too long"));
                  return Value::string(s.repeated(static_cast<qsizetype>(n)));
              });
    addMethod(runtime, proto, QStringLiteral("concat"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  QString s = thisString(rt, self, "concat");
                  for (const Value &v : args)
                      s += rt.toString(v);
                  return Value::string(s);
              });
    addMethod(runtime, proto, QStringLiteral("localeCompare"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const QString s = thisString(rt, self, "localeCompare");
                  const int c = QString::localeAwareCompare(s, rt.toString(argument(args, 0)));
                  return Value::number(c < 0 ? -1 : c > 0 ? 1 : 0);
              });
    addMethod(runtime, proto, QStringLiteral("normalize"), 0,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const QString s = thisString(rt, self, "normalize");
                  const Value f = argument(args, 0);
                  const QString form = f.isUndefined() ? QStringLiteral("NFC") : rt.toString(f);
                  QString::NormalizationForm mode = QString::NormalizationForm_C;
                  if (form == QLatin1String("NFD"))
                      mode = QString::NormalizationForm_D;
                  else if (form == QLatin1String("NFKC"))
                      mode = QString::NormalizationForm_KC;
                  else if (form == QLatin1String("NFKD"))
                      mode = QString::NormalizationForm_KD;
                  else if (form != QLatin1String("NFC"))
                      rt.throwError(ErrorType::RangeError, QStringLiteral("invalid normalization form"));
                  return Value::string(s.normalized(mode));
              });
    addMethod(runtime, proto, QStringLiteral("split"), 2,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const QString s = thisString(rt, self, "split");
                  const Value sep = argument(args, 0), lim = argument(args, 1);
                  const double limit = lim.isUndefined() ? 4294967295.0 : std::trunc(rt.toNumber(lim));
                  Values parts;
                  const auto add = [&](const QString &part) {
                      if (static_cast<double>(parts.size()) < limit)
                          parts.push_back(Value::string(part));
                  };
                  if (limit <= 0)
                      return Value::object(rt.newArray());
                  if (sep.isUndefined()) {
                      add(s);
                  } else if (sep.isObject() && sep.asObject()->cls == Object::Class::RegExp) {
                      auto *r = static_cast<RegExpObject *>(sep.asObject());
                      if (s.isEmpty()) {
                          if (!r->re.match(s).hasMatch())
                              add(s);
                          return Value::object(rt.newArray(parts));
                      }
                      qsizetype last = 0, from = 0;
                      while (from < s.size()) {
                          const QRegularExpressionMatch m = r->re.match(s, from);
                          if (!m.hasMatch())
                              break;
                          const qsizetype start = m.capturedStart(0), end = m.capturedEnd(0);
                          if (end == start && start >= s.size())
                              break;
                          if (end == last && end == start) {
                              from = start + 1;
                              continue;
                          }
                          if (start == 0 && end == 0) {
                              from = 1;
                              continue;
                          }
                          add(s.mid(last, start - last));
                          for (int i = 1; i <= r->re.captureCount(); ++i)
                              if (m.hasCaptured(i))
                                  add(m.captured(i));
                              else
                                  parts.push_back(Value());
                          last = end;
                          from = end == start ? end + 1 : end;
                      }
                      add(s.mid(last));
                  } else {
                      const QString separator = rt.toString(sep);
                      if (separator.isEmpty()) {
                          for (const QChar c : s)
                              add(QString(c));
                      } else {
                          for (const QString &part : s.split(separator))
                              add(part);
                      }
                  }
                  return Value::object(rt.newArray(parts));
              });
    addMethod(
        runtime, proto, QStringLiteral("replace"), 2,
        [](Runtime &rt, const Value &self, const Values &args, Object *) { return replace(rt, self, args, false); });
    addMethod(
        runtime, proto, QStringLiteral("replaceAll"), 2,
        [](Runtime &rt, const Value &self, const Values &args, Object *) { return replace(rt, self, args, true); });
    const auto toRegExp = [](Runtime &rt, const Value &v) {
        if (v.isObject() && v.asObject()->cls == Object::Class::RegExp)
            return static_cast<RegExpObject *>(v.asObject());
        return static_cast<RegExpObject *>(
            newRegExp(rt, v.isUndefined() ? QString() : QRegularExpression::escape(rt.toString(v)), QString()));
    };
    addMethod(runtime, proto, QStringLiteral("match"), 1,
              [toRegExp](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const QString s = thisString(rt, self, "match");
                  RegExpObject *r = toRegExp(rt, argument(args, 0));
                  if (!r->global()) {
                      bool found = false;
                      const QRegularExpressionMatch m = execute(rt, r, s, &found);
                      return found ? matchResult(rt, r, m, s) : Value::null();
                  }
                  Values all;
                  qsizetype from = 0;
                  while (from <= s.size()) {
                      const QRegularExpressionMatch m = r->re.match(s, from);
                      if (!m.hasMatch())
                          break;
                      all.push_back(Value::string(m.captured(0)));
                      from = m.capturedLength(0) == 0 ? m.capturedEnd(0) + 1 : m.capturedEnd(0);
                  }
                  rt.put(Value::object(r), QStringLiteral("lastIndex"), Value::number(0));
                  return all.empty() ? Value::null() : Value::object(rt.newArray(all));
              });
    addMethod(runtime, proto, QStringLiteral("search"), 1,
              [toRegExp](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const QString s = thisString(rt, self, "search");
                  RegExpObject *r = toRegExp(rt, argument(args, 0));
                  const QRegularExpressionMatch m = r->re.match(s);
                  return Value::number(m.hasMatch() ? static_cast<double>(m.capturedStart(0)) : -1.0);
              });
    // QML: "%1 of %2".arg(a).arg(b).
    addMethod(runtime, proto, QStringLiteral("arg"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const QString s = thisString(rt, self, "arg");
                  const Value v = argument(args, 0);
                  if (v.isNumber())
                      return Value::string(s.arg(Runtime::numberToString(v.asNumber())));
                  return Value::string(s.arg(rt.toString(v)));
              });
}

void installRegExp(Runtime &runtime) {
    Object *proto = runtime.newObject();
    runtime.regexpProto = proto;
    addConstructor(runtime, QStringLiteral("RegExp"), 2, proto,
                   [](Runtime &rt, const Value &, const Values &args, Object *) {
                       const Value p = argument(args, 0), f = argument(args, 1);
                       if (p.isObject() && p.asObject()->cls == Object::Class::RegExp) {
                           auto *r = static_cast<RegExpObject *>(p.asObject());
                           return Value::object(newRegExp(rt, r->source, f.isUndefined() ? r->flags : rt.toString(f)));
                       }
                       return Value::object(newRegExp(rt, p.isUndefined() ? QString() : rt.toString(p),
                                                      f.isUndefined() ? QString() : rt.toString(f)));
                   });
    addMethod(runtime, proto, QStringLiteral("exec"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  RegExpObject *r = regExpThis(rt, self, "exec");
                  const QString s = rt.toString(argument(args, 0));
                  bool found = false;
                  const QRegularExpressionMatch m = execute(rt, r, s, &found);
                  return found ? matchResult(rt, r, m, s) : Value::null();
              });
    addMethod(runtime, proto, QStringLiteral("test"), 1,
              [](Runtime &rt, const Value &self, const Values &args, Object *) {
                  RegExpObject *r = regExpThis(rt, self, "test");
                  bool found = false;
                  execute(rt, r, rt.toString(argument(args, 0)), &found);
                  return Value::boolean(found);
              });
    addMethod(runtime, proto, QStringLiteral("toString"), 0,
              [](Runtime &rt, const Value &self, const Values &, Object *) {
                  RegExpObject *r = regExpThis(rt, self, "toString");
                  return Value::string(u'/' + r->source + u'/' + r->flags);
              });
    const auto accessor = [&](const char *name, const std::function<Value(RegExpObject *)> &fn) {
        Function *g = runtime.newNative(QLatin1String(name), 0,
                                        [fn, name](Runtime &rt, const Value &self, const Values &, Object *) {
                                            return fn(regExpThis(rt, self, name));
                                        });
        Property &p = proto->defineOwn(QLatin1String(name));
        p.accessor = true;
        p.getter = g;
        p.enumerable = false;
    };
    accessor("source", [](RegExpObject *r) { return Value::string(r->source); });
    accessor("flags", [](RegExpObject *r) { return Value::string(r->flags); });
    accessor("global", [](RegExpObject *r) { return Value::boolean(r->global()); });
    accessor("ignoreCase", [](RegExpObject *r) { return Value::boolean(r->flags.contains(u'i')); });
    accessor("multiline", [](RegExpObject *r) { return Value::boolean(r->flags.contains(u'm')); });
    accessor("sticky", [](RegExpObject *r) { return Value::boolean(r->sticky()); });
    accessor("unicode", [](RegExpObject *r) { return Value::boolean(r->flags.contains(u'u')); });
    accessor("dotAll", [](RegExpObject *r) { return Value::boolean(r->flags.contains(u's')); });
}

QString radixString(double value, int radix) {
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    const bool negative = value < 0;
    double v = std::fabs(value);
    double integer = std::floor(v);
    double fraction = v - integer;
    QString out;
    if (integer == 0)
        out = QStringLiteral("0");
    while (integer > 0 && out.size() < 1100) {
        const double d = std::fmod(integer, radix);
        out.prepend(QLatin1Char(digits[static_cast<int>(d)]));
        integer = std::floor(integer / radix);
    }
    if (fraction > 0) {
        out += u'.';
        for (int i = 0; i < 52 && fraction > 0; ++i) {
            fraction *= radix;
            const int d = static_cast<int>(std::floor(fraction));
            out += QLatin1Char(digits[d]);
            fraction -= d;
        }
    }
    return negative ? u'-' + out : out;
}

QString exponential(double x, int fractionDigits) {
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*e", fractionDigits, x);
    QString s = QString::fromLatin1(buffer);
    // "1.5e+03" becomes "1.5e+3".
    const qsizetype e = s.indexOf(u'e');
    QString mantissa = s.left(e);
    const int exp = s.mid(e + 1).toInt();
    return mantissa + QStringLiteral("e") + (exp >= 0 ? QStringLiteral("+") : QStringLiteral("-")) +
           QString::number(std::abs(exp));
}

void installNumberImpl(Runtime &runtime) {
    Object *proto = runtime.heap.make<Object>(runtime.objectProto, Object::Class::Number);
    proto->primitive = Value::number(0);
    runtime.numberProto = proto;
    Function *ctor = addConstructor(runtime, QStringLiteral("Number"), 1, proto,
                                    [](Runtime &rt, const Value &, const Values &args, Object *newTarget) {
                                        const Value n = Value::number(args.empty() ? 0.0 : rt.toNumber(args[0]));
                                        return newTarget ? Value::object(rt.toObject(n)) : n;
                                    });
    addConstant(ctor, QStringLiteral("MAX_SAFE_INTEGER"), Value::number(9007199254740991.0));
    addConstant(ctor, QStringLiteral("MIN_SAFE_INTEGER"), Value::number(-9007199254740991.0));
    addConstant(ctor, QStringLiteral("EPSILON"), Value::number(std::numeric_limits<double>::epsilon()));
    addConstant(ctor, QStringLiteral("MAX_VALUE"), Value::number(std::numeric_limits<double>::max()));
    addConstant(ctor, QStringLiteral("MIN_VALUE"), Value::number(std::numeric_limits<double>::denorm_min()));
    addConstant(ctor, QStringLiteral("POSITIVE_INFINITY"), Value::number(std::numeric_limits<double>::infinity()));
    addConstant(ctor, QStringLiteral("NEGATIVE_INFINITY"), Value::number(-std::numeric_limits<double>::infinity()));
    addConstant(ctor, QStringLiteral("NaN"), Value::number(std::numeric_limits<double>::quiet_NaN()));
    addMethod(runtime, ctor, QStringLiteral("isInteger"), 1,
              [](Runtime &, const Value &, const Values &args, Object *) {
                  const Value v = argument(args, 0);
                  return Value::boolean(v.isNumber() && std::isfinite(v.asNumber()) &&
                                        std::trunc(v.asNumber()) == v.asNumber());
              });
    addMethod(runtime, ctor, QStringLiteral("isSafeInteger"), 1,
              [](Runtime &, const Value &, const Values &args, Object *) {
                  const Value v = argument(args, 0);
                  return Value::boolean(v.isNumber() && std::isfinite(v.asNumber()) &&
                                        std::trunc(v.asNumber()) == v.asNumber() &&
                                        std::fabs(v.asNumber()) <= 9007199254740991.0);
              });
    addMethod(runtime, ctor, QStringLiteral("isFinite"), 1, [](Runtime &, const Value &, const Values &args, Object *) {
        const Value v = argument(args, 0);
        return Value::boolean(v.isNumber() && std::isfinite(v.asNumber()));
    });
    addMethod(runtime, ctor, QStringLiteral("isNaN"), 1, [](Runtime &, const Value &, const Values &args, Object *) {
        const Value v = argument(args, 0);
        return Value::boolean(v.isNumber() && std::isnan(v.asNumber()));
    });
    Function *parseFloatFn =
        addMethod(runtime, runtime.global, QStringLiteral("parseFloat"), 1,
                  [](Runtime &rt, const Value &, const Values &args, Object *) {
                      const QString s = rt.toString(argument(args, 0)).trimmed();
                      // The longest prefix that is a number.
                      qsizetype i = 0;
                      if (i < s.size() && (s.at(i) == u'+' || s.at(i) == u'-'))
                          ++i;
                      if (s.mid(i).startsWith(QLatin1String("Infinity")))
                          return Value::number(s.at(0) == u'-' ? -std::numeric_limits<double>::infinity()
                                                               : std::numeric_limits<double>::infinity());
                      qsizetype digits = 0;
                      while (i < s.size() && s.at(i).unicode() < 128 && s.at(i).isDigit()) {
                          ++i;
                          ++digits;
                      }
                      if (i < s.size() && s.at(i) == u'.') {
                          ++i;
                          while (i < s.size() && s.at(i).unicode() < 128 && s.at(i).isDigit()) {
                              ++i;
                              ++digits;
                          }
                      }
                      if (digits == 0)
                          return Value::number(std::numeric_limits<double>::quiet_NaN());
                      if (i < s.size() && (s.at(i) == u'e' || s.at(i) == u'E')) {
                          qsizetype j = i + 1;
                          if (j < s.size() && (s.at(j) == u'+' || s.at(j) == u'-'))
                              ++j;
                          if (j < s.size() && s.at(j).isDigit()) {
                              while (j < s.size() && s.at(j).unicode() < 128 && s.at(j).isDigit())
                                  ++j;
                              i = j;
                          }
                      }
                      return Value::number(Runtime::stringToNumber(s.left(i)));
                  });
    Function *parseIntFn = addMethod(runtime, runtime.global, QStringLiteral("parseInt"), 2,
                                     [](Runtime &rt, const Value &, const Values &args, Object *) {
                                         QString s = rt.toString(argument(args, 0)).trimmed();
                                         bool negative = false;
                                         if (!s.isEmpty() && (s.at(0) == u'+' || s.at(0) == u'-')) {
                                             negative = s.at(0) == u'-';
                                             s.remove(0, 1);
                                         }
                                         int radix = static_cast<int>(rt.toNumber(argument(args, 1)));
                                         if (std::isnan(rt.toNumber(argument(args, 1))))
                                             radix = 0;
                                         if (radix != 0 && (radix < 2 || radix > 36))
                                             return Value::number(std::numeric_limits<double>::quiet_NaN());
                                         if ((radix == 0 || radix == 16) &&
                                             (s.startsWith(QLatin1String("0x")) || s.startsWith(QLatin1String("0X")))) {
                                             s.remove(0, 2);
                                             radix = 16;
                                         }
                                         if (radix == 0)
                                             radix = 10;
                                         double v = 0.0;
                                         qsizetype n = 0;
                                         for (const QChar c : s) {
                                             const QChar l = c.toLower();
                                             int d = -1;
                                             if (l >= u'0' && l <= u'9')
                                                 d = l.unicode() - u'0';
                                             else if (l >= u'a' && l <= u'z')
                                                 d = l.unicode() - u'a' + 10;
                                             if (d < 0 || d >= radix)
                                                 break;
                                             v = v * radix + d;
                                             ++n;
                                         }
                                         if (n == 0)
                                             return Value::number(std::numeric_limits<double>::quiet_NaN());
                                         return Value::number(negative ? -v : v);
                                     });
    ctor->setOwn(QStringLiteral("parseFloat"), Value::object(parseFloatFn), false);
    ctor->setOwn(QStringLiteral("parseInt"), Value::object(parseIntFn), false);

    const auto unwrap = [](Runtime &rt, const Value &self) {
        if (self.isNumber())
            return self.asNumber();
        if (self.isObject() && self.asObject()->cls == Object::Class::Number)
            return self.asObject()->primitive.asNumber();
        rt.throwTypeError(QStringLiteral("not a number"));
    };
    addMethod(
        runtime, proto, QStringLiteral("valueOf"), 0,
        [unwrap](Runtime &rt, const Value &self, const Values &, Object *) { return Value::number(unwrap(rt, self)); });
    addMethod(runtime, proto, QStringLiteral("toString"), 1,
              [unwrap](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const double x = unwrap(rt, self);
                  const Value r = argument(args, 0);
                  const double radix = r.isUndefined() ? 10 : std::trunc(rt.toNumber(r));
                  if (radix < 2 || radix > 36)
                      rt.throwError(ErrorType::RangeError, QStringLiteral("radix must be between 2 and 36"));
                  if (radix == 10 || std::isnan(x) || std::isinf(x))
                      return Value::string(Runtime::numberToString(x));
                  return Value::string(radixString(x, static_cast<int>(radix)));
              });
    addMethod(runtime, proto, QStringLiteral("toLocaleString"), 0,
              [unwrap](Runtime &rt, const Value &self, const Values &, Object *) {
                  return Value::string(Runtime::numberToString(unwrap(rt, self)));
              });
    addMethod(runtime, proto, QStringLiteral("toFixed"), 1,
              [unwrap](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const double x = unwrap(rt, self);
                  const double f = std::trunc(rt.toNumber(argument(args, 0)));
                  const int digits = std::isnan(f) ? 0 : static_cast<int>(f);
                  if (digits < 0 || digits > 100)
                      rt.throwError(ErrorType::RangeError,
                                    QStringLiteral("toFixed() digits must be between 0 and 100"));
                  if (std::isnan(x) || std::fabs(x) >= 1e21)
                      return Value::string(Runtime::numberToString(x));
                  char buffer[160];
                  std::snprintf(buffer, sizeof buffer, "%.*f", digits, x);
                  QString s = QString::fromLatin1(buffer);
                  if (s.startsWith(u'-') && s.mid(1).count(u'0') + s.count(u'.') == s.size() - 1)
                      s.remove(0, 1);
                  return Value::string(s);
              });
    addMethod(runtime, proto, QStringLiteral("toExponential"), 1,
              [unwrap](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const double x = unwrap(rt, self);
                  const Value f = argument(args, 0);
                  if (std::isnan(x) || std::isinf(x))
                      return Value::string(Runtime::numberToString(x));
                  if (f.isUndefined()) {
                      // As many digits as it takes.
                      for (int p = 0; p <= 20; ++p) {
                          char buffer[64];
                          std::snprintf(buffer, sizeof buffer, "%.*e", p, x);
                          if (std::strtod(buffer, nullptr) == x)
                              return Value::string(exponential(x, p));
                      }
                  }
                  const double d = std::trunc(rt.toNumber(f));
                  if (d < 0 || d > 100)
                      rt.throwError(ErrorType::RangeError,
                                    QStringLiteral("toExponential() digits must be between 0 and 100"));
                  return Value::string(exponential(x, static_cast<int>(d)));
              });
    addMethod(runtime, proto, QStringLiteral("toPrecision"), 1,
              [unwrap](Runtime &rt, const Value &self, const Values &args, Object *) {
                  const double x = unwrap(rt, self);
                  const Value pv = argument(args, 0);
                  if (pv.isUndefined() || std::isnan(x) || std::isinf(x))
                      return Value::string(Runtime::numberToString(x));
                  const double p = std::trunc(rt.toNumber(pv));
                  if (p < 1 || p > 100)
                      rt.throwError(ErrorType::RangeError,
                                    QStringLiteral("toPrecision() precision must be between 1 and 100"));
                  const int precision = static_cast<int>(p);
                  if (x == 0) {
                      QString s = QStringLiteral("0");
                      if (precision > 1)
                          s += u'.' + QString(precision - 1, u'0');
                      return Value::string(s);
                  }
                  char buffer[160];
                  std::snprintf(buffer, sizeof buffer, "%.*e", precision - 1, x);
                  const int e = std::atoi(std::strchr(buffer, 'e') + 1);
                  if (e < -6 || e >= precision)
                      return Value::string(exponential(x, precision - 1));
                  std::snprintf(buffer, sizeof buffer, "%.*f", std::max(0, precision - 1 - e), x);
                  return Value::string(QString::fromLatin1(buffer));
              });
}

} // namespace

Object *newRegExp(Runtime &runtime, const QString &pattern, const QString &flags) {
    for (const QChar c : flags)
        if (!QStringLiteral("gimsuyd").contains(c) || flags.count(c) > 1)
            runtime.throwError(ErrorType::SyntaxError,
                               QStringLiteral("invalid regular expression flags '%1'").arg(flags));
    auto *r = runtime.heap.make<RegExpObject>(runtime.regexpProto);
    r->source = pattern.isEmpty() ? QStringLiteral("(?:)") : pattern;
    r->flags = flags;
    QRegularExpression::PatternOptions options;
    if (flags.contains(u'i'))
        options |= QRegularExpression::CaseInsensitiveOption;
    if (flags.contains(u'm'))
        options |= QRegularExpression::MultilineOption;
    if (flags.contains(u's'))
        options |= QRegularExpression::DotMatchesEverythingOption;
    if (flags.contains(u'u'))
        options |= QRegularExpression::UseUnicodePropertiesOption;
    r->re = QRegularExpression(pattern, options);
    if (!r->re.isValid())
        runtime.throwError(ErrorType::SyntaxError,
                           QStringLiteral("invalid regular expression /%1/: %2").arg(pattern, r->re.errorString()));
    Property &li = r->defineOwn(QStringLiteral("lastIndex"));
    li.value = Value::number(0);
    li.enumerable = false;
    return r;
}

void installStringAndRegExp(Runtime &runtime) {
    installString(runtime);
    installRegExp(runtime);
}

void installNumber(Runtime &runtime) { installNumberImpl(runtime); }

} // namespace QtJSPrivate
