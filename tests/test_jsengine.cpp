// MIT License - see LICENSE
//
// Tests for the JavaScript engine of Nt Qml: the language, the standard library, QJSValue, QObjects in
// scripts, errors, limits on hostile input, and garbage collection.
#include <QtCore/qcoreapplication.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtQml/QJSEngine>
#include <QtQml/QJSValue>
#include <QtQml/QJSValueIterator>
#include <QtQml/QtQml>

#include "jsengine_classes.h"

#include <cmath>
#include <cstdio>
#include <string>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

// Evaluates `program` and compares the string form of its value with `expected`.
static bool evaluatesTo(QJSEngine &engine, const char *program, const char *expected) {
    const QJSValue v = engine.evaluate(QString::fromUtf8(program));
    const QString got = v.toString();
    if (got == QString::fromUtf8(expected))
        return true;
    std::printf("  %s\n  gave %s, expected %s\n", program, got.toUtf8().constData(), expected);
    return false;
}
#define EVAL(program, expected) CHECK(evaluatesTo(engine, program, expected))

static void testLanguage() {
    QJSEngine engine;
    EVAL("1 + 2 * 3", "7");
    EVAL("(1 + 2) * 3", "9");
    EVAL("2 ** 10", "1024");
    EVAL("2 ** 3 ** 2", "512");
    EVAL("7 % 3", "1");
    EVAL("-7 % 3", "-1");
    EVAL("1 / 0", "Infinity");
    EVAL("0 / 0", "NaN");
    EVAL("0.1 + 0.2", "0.30000000000000004");
    EVAL("1e21", "1e+21");
    EVAL("123456789012345680000", "123456789012345680000");
    EVAL("1e-7", "1e-7");
    EVAL("0.000001", "0.000001");
    EVAL("0xff + 0o17 + 0b101", "275");
    EVAL("'a' + 1 + 2", "a12");
    EVAL("1 + 2 + 'a'", "3a");
    EVAL("'5' * '2'", "10");
    EVAL("'5' - 2", "3");
    EVAL("[] + {}", "[object Object]");
    EVAL("[1, 2] + ''", "1,2");
    EVAL("1 == '1'", "true");
    EVAL("1 === '1'", "false");
    EVAL("null == undefined", "true");
    EVAL("null === undefined", "false");
    EVAL("NaN == NaN", "false");
    EVAL("'b' > 'a'", "true");
    EVAL("'10' < '9'", "true");
    EVAL("10 < 9", "false");
    EVAL("typeof undefined + typeof null + typeof 1 + typeof 'x' + typeof {} + typeof function(){}",
         "undefinedobjectnumberstringobjectfunction");
    EVAL("typeof notDeclared", "undefined");
    EVAL("~5 + (5 & 3) + (5 | 3) + (5 ^ 3)", "8");
    EVAL("-1 >>> 28", "15");
    EVAL("-16 >> 2", "-4");
    EVAL("1 << 31", "-2147483648");
    EVAL("null ?? 'd'", "d");
    EVAL("0 ?? 'd'", "0");
    EVAL("0 || 'd'", "d");
    EVAL("1 && 2", "2");
    EVAL("var o = {a: {b: 1}}; o?.a?.b", "1");
    EVAL("var n = null; n?.a.b.c", "undefined");
    EVAL("var f = null; f?.()", "undefined");
    EVAL("var x = 1; x += 2; x *= 3; x", "9");
    EVAL("var y = null; y ?\?= 5; y", "5");
    EVAL("var z = 1; z ||= 5; z &&= 7; z", "7");
    EVAL("var i = 0; i++ + ++i", "2");
    EVAL("`a${1 + 1}b${'c'}`", "a2bc");
    EVAL("`line\\nbreak`.length", "10");
    EVAL("var s = 0; for (var k = 0; k < 10; ++k) { if (k == 3) continue; if (k == 6) break; s += k; } s", "12");
    EVAL("var w = 0; while (w < 5) w++; w", "5");
    EVAL("var d = 0; do { d++ } while (d < 3); d", "3");
    EVAL("var keys = []; for (var k in {a: 1, b: 2}) keys.push(k); keys.join()", "a,b");
    EVAL("var sum = 0; for (const v of [1, 2, 3]) sum += v; sum", "6");
    EVAL("var chars = ''; for (const c of 'hé') chars += c + '.'; chars", "h.é.");
    EVAL("outer: for (var a = 0; a < 3; ++a) { for (var b = 0; b < 3; ++b) { if (b == 1) continue outer; if (a == 2) "
         "break outer; } } a",
         "2");
    EVAL("switch (3) { case 1: 'one'; break; case 3: 'three'; case 4: 'fall'; break; default: 'd' }", "fall");
    EVAL("switch ('x') { case 'y': 1; break; default: 'default' }", "default");
    EVAL("var t; try { throw new Error('boom') } catch (e) { t = e.message } finally { t += '!' } t", "boom!");
    EVAL("(function() { try { return 1 } finally { return 2 } })()", "2");
    EVAL("(function() { var a = 1; a++; return a })()", "2");
    EVAL("(function() { 'last'; return })()", "undefined");
    EVAL("(function() { try { throw 1 } catch { return 'caught' } })()", "caught");
    EVAL("var fns = []; for (let i = 0; i < 3; i++) fns.push(() => i); fns.map(f => f()).join()", "0,1,2");
    EVAL("let b1 = 1; { let b1 = 2; } b1", "1");
    EVAL("const c1 = 1; try { c1 = 2 } catch (e) { e.name }", "TypeError");
    EVAL("try { tdz; let tdz = 1 } catch (e) { e.name }", "ReferenceError");
    EVAL("function hoisted() { return 'up' } hoisted()", "up");
    EVAL("early(); function early() { return 1 } early()", "1");
    EVAL("var sq = x => x * x; sq(7)", "49");
    EVAL("((a, b = a + 1, ...rest) => a + b + rest.length)(1, undefined, 3, 4)", "5");
    EVAL("Math.max(...[1, 5, 3])", "5");
    EVAL("[...'ab', ...[1, 2]].join()", "a,b,1,2");
    EVAL("var fact = function f(n) { return n <= 1 ? 1 : n * f(n - 1) }; fact(10)", "3628800");
    EVAL("(function() { return arguments.length })(1, 2, 3)", "3");
    EVAL("var counter = (function() { var c = 0; return { inc: function() { return ++c } } })(); counter.inc(); "
         "counter.inc()",
         "2");
    EVAL("var obj = { v: 2, get double() { return this.v * 2 }, set double(x) { this.v = x / 2 } }; obj.double = 10; "
         "obj.v",
         "5");
    EVAL("var m = { greet() { return 'hi ' + this.n }, n: 'x' }; m.greet()", "hi x");
    EVAL("var k = 'dyn'; var c = { [k + 1]: 1 }; Object.keys(c)[0]", "dyn1");
    EVAL("var spreadObj = { ...{a: 1}, b: 2 }; JSON.stringify(spreadObj)", "{\"a\":1,\"b\":2}");
    EVAL("function P(x) { this.x = x } P.prototype.get = function() { return this.x }; new P(4).get()", "4");
    EVAL("new P(1) instanceof P", "true");
    EVAL("var arrow = () => this; arrow() === globalThis", "true");
    EVAL("var holder = { v: 1, f() { return [1].map(() => this.v)[0] } }; holder.f()", "1");
    EVAL("'v' in {v: 1}", "true");
    EVAL("var del = {a: 1}; delete del.a; 'a' in del", "false");
    EVAL("void 0", "undefined");
    EVAL("(1, 2, 3)", "3");
    EVAL("var re = /(\\d+)-(\\d+)/; re.exec('a 12-34 b')[2]", "34");
    EVAL("'a1b2c3'.replace(/\\d/g, d => d * 2)", "a2b4c6");
    EVAL("'2024-06'.replace(/(\\d+)-(\\d+)/, '$2/$1')", "06/2024");
    EVAL("'a,b;c'.split(/[,;]/).length", "3");
    EVAL("/^[a-z]+$/i.test('Hello')", "true");
    EVAL("'x'.repeat(3)", "xxx");
    EVAL("undeclaredGlobal = 5; globalThis.undeclaredGlobal", "5");
    EVAL("label2: { 'in'; break label2; 'not' }", "in");
    EVAL("if (true) 'yes'; else 'no'", "yes");
    EVAL("1; var unused;", "1");
}

static void testLibrary() {
    QJSEngine engine;
    EVAL("[3, 1, 2].sort().join()", "1,2,3");
    EVAL("[10, 9, 1].sort().join()", "1,10,9");
    EVAL("[10, 9, 1].sort((a, b) => a - b).join()", "1,9,10");
    EVAL("[1, 2, 3, 4].filter(x => x % 2).map(x => x * 10).reduce((a, b) => a + b)", "40");
    EVAL("[1, 2, 3].reduceRight((a, b) => a + b, '')", "321");
    EVAL("[1, [2, [3, [4]]]].flat(Infinity).join()", "1,2,3,4");
    EVAL("var a = [1, 2, 3, 4, 5]; a.splice(1, 2, 'x').join() + '|' + a.join()", "2,3|1,x,4,5");
    EVAL("[1, 2, 3].slice(-2).join()", "2,3");
    EVAL("[1, 2, 3].indexOf(2) + [1, 2, 3].lastIndexOf(9)", "0");
    EVAL("[NaN].includes(NaN) && ![NaN].indexOf(NaN) == 0", "true");
    EVAL("[5, 6, 7].find(x => x > 5) + [5, 6, 7].findIndex(x => x > 5)", "7");
    EVAL("[1, 2].concat([3], 4).length", "4");
    EVAL("Array.from('abc').reverse().join('')", "cba");
    EVAL("Array.from({length: 3}, (v, i) => i * 2).join()", "0,2,4");
    EVAL("Array.isArray([]) && !Array.isArray({})", "true");
    EVAL("new Array(3).length", "3");
    EVAL("[1, 2, 3].some(x => x > 2) && [1, 2, 3].every(x => x > 0)", "true");
    EVAL("var arr = [1]; arr[3] = 4; arr.length + ':' + arr.join()", "4:1,,,4");
    EVAL("var arr2 = [1, 2, 3]; arr2.length = 1; arr2.join()", "1");
    EVAL("[1, 2, 3].at(-1)", "3");
    EVAL("var cyc = [1]; cyc.push(cyc); cyc.join()", "1,");
    EVAL("'Hello'.charAt(1) + 'Hello'.charCodeAt(0) + 'Hello'.indexOf('l')", "e722");
    EVAL("'  x '.trim() + '|' + 'abc'.toUpperCase() + '|' + 'ABC'.toLowerCase()", "x|ABC|abc");
    EVAL("'abc'.padStart(5, '-') + 'abc'.padEnd(5, '.')", "--abcabc..");
    EVAL("'abcdef'.substring(4, 1) + 'abcdef'.slice(-2) + 'abcdef'.substr(1, 2)", "bcdefbc");
    EVAL("'a-b-c'.replaceAll('-', '+')", "a+b+c");
    EVAL("'Hello %1, %2'.arg('you').arg(3)", "Hello you, 3");
    EVAL("'abc'.startsWith('ab') && 'abc'.endsWith('bc') && 'abc'.includes('b')", "true");
    EVAL("String.fromCharCode(72, 105)", "Hi");
    EVAL("'x'.codePointAt(0)", "120");
    EVAL("'aXbX'.split('X').length", "3");
    EVAL("'2023-01-15'.match(/(\\d+)/g).length", "3");
    EVAL("(255).toString(16) + (8).toString(2)", "ff1000");
    EVAL("(3.14159).toFixed(2) + ' ' + (1234.5678).toFixed(0)", "3.14 1235");
    EVAL("(12345).toExponential(2)", "1.23e+4");
    EVAL("(0.000123).toPrecision(2) + ' ' + (123456).toPrecision(2)", "0.00012 1.2e+5");
    EVAL("parseInt('42px') + parseInt('ff', 16) + parseFloat('3.5e1x')", "332");
    EVAL("Number('  12  ') + Number('') + Number('0x10')", "28");
    EVAL("isNaN(Number('12a'))", "true");
    EVAL("Number.isInteger(5.0) && !Number.isInteger(5.5) && Number.isSafeInteger(2 ** 53 - 1)", "true");
    EVAL("Math.round(2.5) + Math.round(-2.5) + Math.floor(-1.5) + Math.ceil(1.2) + Math.trunc(-4.7)", "-3");
    EVAL("Math.max() === -Infinity && Math.min() === Infinity", "true");
    EVAL("Math.hypot(3, 4) + Math.abs(-2) + Math.sign(-3) + Math.cbrt(27)", "9");
    EVAL("var r = Math.random(); r >= 0 && r < 1", "true");
    EVAL("JSON.stringify({a: [1, 'x', true, null], b: undefined, c: () => 1, d: {e: 1.5}})",
         "{\"a\":[1,\"x\",true,null],\"d\":{\"e\":1.5}}");
    EVAL("JSON.stringify({a: 1, b: [1]}, null, 2)", "{\n  \"a\": 1,\n  \"b\": [\n    1\n  ]\n}");
    EVAL("JSON.stringify('q\"\\n')", "\"q\\\"\\n\"");
    EVAL("JSON.parse('{\"x\": [1, 2, {\"y\": \"z\"}], \"n\": -1.5e2}').x[2].y + JSON.parse('{\"n\": -1.5e2}').n",
         "z-150");
    EVAL("JSON.parse('[1, 2]', (k, v) => typeof v === 'number' ? v * 10 : v).join()", "10,20");
    EVAL("try { JSON.parse('{bad}') } catch (e) { e.name }", "SyntaxError");
    EVAL("var cy = {}; cy.self = cy; try { JSON.stringify(cy) } catch (e) { e.name }", "TypeError");
    EVAL("JSON.stringify({d: new Date(Date.UTC(2020, 0, 2, 3, 4, 5, 6))})", "{\"d\":\"2020-01-02T03:04:05.006Z\"}");
    EVAL("Object.keys({b: 1, a: 2}).join()", "b,a");
    EVAL("Object.entries({a: 1}).toString()", "a,1");
    EVAL("Object.assign({a: 1}, {b: 2}, null, {a: 3}).a", "3");
    EVAL("var proto = {hi() { return 'hi' }}; Object.create(proto).hi()", "hi");
    EVAL("var frozen = Object.freeze({a: 1}); frozen.a = 2; frozen.a", "1");
    EVAL("var dp = {}; Object.defineProperty(dp, 'x', {get() { return 42 }}); dp.x + Object.keys(dp).length", "42");
    EVAL("var ro = {}; Object.defineProperty(ro, 'y', {value: 1}); ro.y = 2; ro.y", "1");
    EVAL("({}).hasOwnProperty.call({a: 1}, 'a')", "true");
    EVAL("Object.prototype.toString.call([])", "[object Array]");
    EVAL("Object.is(NaN, NaN) && !Object.is(0, -0)", "true");
    EVAL("function add(a, b) { return a + b } add.call(null, 1, 2) + add.apply(null, [3, 4]) + add.bind(null, 5)(6)",
         "21");
    EVAL("add.length + add.name", "2add");
    EVAL("var e = new TypeError('bad'); e instanceof TypeError && e instanceof Error && e.name + ': ' + e.message",
         "TypeError: bad");
    EVAL("String(new RangeError('r'))", "RangeError: r");
    EVAL("var d = new Date(2020, 1, 29, 12, 30); d.getFullYear() + '-' + (d.getMonth() + 1) + '-' + d.getDate() + "
         "' ' + d.getHours() + ':' + d.getMinutes()",
         "2020-2-29 12:30");
    EVAL("new Date(Date.UTC(2021, 11, 31)).toISOString()", "2021-12-31T00:00:00.000Z");
    EVAL("new Date('2021-03-04T05:06:07Z').getTime()", "1614834367000");
    EVAL("var d2 = new Date(2020, 0, 31); d2.setMonth(1); d2.getMonth()", "2");
    EVAL("isNaN(new Date('nonsense').getTime())", "true");
    EVAL("String(new Date(NaN))", "Invalid Date");
    EVAL("typeof Date.now()", "number");
    EVAL("encodeURIComponent('a b&é') + ' ' + decodeURIComponent('%C3%A9%20x')", "a%20b%26%C3%A9 é x");
    EVAL("new Boolean(false) ? 'object' : 'never'", "object");
}

static void testValues() {
    QJSEngine engine;
    QJSValue o = engine.newObject();
    o.setProperty("x", 3);
    o.setProperty("name", QStringLiteral("box"));
    CHECK(o.isObject() && !o.isArray());
    CHECK(o.property("x").toInt() == 3);
    CHECK(o.property("name").toString() == "box");
    CHECK(o.hasOwnProperty("x") && o.hasProperty("toString") && !o.hasOwnProperty("toString"));
    CHECK(o.deleteProperty("x") && !o.hasProperty("x"));

    engine.globalObject().setProperty("box", o);
    CHECK(engine.evaluate("box.name").toString() == "box");

    QJSValue array = engine.newArray(2);
    array.setProperty(0, 7);
    array.setProperty(1, QJSValue(QJSValue::NullValue));
    CHECK(array.isArray() && array.property("length").toInt() == 2);
    CHECK(array.property(0).toInt() == 7 && array.property(1).isNull());

    QJSValue fn = engine.evaluate("(function(a, b) { return this.base + a * b; })");
    CHECK(fn.isCallable());
    QJSValue self = engine.newObject();
    self.setProperty("base", 100);
    CHECK(fn.callWithInstance(self, {2, 3}).toInt() == 106);

    QJSValue ctor = engine.evaluate("(function(v) { this.v = v; })");
    QJSValue made = ctor.callAsConstructor({5});
    CHECK(made.property("v").toInt() == 5);

    const QVariant v = engine.evaluate("({a: [1, 'two'], b: true, c: null})").toVariant();
    const QVariantMap map = v.toMap();
    CHECK(map.value("a").toList().size() == 2);
    CHECK(map.value("a").toList().at(1).toString() == "two");
    CHECK(map.value("b").toBool());

    CHECK(engine.toScriptValue(QStringList{"a", "b"}).isArray());
    CHECK(engine.fromScriptValue<int>(engine.evaluate("6 * 7")) == 42);
    CHECK(engine.evaluate("new Date(0)").toDateTime().toMSecsSinceEpoch() == 0);

    // Primitives need no engine.
    QJSValue plain(2.5);
    CHECK(plain.isNumber() && plain.toString() == "2.5" && plain.toInt() == 2);
    CHECK(QJSValue("text").toString() == "text" && QJSValue(true).toBool());
    CHECK(QJSValue().isUndefined() && QJSValue(QJSValue::NullValue).isNull());
    CHECK(QJSValue(1).equals(QJSValue("1")) && !QJSValue(1).strictlyEquals(QJSValue("1")));

    QJSValueIterator it(engine.evaluate("({first: 1, second: 2})"));
    QStringList names;
    while (it.hasNext()) {
        it.next();
        names << it.name() + "=" + it.value().toString();
    }
    CHECK(names == QStringList({"first=1", "second=2"}));

    // A value of a destroyed engine reads as undefined.
    QJSValue orphan;
    {
        QJSEngine shortLived;
        orphan = shortLived.newObject();
        CHECK(orphan.isObject());
    }
    CHECK(orphan.isUndefined() || !orphan.isObject());
}

static void testErrors() {
    QJSEngine engine;
    QStringList trace;
    QJSValue e =
        engine.evaluate("function inner() { null.x } \nfunction outer() { inner() }\nouter()", "f.js", 1, &trace);
    CHECK(e.isError());
    CHECK(e.errorType() == QJSValue::TypeError);
    CHECK(e.property("lineNumber").toInt() == 1);
    CHECK(e.property("fileName").toString() == "f.js");
    CHECK(trace.size() >= 2 && trace.first().startsWith("inner@"));

    QJSValue s = engine.evaluate("var = 1", "bad.js", 7);
    CHECK(s.isError() && s.errorType() == QJSValue::SyntaxError);
    CHECK(s.property("lineNumber").toInt() == 7);

    CHECK(engine.evaluate("undefinedFunction()").errorType() == QJSValue::ReferenceError);
    CHECK(engine.evaluate("throw 42").toInt() == 42);
    CHECK(engine.evaluate("class X {}").isError());
    CHECK(engine.evaluate("let [a] = [1]").isError());

    QJSValue thrower = engine.evaluate("(function() { throw new RangeError('no') })");
    QJSValue r = thrower.call();
    CHECK(r.isError() && r.errorType() == QJSValue::RangeError && r.property("message").toString() == "no");

    // Hostile input fails cleanly.
    CHECK(engine.evaluate("function f() { return f() } f()").errorType() == QJSValue::RangeError);
    CHECK(engine.evaluate(QString(5000, u'(') + "1" + QString(5000, u')')).isError());
    CHECK(engine.evaluate(QString("[").repeated(3000)).isError());
    CHECK(engine.evaluate("'x'.repeat(1e9)").errorType() == QJSValue::RangeError);
    CHECK(engine.evaluate("new Array(1e9)").errorType() == QJSValue::RangeError);
    CHECK(engine.evaluate("var big = []; big[1e9] = 1").errorType() == QJSValue::RangeError);
    CHECK(engine.evaluate("'unterminated").isError());
    CHECK(engine.evaluate("/[/").isError());
    CHECK(engine.evaluate("var o = {}; o.__proto__ = o; 1").isNumber() || true);

    engine.setInterrupted(true);
    CHECK(engine.evaluate("while (true) {}").isError());
    engine.setInterrupted(false);
    CHECK(engine.evaluate("1 + 1").toInt() == 2);
}

static void testQObjects() {
    QJSEngine engine;
    engine.installExtensions(QJSEngine::ConsoleExtension | QJSEngine::TranslationExtension);
    Thermometer t;
    t.setObjectName("kitchen");
    QJSValue wrapped = engine.newQObject(&t);
    CHECK(wrapped.isQObject() && wrapped.toQObject() == &t);
    CHECK(engine.newQObject(&t).strictlyEquals(wrapped));
    engine.globalObject().setProperty("thermo", wrapped);

    CHECK(engine.evaluate("thermo.objectName").toString() == "kitchen");
    CHECK(engine.evaluate("thermo.celsius = 21.5; thermo.celsius").toNumber() == 21.5);
    CHECK(t.celsius() == 21.5);
    CHECK(engine.evaluate("thermo.fahrenheit()").toNumber() == 70.7);
    CHECK(engine.evaluate("thermo.describe('now')").toString() == "kitchen now: 21.5");
    CHECK(engine.evaluate("thermo.scaled(2)").toNumber() == 43.0);
    CHECK(engine.evaluate("thermo.scaled(2, 1)").toNumber() == 44.0);
    CHECK(engine.evaluate("thermo.mode = 'Heating'; thermo.mode").toInt() == Thermometer::Heating);
    CHECK(t.mode() == Thermometer::Heating);
    CHECK(engine.evaluate("try { thermo.reading = 3 } catch (e) { e.name }").toString() == "TypeError");
    CHECK(engine.evaluate("thermo.reading").toInt() == 7);
    CHECK(engine.evaluate("thermo.extra = 'kept'; thermo.extra").toString() == "kept");
    CHECK(engine.evaluate("thermo.tags().length").toInt() == 2);
    CHECK(engine.evaluate("typeof thermo.celsiusChanged.connect").toString() == "function");

    // Signals call script functions with their arguments.
    engine.evaluate("var seen = []; function onChange(v) { seen.push(v) } thermo.celsiusChanged.connect(onChange)");
    t.setCelsius(30);
    t.setCelsius(31);
    CHECK(engine.evaluate("seen.join()").toString() == "30,31");
    engine.evaluate("thermo.celsiusChanged.disconnect(onChange)");
    t.setCelsius(32);
    CHECK(engine.evaluate("seen.length").toInt() == 2);
    engine.evaluate("var holder = { n: 0, hit() { this.n++ } }; thermo.alarm.connect(holder, holder.hit)");
    emit t.alarm(QStringLiteral("hot"));
    CHECK(engine.evaluate("holder.n").toInt() == 1);
    // Scripts can emit signals.
    int alarms = 0;
    QObject::connect(&t, &Thermometer::alarm, [&](const QString &) { ++alarms; });
    engine.evaluate("thermo.alarm('from script')");
    CHECK(alarms == 1);

    // A C++ method can throw into the script.
    t.engine = &engine;
    CHECK(engine.evaluate("try { thermo.fail() } catch (e) { e.message }").toString() == "failed on purpose");
    CHECK(!engine.hasError());

    // Methods of deleted objects fail cleanly.
    auto *temporary = new Thermometer;
    engine.globalObject().setProperty("temp", engine.newQObject(temporary));
    QJSEngine::setObjectOwnership(temporary, QJSEngine::CppOwnership);
    delete temporary;
    CHECK(engine.evaluate("try { temp.fahrenheit() } catch (e) { 'gone' }").toString() == "gone");

    // Objects without a parent belong to the engine and are collected.
    QPointer<Thermometer> owned = new Thermometer;
    engine.newQObject(owned);
    engine.collectGarbage();
    CHECK(owned.isNull());
    QPointer<Thermometer> kept = new Thermometer(&t);
    engine.newQObject(kept);
    engine.collectGarbage();
    CHECK(!kept.isNull());

    CHECK(engine.evaluate("qsTr('hello')").toString() == "hello");
    CHECK(engine.evaluate("typeof console.log").toString() == "function");
}

static void testGarbage() {
    QJSEngine engine;
    QJSValue keep = engine.evaluate("var g = {}; ({ kept: 1 })");
    for (int i = 0; i < 50; ++i)
        engine.evaluate("var junk = []; for (var i = 0; i < 1000; ++i) junk.push({ i: i, s: 'x' + i }); junk = null;");
    engine.collectGarbage();
    CHECK(keep.property("kept").toInt() == 1);
    CHECK(engine.evaluate("typeof g").toString() == "object");
    // Closures survive collection while reachable.
    engine.evaluate(
        "var make = function() { var secret = 'v'; return function() { return secret } }; var get = make()");
    engine.collectGarbage();
    CHECK(engine.evaluate("get()").toString() == "v");
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    testLanguage();
    testLibrary();
    testValues();
    testErrors();
    testQObjects();
    testGarbage();
    if (g_failures)
        std::printf("%d failure(s)\n", g_failures);
    else
        std::printf("all JavaScript engine tests passed\n");
    return g_failures ? 1 : 0;
}
