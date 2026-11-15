// MIT License - see LICENSE
// Conditionals that ntmoc must evaluate. A signal in a branch that is not compiled would get a body
// in the generated file for a member the class does not have, and the test would not compile.
#pragma once

#include "moc_preprocessor_cfg.h"
#include "moc_preprocessor_cfg.h" // #pragma once

#include <QObject>

class PreprocessorEmitter : public QObject {
    Q_OBJECT
    Q_MOC_INCLUDE("moc_preprocessor_cfg.h")
signals:
#ifdef NT_MOC_TEST_UNDEFINED
    void undefinedMacro();
#endif
#ifndef NT_MOC_TEST_UNDEFINED
    void notUndefinedMacro();
#endif
#if NT_MOC_TEST_HAVE_B && !defined(NT_MOC_TEST_UNDEFINED)
    void fromInclude();
#endif
#if NT_MOC_TEST_CURRENT >= NT_MOC_TEST_VERSION(6, 1)
    void functionMacro();
#elif 1
    void elifAfterTaken();
#else
    void elseAfterTaken();
#endif
#if NT_MOC_TEST_CAT(NT_MOC_TEST_FEATURE_, x)
    void pasted();
#endif
#ifdef NT_MOC_TEST_DEFINE
    void commandLine();
#else
    void notCommandLine();
#endif
#pragma push_macro("NT_MOC_TEST_HAVE_B")
#undef NT_MOC_TEST_HAVE_B
#if NT_MOC_TEST_HAVE_B
    void undefined();
#endif
#pragma pop_macro("NT_MOC_TEST_HAVE_B")
#if defined NT_MOC_TEST_HAVE_B && (NT_MOC_TEST_HAVE_B ? 0x10 : 0) == 16 && 'a' == 97 && (1 << 4) == 16
    void popped();
#endif
#if 0 /* a comment */ || 0
    void disabled();
#endif // a comment
#define NT_MOC_TEST_LOCAL
#ifdef NT_MOC_TEST_LOCAL
    void localDefine();
#endif
#if defined(NT_MOC_TEST_UNDEFINED) || \
    NT_MOC_TEST_VERSION(1, 0) == 100
    void continuedLine();
#endif
#if __has_include("moc_preprocessor_cfg.h")
    void hasInclude();
#endif
};
