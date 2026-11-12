// MIT License - see LICENSE
//
// The interpreter of Nt Qml: runs programs and calls functions written in JavaScript.
#pragma once

#include "qjsruntime_p.h"

#include <memory>

namespace QtJSPrivate {

struct ProgramData;
struct FunctionNode;

// Runs the top level of `program` in `scope`: its var and function declarations become bindings there.
Value runProgram(Runtime &rt, const std::shared_ptr<const ProgramData> &program, Env *scope, const Value &thisValue);
// Calls a function written in JavaScript.
Value invokeScript(Runtime &rt, Function *f, const Value &thisValue, const Values &args, Object *newTarget);
// Makes a function from its declaration, closing over `scope`.
Function *makeClosure(Runtime &rt, const FunctionNode *node, const std::shared_ptr<const ProgramData> &program,
                      Env *scope);
// Looks a name up in a chain of scopes; returns false if it is not declared. `holder` is set to the object
// that has it, for object scopes.
bool lookupName(Runtime &rt, Env *env, const QString &name, Value *out, Object **holder = nullptr);
// Makes a RegExp object (qjsbuiltins.cpp).
Object *newRegExp(Runtime &rt, const QString &pattern, const QString &flags);

} // namespace QtJSPrivate
