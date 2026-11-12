// MIT License - see LICENSE
#include "qjsinterp_p.h"

#include "qjsparser_p.h"

#include <cmath>
#include <limits>

namespace QtJSPrivate {

namespace {

struct Completion {
    enum Type : unsigned char { Normal, Return, Break, Continue };
    Type type = Normal;
    Value value;
    bool hasValue = false;
    QString label;
};

double toInteger32Base(double d) {
    if (std::isnan(d) || std::isinf(d))
        return 0.0;
    double x = std::trunc(d);
    x = std::fmod(x, 4294967296.0);
    if (x < 0)
        x += 4294967296.0;
    return x;
}

qint32 toInt32(double d) {
    const double x = toInteger32Base(d);
    return static_cast<qint32>(static_cast<quint32>(x));
}

quint32 toUint32(double d) { return static_cast<quint32>(toInteger32Base(d)); }

class Interp {
public:
    Interp(Runtime &r, std::shared_ptr<const ProgramData> p) : rt(r), program(std::move(p)) {}

    Value eval(const Node *n, Env *env);
    Completion exec(const Node *n, Env *env, const QStringList &labels = {});
    Completion execList(const std::vector<Node *> &list, Env *env);
    Completion execBlock(const Node *block, Env *env);
    void declareLexical(const Node *owner, const std::vector<Node *> &statements, Env *scope);
    Completion execFunctionBody(const Node *body, Env *env);

    Runtime &rt;
    std::shared_ptr<const ProgramData> program;

private:
    struct Ref {
        bool isName = false;
        QString name;
        Value base;
        QString key;
    };

    QString propertyKey(const Value &v) { return v.isString() ? v.asString() : rt.toString(v); }
    Ref reference(const Node *target, Env *env);
    Value read(const Ref &r, Env *env);
    void write(const Ref &r, Env *env, const Value &v);
    void assignName(Env *env, const QString &name, const Value &v);
    void initializeName(Env *env, const QString &name, const Value &v);
    Value thisValue(Env *env);

    bool chainPart(const Node *n, Env *env, Value *out);
    bool member(const Node *n, Env *env, Value *out, Value *base);
    bool call(const Node *n, Env *env, Value *out);
    Values arguments(const std::vector<Node *> &list, Env *env);
    void spreadInto(const Value &iterable, Values *out);
    Value binary(Op op, const Value &a, const Value &b);
    bool lessThan(const Value &a, const Value &b, bool leftFirst, bool *undefinedResult);
    Value makeFunction(const Node *n, Env *env, const QString &inferredName = QString());
    Value objectLiteral(const Node *n, Env *env);
    Completion loopBody(const Node *body, Env *env, const QStringList &labels, bool *stop);
    Completion forLoop(const Node *n, Env *env, const QStringList &labels);
    Completion forInLoop(const Node *n, Env *env, const QStringList &labels);
    Completion tryStatement(const Node *n, Env *env);
    Completion switchStatement(const Node *n, Env *env);
    QString calleeName(const Node *n) const;
};

// ---- Names ----

} // namespace

bool lookupName(Runtime &rt, Env *env, const QString &name, Value *out, Object **holder) {
    for (Env *e = env; e; e = e->parent) {
        const auto it = e->vars.constFind(name);
        if (it != e->vars.cend()) {
            if (!it->initialized)
                rt.throwError(ErrorType::ReferenceError,
                              QStringLiteral("cannot access '%1' before it is declared").arg(name));
            *out = it->value;
            if (holder)
                *holder = nullptr;
            return true;
        }
        if (e->object && rt.has(e->object, name)) {
            *out = rt.getFromObject(e->object, name, Value::object(e->object));
            if (holder)
                *holder = e->object;
            return true;
        }
    }
    return false;
}

namespace {

void Interp::assignName(Env *env, const QString &name, const Value &v) {
    for (Env *e = env; e; e = e->parent) {
        const auto it = e->vars.find(name);
        if (it != e->vars.end()) {
            if (!it->initialized)
                rt.throwError(ErrorType::ReferenceError,
                              QStringLiteral("cannot access '%1' before it is declared").arg(name));
            if (!it->mutableBinding)
                rt.throwTypeError(QStringLiteral("assignment to the constant '%1'").arg(name));
            it->value = v;
            return;
        }
        if (e->object && rt.has(e->object, name)) {
            rt.putToObject(e->object, name, v, Value::object(e->object));
            return;
        }
    }
    // An undeclared name becomes a property of the global object.
    rt.putToObject(rt.global, name, v, Value::object(rt.global));
}

void Interp::initializeName(Env *env, const QString &name, const Value &v) {
    for (Env *e = env; e; e = e->parent) {
        const auto it = e->vars.find(name);
        if (it != e->vars.end()) {
            it->value = v;
            it->initialized = true;
            return;
        }
    }
    assignName(env, name, v);
}

Value Interp::thisValue(Env *env) {
    for (Env *e = env; e; e = e->parent)
        if (e->hasThis)
            return e->thisValue;
    return Value();
}

QString Interp::calleeName(const Node *n) const {
    if (n->kind == Node::Kind::Identifier)
        return n->str;
    if (n->kind == Node::Kind::Member && !n->b)
        return calleeName(n->a) + u'.' + n->str;
    if (n->kind == Node::Kind::This)
        return QStringLiteral("this");
    return QStringLiteral("expression");
}

Interp::Ref Interp::reference(const Node *target, Env *env) {
    Ref r;
    if (target->kind == Node::Kind::Identifier) {
        r.isName = true;
        r.name = target->str;
        return r;
    }
    r.base = eval(target->a, env);
    r.key = target->b ? propertyKey(eval(target->b, env)) : target->str;
    if (r.base.isNullish())
        rt.throwTypeError(QStringLiteral("cannot set property '%1' of %2").arg(r.key, rt.toString(r.base)));
    return r;
}

Value Interp::read(const Ref &r, Env *env) {
    if (r.isName) {
        Value v;
        if (!lookupName(rt, env, r.name, &v))
            rt.throwError(ErrorType::ReferenceError, QStringLiteral("%1 is not defined").arg(r.name));
        return v;
    }
    return rt.get(r.base, r.key);
}

void Interp::write(const Ref &r, Env *env, const Value &v) {
    if (r.isName)
        assignName(env, r.name, v);
    else
        rt.put(r.base, r.key, v);
}

// ---- Declarations ----

void Interp::declareLexical(const Node *owner, const std::vector<Node *> &statements, Env *scope) {
    if (owner->names.isEmpty())
        return;
    // Which names are constants, and which are functions declared in the block.
    QStringList constants;
    std::vector<const Node *> functions;
    const auto scan = [&](const std::vector<Node *> &list) {
        for (const Node *s : list) {
            if (s->kind == Node::Kind::VarDecl && s->op2 == 2)
                for (const Node *d : s->list)
                    constants.append(d->str);
            if (s->kind == Node::Kind::FuncDecl)
                functions.push_back(s);
        }
    };
    scan(statements);
    if (owner->kind == Node::Kind::Switch)
        for (const Node *c : owner->list)
            scan(c->list);
    for (const QString &name : owner->names) {
        Env::Binding b;
        b.initialized = false;
        b.mutableBinding = !constants.contains(name);
        scope->vars.insert(name, b);
    }
    for (const Node *f : functions) {
        if (!owner->names.contains(f->func->name))
            continue;
        Env::Binding b;
        b.value = Value::object(makeClosure(rt, f->func, program, scope));
        scope->vars.insert(f->func->name, b);
    }
}

Completion Interp::execList(const std::vector<Node *> &list, Env *env) {
    Completion last;
    for (const Node *s : list) {
        Completion c = exec(s, env);
        if (c.hasValue) {
            last.value = c.value;
            last.hasValue = true;
        }
        if (c.type != Completion::Normal) {
            if (!c.hasValue && last.hasValue) {
                c.value = last.value;
                c.hasValue = true;
            }
            return c;
        }
    }
    return last;
}

Completion Interp::execBlock(const Node *block, Env *env) {
    Env *scope = env;
    if (!block->names.isEmpty()) {
        scope = rt.heap.make<Env>(env);
        declareLexical(block, block->list, scope);
    }
    return execList(block->list, scope);
}

Completion Interp::execFunctionBody(const Node *body, Env *env) {
    declareLexical(body, body->list, env);
    return execList(body->list, env);
}

// ---- Statements ----

Completion Interp::exec(const Node *n, Env *env, const QStringList &labels) {
    rt.currentLine = n->line;
    if (rt.interrupted)
        rt.throwError(ErrorType::Error, QStringLiteral("Interrupted"));
    switch (n->kind) {
    case Node::Kind::ExprStmt: {
        Completion c;
        c.value = eval(n->a, env);
        c.hasValue = true;
        return c;
    }
    case Node::Kind::VarDecl:
        for (const Node *d : n->list) {
            if (n->op2 == 0) {
                if (d->a)
                    assignName(env, d->str,
                               d->a->kind == Node::Kind::Function ? makeFunction(d->a, env, d->str) : eval(d->a, env));
            } else {
                const Value v = !d->a                                ? Value()
                                : d->a->kind == Node::Kind::Function ? makeFunction(d->a, env, d->str)
                                                                     : eval(d->a, env);
                initializeName(env, d->str, v);
            }
        }
        return Completion();
    case Node::Kind::Block:
        // The block the parser puts around a for loop passes the loop's labels on.
        if (!labels.isEmpty() && n->list.size() == 1 &&
            (n->list[0]->kind == Node::Kind::For || n->list[0]->kind == Node::Kind::ForIn)) {
            Env *scope = env;
            if (!n->names.isEmpty()) {
                scope = rt.heap.make<Env>(env);
                declareLexical(n, n->list, scope);
            }
            return exec(n->list[0], scope, labels);
        }
        return execBlock(n, env);
    case Node::Kind::Empty:
    case Node::Kind::FuncDecl:
        return Completion();
    case Node::Kind::If:
        if (rt.toBoolean(eval(n->a, env)))
            return exec(n->b, env);
        if (n->c)
            return exec(n->c, env);
        return Completion();
    case Node::Kind::For:
        return forLoop(n, env, labels);
    case Node::Kind::ForIn:
        return forInLoop(n, env, labels);
    case Node::Kind::While:
    case Node::Kind::DoWhile: {
        Completion result;
        bool first = n->kind == Node::Kind::DoWhile;
        while (first || rt.toBoolean(eval(n->a, env))) {
            first = false;
            bool stop = false;
            Completion c = loopBody(n->d, env, labels, &stop);
            if (c.hasValue) {
                result.value = c.value;
                result.hasValue = true;
            }
            if (c.type != Completion::Normal)
                return c;
            if (stop)
                break;
        }
        return result;
    }
    case Node::Kind::Return: {
        Completion c;
        c.type = Completion::Return;
        c.hasValue = true;
        if (n->a)
            c.value = eval(n->a, env);
        return c;
    }
    case Node::Kind::Break:
    case Node::Kind::Continue: {
        Completion c;
        c.type = n->kind == Node::Kind::Break ? Completion::Break : Completion::Continue;
        c.label = n->str;
        return c;
    }
    case Node::Kind::Throw:
        throw Throw{eval(n->a, env)};
    case Node::Kind::Try:
        return tryStatement(n, env);
    case Node::Kind::Switch:
        return switchStatement(n, env);
    case Node::Kind::Labeled: {
        QStringList inner = labels;
        inner.append(n->str);
        Completion c = exec(n->a, env, inner);
        if (c.type == Completion::Break && c.label == n->str)
            c.type = Completion::Normal;
        return c;
    }
    default:
        break;
    }
    rt.throwError(ErrorType::SyntaxError, QStringLiteral("unexpected statement"));
}

Completion Interp::loopBody(const Node *body, Env *env, const QStringList &labels, bool *stop) {
    Completion c = exec(body, env);
    if (c.type == Completion::Break && (c.label.isEmpty() || labels.contains(c.label))) {
        *stop = true;
        c.type = Completion::Normal;
    } else if (c.type == Completion::Continue && (c.label.isEmpty() || labels.contains(c.label))) {
        c.type = Completion::Normal;
    }
    return c;
}

Completion Interp::forLoop(const Node *n, Env *env, const QStringList &labels) {
    if (n->a) {
        if (n->a->kind == Node::Kind::VarDecl)
            exec(n->a, env);
        else
            eval(n->a, env);
    }
    // Each iteration has its own copy of the let bindings, so closures see the value of their iteration.
    const bool perIteration = n->a && n->a->kind == Node::Kind::VarDecl && n->a->op2 != 0 && env->parent;
    const auto copy = [this](Env *from) {
        Env *e = rt.heap.make<Env>(from->parent);
        e->vars = from->vars;
        return e;
    };
    Env *iter = perIteration ? copy(env) : env;
    Completion result;
    while (true) {
        if (n->b && !rt.toBoolean(eval(n->b, iter)))
            break;
        bool stop = false;
        Completion c = loopBody(n->d, iter, labels, &stop);
        if (c.hasValue) {
            result.value = c.value;
            result.hasValue = true;
        }
        if (c.type != Completion::Normal)
            return c;
        if (stop)
            break;
        if (perIteration)
            iter = copy(iter);
        if (n->c)
            eval(n->c, iter);
    }
    return result;
}

Completion Interp::forInLoop(const Node *n, Env *env, const QStringList &labels) {
    const Value collection = eval(n->b, env);
    Values items;
    if (n->flag) {
        spreadInto(collection, &items);
    } else {
        if (collection.isNullish())
            return Completion();
        Object *o = rt.toObject(collection);
        QStringList seen;
        int guard = 0;
        for (Object *p = o; p && ++guard < 10000; p = p->proto)
            for (const QString &k : p->enumerableKeys(rt))
                if (!seen.contains(k)) {
                    seen.append(k);
                    items.push_back(Value::string(k));
                }
    }
    Completion result;
    for (const Value &item : items) {
        Env *iter = env;
        if (n->a->kind == Node::Kind::VarDecl) {
            const QString &name = n->a->list[0]->str;
            if (n->a->op2 == 0) {
                assignName(env, name, item);
            } else {
                iter = rt.heap.make<Env>(env);
                Env::Binding b;
                b.value = item;
                b.mutableBinding = n->a->op2 != 2;
                iter->vars.insert(name, b);
            }
        } else {
            write(reference(n->a, env), env, item);
        }
        bool stop = false;
        Completion c = loopBody(n->d, iter, labels, &stop);
        if (c.hasValue) {
            result.value = c.value;
            result.hasValue = true;
        }
        if (c.type != Completion::Normal)
            return c;
        if (stop)
            break;
    }
    return result;
}

Completion Interp::tryStatement(const Node *n, Env *env) {
    Completion result;
    bool threw = false;
    Throw pending;
    try {
        result = execBlock(n->a, env);
    } catch (const Throw &t) {
        if (n->b) {
            try {
                Env *scope = rt.heap.make<Env>(env);
                if (!n->str.isEmpty()) {
                    Env::Binding b;
                    b.value = t.value;
                    scope->vars.insert(n->str, b);
                }
                // The catch block is a block that holds the clause's block.
                result = execList(n->b->list, scope);
            } catch (const Throw &again) {
                if (!n->c)
                    throw;
                threw = true;
                pending = again;
            }
        } else {
            threw = true;
            pending = t;
        }
    }
    if (n->c) {
        Completion f = execBlock(n->c, env);
        if (f.type != Completion::Normal)
            return f;
    }
    if (threw)
        throw Throw{pending.value};
    return result;
}

Completion Interp::switchStatement(const Node *n, Env *env) {
    const Value discriminant = eval(n->a, env);
    Env *scope = env;
    if (!n->names.isEmpty()) {
        scope = rt.heap.make<Env>(env);
        declareLexical(n, {}, scope);
    }
    qsizetype start = -1, fallback = -1;
    for (std::size_t i = 0; i < n->list.size(); ++i) {
        const Node *c = n->list[i];
        if (!c->a) {
            fallback = static_cast<qsizetype>(i);
            continue;
        }
        if (rt.strictEquals(discriminant, eval(c->a, scope))) {
            start = static_cast<qsizetype>(i);
            break;
        }
    }
    if (start < 0)
        start = fallback;
    Completion result;
    if (start < 0)
        return result;
    for (std::size_t i = static_cast<std::size_t>(start); i < n->list.size(); ++i) {
        Completion c = execList(n->list[i]->list, scope);
        if (c.hasValue) {
            result.value = c.value;
            result.hasValue = true;
        }
        if (c.type == Completion::Break && c.label.isEmpty())
            return result;
        if (c.type != Completion::Normal)
            return c;
    }
    return result;
}

// ---- Expressions ----

void Interp::spreadInto(const Value &iterable, Values *out) {
    if (iterable.isString()) {
        const QString &s = iterable.asString();
        for (qsizetype i = 0; i < s.size(); ++i) {
            if (s.at(i).isHighSurrogate() && i + 1 < s.size() && s.at(i + 1).isLowSurrogate()) {
                out->push_back(Value::string(s.mid(i, 2)));
                ++i;
            } else {
                out->push_back(Value::string(QString(s.at(i))));
            }
        }
        return;
    }
    if (iterable.isObject()) {
        Object *o = iterable.asObject();
        if (o->cls == Object::Class::Array || o->cls == Object::Class::Arguments) {
            // The array may change while it is iterated; read its length each time.
            for (std::size_t i = 0; i < o->elements.size(); ++i)
                out->push_back(o->elements[i]);
            return;
        }
        if (o->cls == Object::Class::String) {
            spreadInto(o->primitive, out);
            return;
        }
        // Array-likes from C++ (lists of a QObject property) have a length.
        if (o->cls == Object::Class::Host) {
            const double length = rt.toNumber(rt.getFromObject(o, QStringLiteral("length"), iterable));
            if (length >= 0 && length < 1e7) {
                for (int i = 0; i < static_cast<int>(length); ++i)
                    out->push_back(rt.getFromObject(o, QString::number(i), iterable));
                return;
            }
        }
    }
    rt.throwTypeError(QStringLiteral("%1 is not iterable").arg(rt.typeOf(iterable)));
}

Values Interp::arguments(const std::vector<Node *> &list, Env *env) {
    Values args;
    args.reserve(list.size());
    for (const Node *a : list) {
        if (a->kind == Node::Kind::Spread)
            spreadInto(eval(a->a, env), &args);
        else
            args.push_back(eval(a, env));
    }
    return args;
}

bool Interp::chainPart(const Node *n, Env *env, Value *out) {
    if (n->kind == Node::Kind::Member)
        return member(n, env, out, nullptr);
    if (n->kind == Node::Kind::Call)
        return call(n, env, out);
    *out = eval(n, env);
    return true;
}

bool Interp::member(const Node *n, Env *env, Value *out, Value *base) {
    Value object;
    if (!chainPart(n->a, env, &object))
        return false;
    if (n->flag && object.isNullish())
        return false;
    const QString key = n->b ? propertyKey(eval(n->b, env)) : n->str;
    if (object.isNullish())
        rt.throwTypeError(
            QStringLiteral("cannot read property '%1' of %2 (%3)").arg(key, rt.toString(object), calleeName(n->a)));
    *out = rt.get(object, key);
    if (base)
        *base = object;
    return true;
}

bool Interp::call(const Node *n, Env *env, Value *out) {
    Value fn, self;
    const Node *callee = n->a;
    if (callee->kind == Node::Kind::Member) {
        if (!member(callee, env, &fn, &self))
            return false;
    } else if (callee->kind == Node::Kind::Identifier) {
        Object *holder = nullptr;
        if (!lookupName(rt, env, callee->str, &fn, &holder))
            rt.throwError(ErrorType::ReferenceError, QStringLiteral("%1 is not defined").arg(callee->str));
        if (holder && holder != rt.global)
            self = Value::object(holder);
    } else if (!chainPart(callee, env, &fn)) {
        return false;
    }
    if (n->flag && fn.isNullish())
        return false;
    const Values args = arguments(n->list, env);
    if (!rt.isCallable(fn))
        rt.throwTypeError(QStringLiteral("%1 is not a function").arg(calleeName(callee)));
    rt.currentLine = n->line;
    *out = rt.call(fn, self, args);
    return true;
}

Value Interp::makeFunction(const Node *n, Env *env, const QString &inferredName) {
    const FunctionNode *f = n->func;
    if (f->expression && !f->name.isEmpty()) {
        // A named function expression sees its own name.
        Env *self = rt.heap.make<Env>(env);
        Function *fn = makeClosure(rt, f, program, self);
        Env::Binding b;
        b.value = Value::object(fn);
        b.mutableBinding = false;
        self->vars.insert(f->name, b);
        return Value::object(fn);
    }
    Function *fn = makeClosure(rt, f, program, env);
    if (f->name.isEmpty() && !inferredName.isEmpty()) {
        fn->name = inferredName;
        fn->defineOwn(QStringLiteral("name")).value = Value::string(inferredName);
    }
    return Value::object(fn);
}

Value Interp::objectLiteral(const Node *n, Env *env) {
    Object *o = rt.newObject();
    for (const Node *p : n->list) {
        if (p->op2 == 3) {
            const Value source = eval(p->b, env);
            if (source.isNullish())
                continue;
            Object *s = rt.toObject(source);
            for (const QString &k : s->enumerableKeys(rt))
                o->setOwn(k, rt.getFromObject(s, k, source));
            continue;
        }
        const QString key = p->a ? propertyKey(eval(p->a, env)) : p->str;
        if (p->op2 == 1 || p->op2 == 2) {
            Function *fn = makeClosure(rt, p->b->func, program, env);
            fn->constructor = false;
            Property &prop = o->defineOwn(key);
            if (!prop.accessor) {
                prop.value = Value();
                prop.accessor = true;
            }
            (p->op2 == 1 ? prop.getter : prop.setter) = fn;
            continue;
        }
        const Value v = p->b->kind == Node::Kind::Function ? makeFunction(p->b, env, key) : eval(p->b, env);
        if (key == QLatin1String("__proto__") && !p->a) {
            if (v.isObject())
                o->proto = v.asObject();
            else if (v.isNull())
                o->proto = nullptr;
            continue;
        }
        o->setOwn(key, v);
    }
    return Value::object(o);
}

bool Interp::lessThan(const Value &a, const Value &b, bool leftFirst, bool *undefinedResult) {
    Value pa, pb;
    if (leftFirst) {
        pa = rt.toPrimitive(a, false);
        pb = rt.toPrimitive(b, false);
    } else {
        pb = rt.toPrimitive(b, false);
        pa = rt.toPrimitive(a, false);
    }
    *undefinedResult = false;
    if (pa.isString() && pb.isString())
        return pa.asString() < pb.asString();
    const double x = rt.toNumber(pa), y = rt.toNumber(pb);
    if (std::isnan(x) || std::isnan(y)) {
        *undefinedResult = true;
        return false;
    }
    return x < y;
}

Value Interp::binary(Op op, const Value &a, const Value &b) {
    switch (op) {
    case Op::Add: {
        if (a.isNumber() && b.isNumber())
            return Value::number(a.asNumber() + b.asNumber());
        const Value pa = rt.toPrimitive(a, false), pb = rt.toPrimitive(b, false);
        if (pa.isString() || pb.isString())
            return Value::string(rt.toString(pa) + rt.toString(pb));
        return Value::number(rt.toNumber(pa) + rt.toNumber(pb));
    }
    case Op::Sub:
        return Value::number(rt.toNumber(a) - rt.toNumber(b));
    case Op::Mul:
        return Value::number(rt.toNumber(a) * rt.toNumber(b));
    case Op::Div:
        return Value::number(rt.toNumber(a) / rt.toNumber(b));
    case Op::Mod:
        return Value::number(std::fmod(rt.toNumber(a), rt.toNumber(b)));
    case Op::Exp: {
        const double x = rt.toNumber(a), y = rt.toNumber(b);
        if (std::isnan(y) || (std::fabs(x) == 1.0 && std::isinf(y)))
            return Value::number(std::numeric_limits<double>::quiet_NaN());
        return Value::number(std::pow(x, y));
    }
    case Op::Shl:
        return Value::number(static_cast<qint32>(toUint32(rt.toNumber(a)) << (toUint32(rt.toNumber(b)) & 31)));
    case Op::Shr:
        return Value::number(toInt32(rt.toNumber(a)) >> (toUint32(rt.toNumber(b)) & 31));
    case Op::UShr:
        return Value::number(toUint32(rt.toNumber(a)) >> (toUint32(rt.toNumber(b)) & 31));
    case Op::BitAnd:
        return Value::number(toInt32(rt.toNumber(a)) & toInt32(rt.toNumber(b)));
    case Op::BitOr:
        return Value::number(toInt32(rt.toNumber(a)) | toInt32(rt.toNumber(b)));
    case Op::BitXor:
        return Value::number(toInt32(rt.toNumber(a)) ^ toInt32(rt.toNumber(b)));
    case Op::Lt: {
        bool undef = false;
        const bool r = lessThan(a, b, true, &undef);
        return Value::boolean(!undef && r);
    }
    case Op::Gt: {
        bool undef = false;
        const bool r = lessThan(b, a, false, &undef);
        return Value::boolean(!undef && r);
    }
    case Op::Le: {
        bool undef = false;
        const bool r = lessThan(b, a, false, &undef);
        return Value::boolean(!undef && !r);
    }
    case Op::Ge: {
        bool undef = false;
        const bool r = lessThan(a, b, true, &undef);
        return Value::boolean(!undef && !r);
    }
    case Op::Eq:
        return Value::boolean(rt.looseEquals(a, b));
    case Op::Ne:
        return Value::boolean(!rt.looseEquals(a, b));
    case Op::StrictEq:
        return Value::boolean(rt.strictEquals(a, b));
    case Op::StrictNe:
        return Value::boolean(!rt.strictEquals(a, b));
    case Op::In:
        if (!b.isObject())
            rt.throwTypeError(
                QStringLiteral("cannot use 'in' to search for '%1' in %2").arg(rt.toString(a), rt.toString(b)));
        return Value::boolean(rt.has(b.asObject(), propertyKey(a)));
    case Op::InstanceOf: {
        Function *f = b.asFunction();
        if (!f)
            rt.throwTypeError(QStringLiteral("the right side of 'instanceof' is not callable"));
        while (f->target)
            f = f->target;
        if (!a.isObject())
            return Value::boolean(false);
        const Value proto = rt.getFromObject(f, QStringLiteral("prototype"), Value::object(f));
        if (!proto.isObject())
            rt.throwTypeError(QStringLiteral("the prototype of the right side of 'instanceof' is not an object"));
        int guard = 0;
        for (Object *o = a.asObject()->proto; o && ++guard < 10000; o = o->proto)
            if (o == proto.asObject())
                return Value::boolean(true);
        return Value::boolean(false);
    }
    default:
        break;
    }
    return Value();
}

Value Interp::eval(const Node *n, Env *env) {
    switch (n->kind) {
    case Node::Kind::Number:
        return Value::number(n->num);
    case Node::Kind::String:
        return Value::string(n->str);
    case Node::Kind::True:
        return Value::boolean(true);
    case Node::Kind::False:
        return Value::boolean(false);
    case Node::Kind::Null:
        return Value::null();
    case Node::Kind::Template: {
        QString s = n->strs.value(0);
        for (std::size_t i = 0; i < n->list.size(); ++i) {
            s += rt.toString(eval(n->list[i], env));
            s += n->strs.value(static_cast<qsizetype>(i) + 1);
        }
        return Value::string(s);
    }
    case Node::Kind::RegExp:
        return Value::object(newRegExp(rt, n->str, n->str2));
    case Node::Kind::Identifier: {
        Value v;
        if (!lookupName(rt, env, n->str, &v)) {
            if (n->str == QLatin1String("undefined"))
                return Value();
            rt.throwError(ErrorType::ReferenceError, QStringLiteral("%1 is not defined").arg(n->str));
        }
        return v;
    }
    case Node::Kind::This:
        return thisValue(env);
    case Node::Kind::Array: {
        Values elements;
        for (const Node *e : n->list) {
            if (!e)
                elements.emplace_back();
            else if (e->kind == Node::Kind::Spread)
                spreadInto(eval(e->a, env), &elements);
            else
                elements.push_back(eval(e, env));
        }
        return Value::object(rt.newArray(elements));
    }
    case Node::Kind::ObjectLit:
        return objectLiteral(n, env);
    case Node::Kind::Function:
        return makeFunction(n, env);
    case Node::Kind::Unary: {
        switch (n->op) {
        case Op::TypeOf:
            if (n->a->kind == Node::Kind::Identifier) {
                Value v;
                if (!lookupName(rt, env, n->a->str, &v))
                    return Value::string(QStringLiteral("undefined"));
                return Value::string(rt.typeOf(v));
            }
            return Value::string(rt.typeOf(eval(n->a, env)));
        case Op::Delete: {
            if (n->a->kind != Node::Kind::Member) {
                if (n->a->kind != Node::Kind::Identifier)
                    eval(n->a, env);
                return Value::boolean(n->a->kind != Node::Kind::Identifier);
            }
            Value base;
            if (!chainPart(n->a->a, env, &base))
                return Value::boolean(true);
            const QString key = n->a->b ? propertyKey(eval(n->a->b, env)) : n->a->str;
            if (base.isNullish())
                rt.throwTypeError(QStringLiteral("cannot delete property '%1' of %2").arg(key, rt.toString(base)));
            if (!base.isObject())
                return Value::boolean(true);
            return Value::boolean(base.asObject()->deleteOwn(rt, key));
        }
        case Op::Void:
            eval(n->a, env);
            return Value();
        case Op::Not:
            return Value::boolean(!rt.toBoolean(eval(n->a, env)));
        case Op::Minus:
            return Value::number(-rt.toNumber(eval(n->a, env)));
        case Op::Plus:
            return Value::number(rt.toNumber(eval(n->a, env)));
        case Op::BitNot:
            return Value::number(~toInt32(rt.toNumber(eval(n->a, env))));
        default:
            break;
        }
        break;
    }
    case Node::Kind::Update: {
        const Ref r = reference(n->a, env);
        const double old = rt.toNumber(read(r, env));
        const double now = n->op == Op::Inc ? old + 1 : old - 1;
        write(r, env, Value::number(now));
        return Value::number(n->flag ? now : old);
    }
    case Node::Kind::Binary:
        if (n->op == Op::Add && n->a->kind == Node::Kind::String && n->b->kind == Node::Kind::String)
            return Value::string(n->a->str + n->b->str);
        {
            const Value a = eval(n->a, env);
            const Value b = eval(n->b, env);
            return binary(n->op, a, b);
        }
    case Node::Kind::Logical: {
        const Value a = eval(n->a, env);
        if (n->op == Op::And)
            return rt.toBoolean(a) ? eval(n->b, env) : a;
        if (n->op == Op::Or)
            return rt.toBoolean(a) ? a : eval(n->b, env);
        return a.isNullish() ? eval(n->b, env) : a;
    }
    case Node::Kind::Assign: {
        if (n->op == Op::None) {
            if (n->a->kind == Node::Kind::Identifier) {
                const Value v =
                    n->b->kind == Node::Kind::Function ? makeFunction(n->b, env, n->a->str) : eval(n->b, env);
                assignName(env, n->a->str, v);
                return v;
            }
            const Ref r = reference(n->a, env);
            const Value v = eval(n->b, env);
            write(r, env, v);
            return v;
        }
        const Ref r = reference(n->a, env);
        Value old = read(r, env);
        Value v;
        if (n->op == Op::And) {
            if (!rt.toBoolean(old))
                return old;
            v = eval(n->b, env);
        } else if (n->op == Op::Or) {
            if (rt.toBoolean(old))
                return old;
            v = eval(n->b, env);
        } else if (n->op == Op::Nullish) {
            if (!old.isNullish())
                return old;
            v = eval(n->b, env);
        } else {
            v = binary(n->op, old, eval(n->b, env));
        }
        write(r, env, v);
        return v;
    }
    case Node::Kind::Conditional:
        return rt.toBoolean(eval(n->a, env)) ? eval(n->b, env) : eval(n->c, env);
    case Node::Kind::Member:
    case Node::Kind::Call: {
        Value v;
        if (!chainPart(n, env, &v))
            return Value();
        return v;
    }
    case Node::Kind::New: {
        const Value callee = eval(n->a, env);
        const Values args = arguments(n->list, env);
        Function *f = callee.asFunction();
        if (!f || !f->constructor)
            rt.throwTypeError(QStringLiteral("%1 is not a constructor").arg(calleeName(n->a)));
        rt.currentLine = n->line;
        return rt.construct(callee, args);
    }
    case Node::Kind::Sequence: {
        Value v;
        for (const Node *e : n->list)
            v = eval(e, env);
        return v;
    }
    default:
        break;
    }
    rt.throwError(ErrorType::SyntaxError, QStringLiteral("unexpected expression"));
}

// Keeps the name of the running function on the stack of the runtime, for error messages.
struct Frame {
    Frame(Runtime &r, const QString &name, const QString &file, int line) : rt(r), savedFile(r.currentFile) {
        if (rt.stack.size() < 64)
            rt.stack.prepend(
                QStringLiteral("%1@%2:%3")
                    .arg(name.isEmpty() ? QStringLiteral("<anonymous>") : name, file, QString::number(line)));
        pushed = rt.stack.size() <= 64;
        rt.currentFile = file;
    }
    ~Frame() {
        if (pushed && !rt.stack.isEmpty())
            rt.stack.removeFirst();
        rt.currentFile = savedFile;
    }
    Frame(const Frame &) = delete;
    Frame &operator=(const Frame &) = delete;
    Runtime &rt;
    QString savedFile;
    bool pushed = false;
};

} // namespace

Function *makeClosure(Runtime &rt, const FunctionNode *node, const std::shared_ptr<const ProgramData> &program,
                      Env *scope) {
    Function *f = rt.heap.make<Function>(rt.functionProto);
    f->node = node;
    f->program = program;
    f->scope = scope;
    f->name = node->name;
    f->constructor = !node->arrow;
    int length = 0;
    for (const FunctionNode::Param &p : node->params) {
        if (p.rest || p.defaultValue)
            break;
        ++length;
    }
    Property &l = f->defineOwn(QStringLiteral("length"));
    l.value = Value::number(length);
    l.enumerable = l.writable = false;
    Property &n = f->defineOwn(QStringLiteral("name"));
    n.value = Value::string(node->name);
    n.enumerable = n.writable = false;
    if (f->constructor) {
        Object *proto = rt.newObject();
        proto->setOwn(QStringLiteral("constructor"), Value::object(f), false);
        Property &p = f->defineOwn(QStringLiteral("prototype"));
        p.value = Value::object(proto);
        p.enumerable = false;
    }
    return f;
}

Value invokeScript(Runtime &rt, Function *f, const Value &thisValue, const Values &args, Object *newTarget) {
    (void)newTarget;
    const FunctionNode *fn = f->node;
    DepthGuard guard(rt);
    Frame frame(rt, f->name, f->program->fileName, fn->line);
    Env *env = rt.heap.make<Env>(f->scope);
    if (!fn->arrow) {
        env->hasThis = true;
        env->thisValue = thisValue.isNullish() ? Value::object(rt.global) : thisValue;
        env->callee = f;
        if (fn->usesArguments) {
            Object *a = rt.heap.make<Object>(rt.objectProto, Object::Class::Arguments);
            a->elements = args;
            Env::Binding b;
            b.value = Value::object(a);
            env->vars.insert(QStringLiteral("arguments"), b);
        }
    }
    Interp interp(rt, f->program);
    for (std::size_t i = 0; i < fn->params.size(); ++i) {
        const FunctionNode::Param &p = fn->params[i];
        Value v;
        if (p.rest) {
            Values rest;
            for (std::size_t k = i; k < args.size(); ++k)
                rest.push_back(args[k]);
            v = Value::object(rt.newArray(rest));
        } else {
            if (i < args.size())
                v = args[i];
            if (v.isUndefined() && p.defaultValue)
                v = interp.eval(p.defaultValue, env);
        }
        Env::Binding b;
        b.value = v;
        env->vars.insert(p.name, b);
    }
    for (const QString &name : fn->varNames)
        if (!env->vars.contains(name))
            env->vars.insert(name, Env::Binding());
    for (const Node *d : fn->functionDecls) {
        Env::Binding b;
        b.value = Value::object(makeClosure(rt, d->func, f->program, env));
        env->vars.insert(d->func->name, b);
    }
    if (fn->exprBody)
        return interp.eval(fn->exprBody, env);
    const Completion c = interp.execFunctionBody(fn->body, env);
    return c.type == Completion::Return ? c.value : Value();
}

Value runProgram(Runtime &rt, const std::shared_ptr<const ProgramData> &program, Env *scope, const Value &thisValue) {
    const FunctionNode *top = program->program;
    DepthGuard guard(rt);
    Frame frame(rt, QStringLiteral("%entry"), program->fileName, top->line);
    Interp interp(rt, program);
    if (top->exprBody) {
        Env *env = scope;
        if (!thisValue.isUndefined()) {
            env = rt.heap.make<Env>(scope);
            env->hasThis = true;
            env->thisValue = thisValue;
        }
        return interp.eval(top->exprBody, env);
    }
    for (const QString &name : top->varNames) {
        if (scope->object) {
            if (!rt.has(scope->object, name))
                scope->object->setOwn(name, Value());
        } else if (!scope->vars.contains(name)) {
            scope->vars.insert(name, Env::Binding());
        }
    }
    for (const Node *d : top->functionDecls) {
        const Value fn = Value::object(makeClosure(rt, d->func, program, scope));
        if (scope->object)
            rt.putToObject(scope->object, d->func->name, fn, Value::object(scope->object));
        else
            scope->vars.insert(d->func->name, Env::Binding{fn, true, true});
    }
    const Node *body = top->body;
    for (const QString &name : body->names)
        if (scope->vars.contains(name) && scope->vars.value(name).initialized)
            rt.throwError(ErrorType::SyntaxError, QStringLiteral("'%1' has already been declared").arg(name));
    interp.declareLexical(body, body->list, scope);
    Env *env = scope;
    if (!thisValue.isUndefined()) {
        env = rt.heap.make<Env>(scope);
        env->hasThis = true;
        env->thisValue = thisValue;
    }
    const Completion c = interp.execList(body->list, env);
    return c.value;
}

} // namespace QtJSPrivate
