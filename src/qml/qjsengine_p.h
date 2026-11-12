// MIT License - see LICENSE
//
// What QJSValue holds, and how the public classes reach the runtime.
#pragma once

#include <QtQml/qjsengine.h>
#include <QtQml/qjsvalue.h>

#include "qjsruntime_p.h"

// A value and the engine it belongs to. Objects it holds are kept alive (Runtime::persistent) until the
// last QJSValue that refers to them is gone, or the engine is.
struct QJSValuePrivate {
    QJSValuePrivate(QtJSPrivate::Runtime *runtime, const QtJSPrivate::Value &v);
    ~QJSValuePrivate();
    QJSValuePrivate(const QJSValuePrivate &) = delete;
    QJSValuePrivate &operator=(const QJSValuePrivate &) = delete;

    void release();

    QtJSPrivate::Runtime *rt;
    QtJSPrivate::Value value;
};

namespace QtJSPrivate {

class EngineAccess {
public:
    static Runtime *runtime(const QJSEngine *engine) { return engine ? engine->m_runtime.get() : nullptr; }
    static QJSValue make(Runtime *rt, const Value &v);
    // The value in `rt`; values of no engine are converted, values of another engine become undefined.
    static Value value(Runtime *rt, const QJSValue &v);
    static const QJSValuePrivate *data(const QJSValue &v) { return v.d.get(); }
    // Runs `f`, turning an error that a script throws into a QJSValue error.
    template <typename F> static QJSValue guarded(Runtime *rt, F f);
};

template <typename F> QJSValue EngineAccess::guarded(Runtime *rt, F f) {
    try {
        const QJSValue r = make(rt, f());
        rt->maybeCollect();
        return r;
    } catch (const Throw &t) {
        const QJSValue r = make(rt, t.value);
        rt->maybeCollect();
        return r;
    }
}

} // namespace QtJSPrivate
