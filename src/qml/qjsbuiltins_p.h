// MIT License - see LICENSE
//
// Helpers shared by the files that make the built-in objects of the JavaScript runtime.
#pragma once

#include "qjsruntime_p.h"

namespace QtJSPrivate {

inline Value argument(const Values &args, std::size_t i) { return i < args.size() ? args[i] : Value(); }

// Adds a method, not enumerable, to `object`.
Function *addMethod(Runtime &rt, Object *object, const QString &name, int length, Function::Native fn);
// Adds a constant, neither enumerable nor writable.
void addConstant(Object *object, const QString &name, const Value &value);
// Makes a constructor with its prototype, and puts it on the global object.
Function *addConstructor(Runtime &rt, const QString &name, int length, Object *prototype, Function::Native fn);

// The parts of the standard library, by file.
void installStringAndRegExp(Runtime &rt);
void installNumber(Runtime &rt);
void installDate(Runtime &rt);
void installJson(Runtime &rt);

// JSON text of `value`, as JSON.stringify(value) with no replacer and `indent`.
QString jsonStringify(Runtime &rt, const Value &value, const QString &indent);

} // namespace QtJSPrivate
