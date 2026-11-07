// MIT License - see LICENSE
#include "meta_classes.h"

#include <QtCore>

#include <atomic>
#include <cstdio>
#include <thread>
#include <string>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

using meta::Receiver;
using meta::Shape;
using meta::Square;

static QStringList signatures(const QMetaObject *mo, QMetaMethod::MethodType type, bool local = true) {
    QStringList list;
    for (int i = local ? mo->methodOffset() : 0; i < mo->methodCount(); ++i) {
        const QMetaMethod m = mo->method(i);
        if (m.methodType() == type)
            list.append(QString::fromLatin1(m.methodSignature()));
    }
    return list;
}

// Classes in an unnamed namespace and unusual declarations. ntmoc processes this file too; the
// generated code is included at the end.
namespace {

struct Marker {
    int marked = 0;
};

class AbstractBase : public QObject {
    Q_OBJECT

public slots:
    virtual int compute(int x) = 0;
    virtual void notifyAll() { log += "base;"; }

public:
    QString log;
};

class Concrete : public AbstractBase, public Marker {
    Q_OBJECT
    // Q_PROPERTY(int commented READ nothing)
    /* signals:
         void fake(); */
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(QMap<QString, QList<int>> table READ table WRITE setTable)
    Q_PROPERTY(double multi READ multi WRITE setMulti)
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
    Q_PROPERTY(int revised READ value REVISION(3))
    Q_PROPERTY(int constant MEMBER m_constant CONSTANT)
    Q_PROPERTY(int required MEMBER m_required REQUIRED)
    Q_PROPERTY(bool stored READ flag STORED stored())
    Q_CLASSINFO("text", "a \"quoted\" value, with (parentheses)")
    Q_CLASSINFO("empty", "")

public:
    enum Mode { A, B = 1 << 3, C = A | B, D = 10 };
    Q_ENUM(Mode)
    enum class Strict : unsigned char { Off, On };
    Q_ENUM(Strict)
    enum Opt { X = 1, Y = 2 };
    Q_DECLARE_FLAGS(Opts, Opt)
    Q_FLAG(Opts)

    Concrete() = default;
    Q_DISABLE_COPY(Concrete)

    int compute(int x) override { return x * 3; }
    void notifyAll() override { log += "concrete;"; }
    int value() const { return m_value; }
    void setValue(int v) {
        m_value = v;
        emit valueChanged(v);
    }
    QMap<QString, QList<int>> table() const { return m_table; }
    void setTable(const QMap<QString, QList<int>> &t) { m_table = t; }
    double multi() const { return m_multi; }
    void setMulti(double d) { m_multi = d; }
    Mode mode() const { return m_mode; }
    void setMode(Mode m) { m_mode = m; }
    bool flag() const { return true; }
    bool stored() const { return false; }

    int m_constant = 5;
    int m_required = 6;

public slots:
    QList<int> make(int n, const QString &sep = QString(","), int base = std::max(1, 2)) const noexcept {
        Q_UNUSED(sep)
        return QList<int>{n, base};
    }
    std::pair<int, int> twoInts() { return {1, 2}; }
    static int staticSlot(int x) { return x + 1; }
    const QString &refResult() const { return m_text; }
    void withFunctionPointer(void (*callback)(int), int x) {
        callback(x);
        log += "callback;";
    }

public:
    Q_INVOKABLE void marker() { log += "marker;"; }
    Q_INVOKABLE Q_REVISION(2) int revised() { return 22; }
    Q_INVOKABLE int defaults(int a = std::max(1, 2), const QString &b = "x,y") {
        return a + static_cast<int>(b.size());
    }
    Q_SIGNAL void inlineSignal(int v);
    Q_SLOT void inlineSlot() { log += "inline;"; }
    void notASlot() { log += "not;"; }
    int notInvokable(int x) { return x; }

signals:
    void valueChanged(int value);
    void sig(const QMap<int, QString> &m, const QList<QList<int>> &nested = {});

public:
    class Inner : public QObject {
        Q_OBJECT
    public slots:
        int innerSlot() { return 8; }
    };

private:
    class Hidden;
    int m_value = 0;
    QMap<QString, QList<int>> m_table;
    double m_multi = 0;
    Mode m_mode = A;
    QString m_text = QStringLiteral("text");
    const char *m_fake = "Q_PROPERTY(int fake READ x) signals: void fake();";
};

#if 0
class Disabled : public QObject {
    Q_OBJECT
};
#endif

// ntmoc evaluates the tests of the macros it is given with -D (the target defines NT_META_FEATURE).
class Conditional : public QObject {
    Q_OBJECT
public:
#ifndef NT_META_FEATURE
    int missing() const { return 0; }
signals:
    void notBuilt();
#else
signals:
    void featured();
#endif
#if !defined(NT_META_FEATURE)
public slots:
    void alsoNotBuilt() {}
#endif
public slots:
    void always() {}
};

struct StructObject : QObject {
    Q_OBJECT
    Q_PROPERTY(int number MEMBER number)
signals:
    void changed();

public slots:
    void bump() { ++number; }

public:
    int number = 0;
};

namespace outer {
namespace inner {
class Deep : public QObject {
    Q_OBJECT
public slots:
    void ping() {}
};
} // namespace inner
} // namespace outer

} // namespace

// Class information

static void testClassInfo() {
    const QMetaObject *mo = &Shape::staticMetaObject;
    CHECK(std::string(mo->className()) == "meta::Shape");
    CHECK(mo->superClass() == &QObject::staticMetaObject);
    CHECK(QObject::staticMetaObject.superClass() == nullptr);
    CHECK(std::string(QObject::staticMetaObject.className()) == "QObject");
    CHECK(mo->inherits(&QObject::staticMetaObject));
    CHECK(mo->inherits(mo));
    CHECK(!QObject::staticMetaObject.inherits(mo));
    CHECK(Square::staticMetaObject.inherits(mo));
    CHECK(Square::staticMetaObject.superClass() == mo);

    CHECK(mo->classInfoCount() == 2);
    CHECK(mo->classInfoOffset() == 0);
    CHECK(mo->indexOfClassInfo("Version") == 1);
    CHECK(mo->indexOfClassInfo("Nope") == -1);
    CHECK(std::string(mo->classInfo(0).name()) == "Author");
    CHECK(std::string(mo->classInfo(0).value()) == "Nt");
    CHECK(std::string(mo->classInfo(1).value()) == "1.0");
    CHECK(mo->classInfo(0).enclosingMetaObject() == mo);
    CHECK(!mo->classInfo(5).isValid());
    CHECK(!mo->classInfo(-1).isValid());
    CHECK(mo->classInfo(5).name() == nullptr);
    CHECK(Square::staticMetaObject.classInfoCount() == 2);
    CHECK(Square::staticMetaObject.classInfoOffset() == 2);
    CHECK(Square::staticMetaObject.indexOfClassInfo("Author") == 0);

    CHECK(mo->constructorCount() == 4); // both have a default argument
    CHECK(Square::staticMetaObject.constructorCount() == 0);
}

// Methods

static void testMethods() {
    const QMetaObject *mo = &Shape::staticMetaObject;
    // Signals come first, then slots, then methods; a method with default arguments is followed by its
    // shorter forms.
    CHECK((signatures(mo, QMetaMethod::Signal) ==
           QStringList{"sidesChanged(int)", "titleChanged(QString)", "counterChanged()", "multi(int,QString,double)",
                       "multi(int,QString)", "withPrivate(int)", "unnamed(int,std::string)"}));
    CHECK((signatures(mo, QMetaMethod::Slot) ==
           QStringList{"describe(int)", "describe()", "add(int,int)", "setLabel(QString)", "ping()", "ping(int)",
                       "byRef(QString&)", "many(int,int,int,int,int,int,int,int,int,int)", "protectedSlot()",
                       "secret()"}));
    CHECK((signatures(mo, QMetaMethod::Method) == QStringList{"twice(int)", "kindName(Kind)", "fireMulti()",
                                                              "firePrivate()", "fireNameless()", "bumpCounter()"}));

    CHECK(mo->methodOffset() == QObject::staticMetaObject.methodCount());
    CHECK(QObject::staticMetaObject.methodOffset() == 0);
    CHECK((signatures(&QObject::staticMetaObject, QMetaMethod::Signal) ==
           QStringList{"destroyed(QObject*)", "destroyed()", "objectNameChanged(QString)"}));
    CHECK((signatures(&QObject::staticMetaObject, QMetaMethod::Slot) == QStringList{"deleteLater()"}));

    const int multi = mo->indexOfSignal("multi(int,QString,double)");
    CHECK(multi >= mo->methodOffset());
    CHECK(mo->indexOfSignal("multi( int , const QString & , double )") == multi);
    CHECK(mo->indexOfSignal("multi(int,QString)") == multi + 1);
    CHECK(mo->indexOfMethod("multi(int,QString)") == multi + 1);
    CHECK(mo->indexOfSlot("multi(int,QString)") == -1);
    CHECK(mo->indexOfSignal("add(int,int)") == -1);
    CHECK(mo->indexOfSlot("add(int,int)") > multi);
    CHECK(mo->indexOfMethod("twice(int)") > mo->indexOfSlot("secret()"));
    CHECK(mo->indexOfMethod("nope()") == -1);
    CHECK(mo->indexOfMethod(nullptr) == -1);
    CHECK(mo->indexOfSlot("deleteLater()") == QObject::staticMetaObject.indexOfSlot("deleteLater()"));
    CHECK(Square::staticMetaObject.indexOfSlot("add(int,int)") == mo->indexOfSlot("add(int,int)"));
    CHECK(Square::staticMetaObject.methodOffset() == mo->methodCount());

    const QMetaMethod m = mo->method(multi);
    CHECK(m.isValid());
    CHECK(m.methodType() == QMetaMethod::Signal);
    CHECK(m.access() == QMetaMethod::Public);
    CHECK(m.name() == "multi");
    CHECK(m.parameterCount() == 3);
    CHECK((m.parameterTypes() == QList<QByteArray>{"int", "QString", "double"}));
    CHECK((m.parameterNames() == QList<QByteArray>{"a", "b", "c"}));
    CHECK(m.parameterType(0) == QMetaType::Int);
    CHECK(m.parameterType(1) == QMetaType::QString);
    CHECK(m.parameterMetaType(2) == QMetaType::fromType<double>());
    CHECK(m.parameterType(3) == QMetaType::UnknownType);
    CHECK(m.parameterType(-1) == QMetaType::UnknownType);
    CHECK(std::string(m.typeName()) == "void");
    CHECK(m.returnType() == QMetaType::Void);
    CHECK(m.methodIndex() == multi);
    CHECK(m.relativeMethodIndex() == multi - mo->methodOffset());
    CHECK(m.enclosingMetaObject() == mo);
    int types[3] = {};
    m.getParameterTypes(types);
    CHECK(types[0] == QMetaType::Int && types[1] == QMetaType::QString && types[2] == QMetaType::Double);
    CHECK(mo->method(multi + 1).parameterCount() == 2);
    CHECK(mo->method(multi) != mo->method(multi + 1));
    CHECK(mo->method(multi) == mo->method(multi));

    // The private-signal marker is not part of the signature.
    const QMetaMethod priv = mo->method(mo->indexOfSignal("withPrivate(int)"));
    CHECK(priv.isValid() && priv.parameterCount() == 1);
    // A parameter without a name has an empty name.
    const QMetaMethod unnamed = mo->method(mo->indexOfSignal("unnamed(int,std::string)"));
    CHECK((unnamed.parameterNames() == QList<QByteArray>{"", ""}));

    const QMetaMethod add = mo->method(mo->indexOfSlot("add(int,int)"));
    CHECK(add.methodType() == QMetaMethod::Slot);
    CHECK(std::string(add.typeName()) == "int");
    CHECK(add.returnType() == QMetaType::Int);
    CHECK(add.returnMetaType() == QMetaType::fromType<int>());
    const QMetaMethod secret = mo->method(mo->indexOfSlot("secret()"));
    CHECK(secret.access() == QMetaMethod::Private);
    CHECK(mo->method(mo->indexOfSlot("protectedSlot()")).access() == QMetaMethod::Protected);
    CHECK(mo->method(mo->indexOfSlot("add(int,int)")).access() == QMetaMethod::Public);
    CHECK(mo->method(mo->indexOfMethod("twice(int)")).methodType() == QMetaMethod::Method);
    CHECK(std::string(mo->method(mo->indexOfSlot("describe()")).typeName()) == "QString");
    CHECK(mo->method(mo->indexOfSlot("byRef(QString&)")).parameterType(0) == QMetaType::QString);
    // Enumerations are registered with their qualified names.
    const QMetaMethod kindName = mo->method(mo->indexOfMethod("kindName(Kind)"));
    CHECK(kindName.parameterType(0) == QMetaType::fromType<Shape::Kind>().id());

    CHECK(!QMetaMethod().isValid());
    CHECK(QMetaMethod().methodSignature().isEmpty());
    CHECK(QMetaMethod().parameterCount() == 0);
    CHECK(!mo->method(-1).isValid());
    CHECK(!mo->method(mo->methodCount()).isValid());

    // fromSignal
    const QMetaMethod fs = QMetaMethod::fromSignal(&Shape::sidesChanged);
    CHECK(fs.isValid() && fs.methodIndex() == mo->indexOfSignal("sidesChanged(int)"));
    CHECK(QMetaMethod::fromSignal(&Shape::titleChanged).name() == "titleChanged");
    CHECK(QMetaMethod::fromSignal(&QObject::destroyed).methodSignature() == "destroyed(QObject*)");
    CHECK(QMetaMethod::fromSignal(&QObject::objectNameChanged).methodIndex() == 2);
    const QMetaMethod fsPriv = QMetaMethod::fromSignal(&Shape::withPrivate);
    CHECK(fsPriv.isValid() && fsPriv.methodSignature() == "withPrivate(int)");
    CHECK(QMetaMethod::fromSignal(&Square::sidesChanged) == fs);

    // Constructors
    CHECK(mo->indexOfConstructor("Shape(QObject*)") == 0);
    CHECK(mo->indexOfConstructor("Shape()") == 1);
    CHECK(mo->indexOfConstructor("Shape(int,QString,QObject*)") == 2);
    CHECK(mo->indexOfConstructor("Shape(int,QString)") == 3);
    CHECK(mo->indexOfConstructor("Shape(int)") == -1);
    const QMetaMethod ctor = mo->constructor(2);
    CHECK(ctor.methodType() == QMetaMethod::Constructor);
    CHECK(ctor.parameterCount() == 3);
    CHECK(std::string(ctor.typeName()).empty());
    CHECK(ctor.methodIndex() == 2);
    CHECK(!mo->constructor(4).isValid());
}

// The meta-object of the dynamic type

static void testDynamicMetaObject() {
    Square sq;
    Shape *asShape = &sq;
    QObject *asObject = &sq;
    CHECK(asShape->metaObject() == &Square::staticMetaObject);
    CHECK(asObject->metaObject() == &Square::staticMetaObject);
    CHECK(QObject().metaObject() == &QObject::staticMetaObject);
    CHECK(asObject->inherits("meta::Square"));
    CHECK(asObject->inherits("meta::Shape"));
    CHECK(asObject->inherits("QObject"));
    CHECK(!asObject->inherits("QTimer"));
    CHECK(!asObject->inherits(nullptr));
    QTimer timer;
    CHECK(timer.metaObject() == &QTimer::staticMetaObject);
    CHECK(timer.inherits("QObject"));
    CHECK(std::string(timer.metaObject()->className()) == "QTimer");
    QFile file;
    CHECK(file.metaObject() == &QFile::staticMetaObject);
    CHECK(QFile::staticMetaObject.superClass() == &QFileDevice::staticMetaObject);
    CHECK(QFileDevice::staticMetaObject.superClass() == &QIODevice::staticMetaObject);
    CHECK(file.inherits("QIODevice"));
    CHECK(Shape::staticMetaObject.cast(&sq) == &sq);
    CHECK(Square::staticMetaObject.cast(static_cast<QObject *>(&timer)) == nullptr);
    const QObject *constObject = &sq;
    CHECK(Shape::staticMetaObject.cast(constObject) == constObject);
}

// Properties

static void testAttributes() {
    const QMetaObject *mo = &meta::Tagged::staticMetaObject;
    auto method = [mo](const char *sig) { return mo->method(mo->indexOfMethod(sig)); };
    CHECK(method("scripted()").attributes() == QMetaMethod::Scriptable);
    CHECK(std::strcmp(method("scripted()").tag(), "") == 0);
    CHECK(method("tagged()").attributes() == 0);
    CHECK(std::strcmp(method("tagged()").tag(), "MY_TAG") == 0);
    CHECK(std::strcmp(method("tagged()").typeName(), "void") == 0);
    CHECK(method("both()").attributes() == QMetaMethod::Scriptable);
    CHECK(std::strcmp(method("both()").tag(), "MY_TAG") == 0);
    CHECK(std::strcmp(method("both()").typeName(), "QString") == 0);
    CHECK(method("plain(int)").attributes() == 0);
    CHECK(method("plain()").attributes() == QMetaMethod::Cloned);
    CHECK(method("scriptedSignal()").attributes() == QMetaMethod::Scriptable);
    // Q_SCRIPTABLE makes a meta-method of a public function, like Q_INVOKABLE.
    CHECK(method("invokable()").methodType() == QMetaMethod::Method);
    CHECK(method("invokable()").attributes() == QMetaMethod::Scriptable);
    CHECK(QMetaMethod().attributes() == 0);
}

static void testPropertyDescriptions() {
    const QMetaObject *mo = &Shape::staticMetaObject;
    CHECK(mo->propertyOffset() == 1); // objectName
    CHECK(mo->propertyCount() == 12);
    CHECK(Square::staticMetaObject.propertyOffset() == 12);
    CHECK(Square::staticMetaObject.propertyCount() == 13);
    CHECK(QObject::staticMetaObject.propertyCount() == 1);

    const QMetaProperty objectName = QObject::staticMetaObject.property(0);
    CHECK(std::string(objectName.name()) == "objectName");
    CHECK(objectName.isReadable() && objectName.isWritable() && objectName.hasNotifySignal());
    CHECK(objectName.notifySignal().methodSignature() == "objectNameChanged(QString)");

    const QMetaProperty sides = mo->property(mo->indexOfProperty("sides"));
    CHECK(sides.isValid());
    CHECK(std::string(sides.typeName()) == "int");
    CHECK(sides.typeId() == QMetaType::Int && sides.userType() == QMetaType::Int);
    CHECK(sides.metaType() == QMetaType::fromType<int>());
    CHECK(sides.isReadable() && sides.isWritable() && !sides.isResettable());
    CHECK(sides.isDesignable() && sides.isScriptable() && sides.isStored() && !sides.isUser());
    CHECK(!sides.isConstant() && !sides.isFinal() && !sides.isRequired() && !sides.isBindable());
    CHECK(sides.hasNotifySignal());
    CHECK(sides.notifySignal().methodSignature() == "sidesChanged(int)");
    CHECK(sides.notifySignalIndex() == mo->indexOfSignal("sidesChanged(int)"));
    CHECK(sides.propertyIndex() == 1 && sides.relativePropertyIndex() == 0);
    CHECK(sides.enclosingMetaObject() == mo);
    CHECK(!sides.isEnumType() && !sides.isFlagType() && !sides.enumerator().isValid());
    CHECK(sides.revision() == 0);

    const QMetaProperty title = mo->property(mo->indexOfProperty("title"));
    CHECK(title.isResettable());

    const QMetaProperty area = mo->property(mo->indexOfProperty("area"));
    CHECK(area.isReadable() && !area.isWritable() && !area.isStored() && !area.isScriptable() && area.isDesignable());
    CHECK(!area.hasNotifySignal() && area.notifySignalIndex() == -1 && !area.notifySignal().isValid());

    const QMetaProperty fixed = mo->property(mo->indexOfProperty("fixed"));
    CHECK(fixed.isConstant() && fixed.isFinal() && !fixed.isWritable());
    CHECK(fixed.revision() == ((1 << 8) | 2));

    const QMetaProperty kind = mo->property(mo->indexOfProperty("kind"));
    CHECK(kind.isEnumType() && !kind.isFlagType());
    CHECK(std::string(kind.enumerator().name()) == "Kind");
    CHECK(kind.typeId() == QMetaType::fromType<Shape::Kind>().id());
    const QMetaProperty features = mo->property(mo->indexOfProperty("features"));
    CHECK(features.isEnumType() && features.isFlagType());
    CHECK(std::string(features.enumerator().name()) == "Features");

    const QMetaProperty special = mo->property(mo->indexOfProperty("special"));
    CHECK(special.isUser());
    Shape shape;
    CHECK(special.isDesignable(&shape));
    shape.m_designable = false;
    CHECK(!special.isDesignable(&shape));
    CHECK(special.isDesignable(nullptr)); // no object to ask
    CHECK(mo->userProperty().isValid() && std::string(mo->userProperty().name()) == "special");
    CHECK(!QObject::staticMetaObject.userProperty().isValid());

    const QMetaProperty member = mo->property(mo->indexOfProperty("counter"));
    CHECK(member.isReadable() && member.isWritable());
    CHECK(member.notifySignal().methodSignature() == "counterChanged()");

    CHECK(mo->indexOfProperty("nope") == -1);
    CHECK(mo->indexOfProperty(nullptr) == -1);
    CHECK(Square::staticMetaObject.indexOfProperty("sides") == 1);
    CHECK(Square::staticMetaObject.indexOfProperty("side") == 12);
    CHECK(!mo->property(100).isValid());
    CHECK(!mo->property(-1).isValid());
    CHECK(!QMetaProperty().isValid());
    CHECK(QMetaProperty().name() == nullptr);
    CHECK(QMetaProperty().read(&shape).isNull());
    CHECK(!QMetaProperty().write(&shape, 1));
}

static void testPropertyAccess() {
    Square sq;
    const QMetaObject *mo = sq.metaObject();
    const auto prop = [&](const char *name) { return mo->property(mo->indexOfProperty(name)); };

    CHECK(prop("sides").read(&sq).toInt() == 4);
    CHECK(prop("sides").write(&sq, 7));
    CHECK(sq.sides() == 7);
    CHECK(prop("sides").write(&sq, QStringLiteral("9"))); // converted
    CHECK(sq.sides() == 9);
    CHECK(prop("sides").write(&sq, 2.0));
    CHECK(sq.sides() == 2);
    CHECK(!prop("sides").write(&sq, QStringLiteral("nine")));
    CHECK(sq.sides() == 2);
    CHECK(!prop("sides").write(&sq, QVariant())); // not resettable
    CHECK(!prop("area").write(&sq, 3.0));
    CHECK(prop("area").read(&sq).toDouble() == 1.5);
    CHECK(prop("area").read(&sq).typeId() == QMetaType::Double);

    CHECK(prop("title").read(&sq).toString() == "square");
    CHECK(prop("title").reset(&sq));
    CHECK(sq.title() == "untitled");
    CHECK(prop("title").write(&sq, QStringLiteral("x")));
    CHECK(prop("title").write(&sq, QVariant())); // an invalid value resets
    CHECK(sq.title() == "untitled");
    CHECK(!prop("sides").reset(&sq));

    CHECK(prop("side").read(&sq).toDouble() == 2.0);
    CHECK(prop("side").write(&sq, 4));
    CHECK(sq.side() == 4.0);

    // Enumerations by value and by key.
    CHECK(prop("kind").read(&sq).typeId() == QMetaType::fromType<Shape::Kind>().id());
    CHECK(prop("kind").read(&sq).toInt() == Shape::Triangle);
    CHECK(prop("kind").read(&sq).toString() == "Triangle");
    CHECK(prop("kind").write(&sq, 100));
    CHECK(sq.kind() == Shape::Circle);
    CHECK(prop("kind").write(&sq, QStringLiteral("Square")));
    CHECK(sq.kind() == Shape::Square);
    CHECK(prop("kind").write(&sq, QVariant::fromValue(Shape::Triangle)));
    CHECK(sq.kind() == Shape::Triangle);
    CHECK(!prop("kind").write(&sq, QStringLiteral("Hexagon")));
    CHECK(sq.kind() == Shape::Triangle);
    // Flags by value and by keys.
    CHECK(prop("features").write(&sq, QStringLiteral("Filled|Shadowed")));
    CHECK(sq.features() == (Shape::Filled | Shape::Shadowed));
    CHECK(prop("features").read(&sq).toString() == "Filled|Shadowed");
    CHECK(prop("features").write(&sq, 2));
    CHECK(sq.features() == Shape::Outlined);
    CHECK(!prop("features").write(&sq, QStringLiteral("Filled|Nope")));

    // A MEMBER property emits its notify signal when the value changes.
    int notified = 0;
    QObject::connect(&sq, &Shape::counterChanged, [&] { ++notified; });
    CHECK(prop("counter").read(&sq).toInt() == 0);
    CHECK(prop("counter").write(&sq, 5));
    CHECK(sq.m_counter == 5 && notified == 1);
    CHECK(prop("counter").write(&sq, 5));
    CHECK(notified == 1);
    CHECK(prop("counter").write(&sq, 6));
    CHECK(notified == 2);

    // Container, QVariant and pointer properties.
    CHECK(prop("values").read(&sq).value<QList<int>>().isEmpty());
    CHECK(prop("values").write(&sq, QVariant::fromValue(QList<int>{1, 2, 3})));
    CHECK((sq.values() == QList<int>{1, 2, 3}));
    QVariantList asList{4, 5};
    CHECK(prop("values").write(&sq, asList)); // converted from QVariantList
    CHECK((sq.values() == QList<int>{4, 5}));
    CHECK(prop("any").write(&sq, QStringLiteral("anything")));
    CHECK(prop("any").read(&sq).toString() == "anything");
    CHECK(prop("any").write(&sq, QVariant())); // a QVariant property can hold an invalid variant
    CHECK(!prop("any").read(&sq).isValid());
    Shape other;
    CHECK(prop("partner").write(&sq, QVariant::fromValue(&other)));
    CHECK(sq.m_partner == &other);
    CHECK(prop("partner").read(&sq).value<Shape *>() == &other);
    CHECK(prop("partner").read(&sq).value<QObject *>() == &other);
    CHECK(prop("partner").write(&sq, QVariant::fromValue(static_cast<QObject *>(&other))));
    QTimer wrong;
    CHECK(!prop("partner").write(&sq, QVariant::fromValue(&wrong)));
    CHECK(prop("partner").metaType().flags().testFlag(QMetaType::PointerToQObject));
    CHECK(prop("partner").metaType().metaObject() == &Shape::staticMetaObject);

    // objectName comes from QObject.
    QSignalBlocker blocker(sq);
    CHECK(sq.setProperty("objectName", QStringLiteral("sq")));
    CHECK(sq.objectName() == "sq");
    CHECK(sq.property("objectName").toString() == "sq");
}

// QObject::property and setProperty

class DynamicWatcher : public QObject {
public:
    QList<QByteArray> seen;

protected:
    bool event(QEvent *e) override {
        if (e->type() == QEvent::DynamicPropertyChange)
            seen.append(static_cast<QDynamicPropertyChangeEvent *>(e)->propertyName());
        return QObject::event(e);
    }
};

static void testObjectProperties() {
    Shape shape;
    CHECK(shape.property("sides").toInt() == 3);
    CHECK(shape.setProperty("sides", 8));
    CHECK(shape.sides() == 8);
    CHECK(!shape.setProperty("area", 1.0)); // read-only
    CHECK(!shape.property("nope").isValid());
    CHECK(!shape.property(nullptr).isValid());
    CHECK(shape.dynamicPropertyNames().isEmpty());

    // Dynamic properties
    DynamicWatcher w;
    CHECK(!w.setProperty("color", QStringLiteral("red"))); // dynamic: returns false
    CHECK(w.property("color").toString() == "red");
    CHECK((w.dynamicPropertyNames() == QList<QByteArray>{"color"}));
    CHECK((w.seen == QList<QByteArray>{"color"}));
    CHECK(!w.setProperty("size", 3));
    CHECK(!w.setProperty("color", QStringLiteral("blue")));
    CHECK(w.property("color").toString() == "blue");
    CHECK(w.property("size").toInt() == 3);
    CHECK((w.dynamicPropertyNames() == QList<QByteArray>{"color", "size"}));
    CHECK(w.seen.size() == 3);
    CHECK(!w.setProperty("color", QVariant())); // removes it
    CHECK(!w.property("color").isValid());
    CHECK((w.dynamicPropertyNames() == QList<QByteArray>{"size"}));
    CHECK(w.seen.size() == 4 && w.seen.last() == "color");
    CHECK(!w.setProperty("never", QVariant())); // nothing to remove: no event
    CHECK(w.seen.size() == 4);
    CHECK(!w.setProperty(nullptr, 1));
    // A static property of the base class still works on a derived object.
    CHECK(w.setProperty("objectName", QStringLiteral("watcher")));
    CHECK(w.objectName() == "watcher");
    CHECK(w.seen.size() == 4);

    QDynamicPropertyChangeEvent ev("abc");
    CHECK(ev.type() == QEvent::DynamicPropertyChange);
    CHECK(ev.propertyName() == "abc");
}

// Enumerations

static void testEnums() {
    const QMetaObject *mo = &Shape::staticMetaObject;
    CHECK(mo->enumeratorCount() == 3);
    CHECK(mo->enumeratorOffset() == 0);
    const QMetaEnum kind = mo->enumerator(mo->indexOfEnumerator("Kind"));
    CHECK(kind.isValid());
    CHECK(std::string(kind.name()) == "Kind");
    CHECK(std::string(kind.enumName()) == "Kind");
    CHECK(std::string(kind.scope()) == "meta::Shape");
    CHECK(!kind.isFlag() && !kind.isScoped());
    CHECK(kind.keyCount() == 3);
    CHECK(std::string(kind.key(1)) == "Square" && kind.value(1) == 4 && kind.value(2) == 100);
    CHECK(kind.key(3) == nullptr && kind.value(3) == -1 && kind.key(-1) == nullptr);
    CHECK(kind.enclosingMetaObject() == mo);
    bool ok = false;
    CHECK(kind.keyToValue("Circle", &ok) == 100 && ok);
    CHECK(kind.keyToValue("Shape::Circle", &ok) == 100 && ok);
    CHECK(kind.keyToValue("circle", &ok) == -1 && !ok);
    CHECK(kind.keyToValue(nullptr, &ok) == -1 && !ok);
    CHECK(std::string(kind.valueToKey(4)) == "Square");
    CHECK(kind.valueToKey(5) == nullptr);

    const QMetaEnum features = QMetaEnum::fromType<Shape::Features>();
    CHECK(features.isValid() && features.isFlag());
    CHECK(std::string(features.name()) == "Features");
    CHECK(features.keyCount() == 3);
    CHECK(features.keysToValue("Filled|Shadowed", &ok) == 5 && ok);
    CHECK(features.keysToValue(" Filled | Outlined ", &ok) == 3 && ok);
    CHECK(features.keysToValue("Filled|Nope", &ok) == -1 && !ok);
    CHECK(features.keysToValue("", &ok) == -1 && !ok);
    CHECK(features.valueToKeys(5) == "Filled|Shadowed");
    CHECK(features.valueToKeys(2) == "Outlined");
    CHECK(features.valueToKeys(0).isEmpty());
    CHECK(features.valueToKeys(8).isEmpty());

    const QMetaEnum color = QMetaEnum::fromType<Shape::Color>();
    CHECK(color.isValid() && color.isScoped());
    CHECK(color.keyToValue("Green") == 7);
    CHECK(std::string(color.valueToKey(7)) == "Green");

    const QMetaEnum level = QMetaEnum::fromType<meta::Level>();
    CHECK(level.isValid() && level.isScoped());
    CHECK(std::string(level.scope()) == "meta");
    CHECK(level.keyCount() == 3 && level.value(2) == 6);
    CHECK(std::string(meta::staticMetaObject.className()) == "meta");
    CHECK(meta::staticMetaObject.enumeratorCount() == 1);
    CHECK(meta::staticMetaObject.superClass() == nullptr);

    CHECK(!QMetaEnum().isValid());
    CHECK(QMetaEnum().keyCount() == 0 && QMetaEnum().name() == nullptr);
    CHECK(!mo->enumerator(9).isValid());
    CHECK(mo->indexOfEnumerator("Nope") == -1);
    CHECK(Square::staticMetaObject.indexOfEnumerator("Kind") == 0);
    CHECK(Square::staticMetaObject.enumeratorOffset() == 3);

    // Enumerations convert through QVariant.
    const QVariant v = QVariant::fromValue(Shape::Circle);
    CHECK(v.canConvert<int>() && v.toInt() == 100);
    CHECK(v.toString() == "Circle");
    QVariant s = QStringLiteral("Square");
    CHECK(s.convert(QMetaType::fromType<Shape::Kind>()));
    CHECK(s.value<Shape::Kind>() == Shape::Square);
    CHECK(QVariant(4).value<Shape::Kind>() == Shape::Square);
    CHECK(QVariant(QStringLiteral("Circle")).value<Shape::Kind>() == Shape::Circle);
    CHECK(QVariant::fromValue(meta::Level::Mid).toString() == "Mid");
    CHECK(QVariant::fromValue(Shape::Features(Shape::Filled | Shape::Outlined)).toString() == "Filled|Outlined");
    CHECK(QMetaType::fromType<Shape::Kind>().flags().testFlag(QMetaType::IsEnumeration));
    CHECK(QMetaType::fromType<Shape::Kind>().metaObject() == mo);
    CHECK(std::string(QMetaType::fromType<Shape::Kind>().name()) == "meta::Shape::Kind");
}

// invokeMethod

static void testInvokeMethod() {
    Shape shape;
    int sum = 0;
    CHECK(QMetaObject::invokeMethod(&shape, "add", Q_RETURN_ARG(int, sum), Q_ARG(int, 3), Q_ARG(int, 4)));
    CHECK(sum == 7);
    sum = 0;
    CHECK(QMetaObject::invokeMethod(&shape, "add", Qt::DirectConnection, Q_RETURN_ARG(int, sum), Q_ARG(int, 5),
                                    Q_ARG(int, 6)));
    CHECK(sum == 11);
    QString text;
    CHECK(QMetaObject::invokeMethod(&shape, "describe", Q_RETURN_ARG(QString, text)));
    CHECK(text == "shape v1"); // the shorter form of a method with a default argument
    CHECK(QMetaObject::invokeMethod(&shape, "describe", Q_RETURN_ARG(QString, text), Q_ARG(int, 5)));
    CHECK(text == "shape v5");
    CHECK(QMetaObject::invokeMethod(&shape, "describe")); // the result is discarded
    CHECK(QMetaObject::invokeMethod(&shape, "ping"));
    CHECK(QMetaObject::invokeMethod(&shape, "ping", Q_ARG(int, 4)));
    CHECK(shape.log == "ping;ping4;");
    CHECK(QMetaObject::invokeMethod(&shape, "secret")); // private slots can be invoked by name
    CHECK(QMetaObject::invokeMethod(&shape, "protectedSlot"));
    CHECK(shape.log == "ping;ping4;secret;protected;");
    // A const reference parameter takes the plain type.
    CHECK(QMetaObject::invokeMethod(&shape, "setLabel", Q_ARG(QString, QStringLiteral("L"))));
    const QString constLabel = QStringLiteral("C");
    CHECK(QMetaObject::invokeMethod(&shape, "setLabel", Q_ARG(QString, constLabel)));
    CHECK(shape.log.endsWith("label:L;label:C;"));
    // A non-const reference argument is passed through.
    QString ref = "hi";
    CHECK(QMetaObject::invokeMethod(&shape, "byRef", Q_ARG(QString &, ref)));
    CHECK(ref == "hi!");
    // Ten arguments
    CHECK(QMetaObject::invokeMethod(&shape, "many", Q_ARG(int, 1), Q_ARG(int, 2), Q_ARG(int, 3), Q_ARG(int, 4),
                                    Q_ARG(int, 5), Q_ARG(int, 6), Q_ARG(int, 7), Q_ARG(int, 8), Q_ARG(int, 9),
                                    Q_ARG(int, 10)));
    CHECK(shape.log.endsWith("55;"));
    // Q_INVOKABLE static function and methods.
    int twice = 0;
    CHECK(QMetaObject::invokeMethod(&shape, "twice", Q_RETURN_ARG(int, twice), Q_ARG(int, 21)));
    CHECK(twice == 42);
    QString kindName;
    CHECK(QMetaObject::invokeMethod(&shape, "kindName", Q_RETURN_ARG(QString, kindName),
                                    Q_ARG(Shape::Kind, Shape::Circle)));
    CHECK(kindName == "Circle");
    // Invoked through a base pointer, the derived class's slot is found.
    Square sq;
    QObject *asObject = &sq;
    int extra = 0;
    CHECK(QMetaObject::invokeMethod(asObject, "extra", Q_RETURN_ARG(int, extra)));
    CHECK(extra == 99);
    CHECK(QMetaObject::invokeMethod(asObject, "add", Q_RETURN_ARG(int, sum), Q_ARG(int, 1), Q_ARG(int, 1)));
    CHECK(sum == 2);
    // A signal can be invoked too; it emits.
    int got = 0;
    QObject::connect(&shape, &Shape::sidesChanged, [&](int v) { got = v; });
    CHECK(QMetaObject::invokeMethod(&shape, "sidesChanged", Q_ARG(int, 12)));
    CHECK(got == 12);

    // Errors
    CHECK(!QMetaObject::invokeMethod(&shape, "nope"));
    CHECK(!QMetaObject::invokeMethod(&shape, "add", Q_ARG(int, 1))); // not enough arguments
    CHECK(!QMetaObject::invokeMethod(&shape, "add", Q_ARG(QString, QStringLiteral("a")), Q_ARG(int, 2)));
    CHECK(!QMetaObject::invokeMethod(nullptr, "add"));
    CHECK(!QMetaObject::invokeMethod(&shape, nullptr));
    double wrongReturn = 0;
    CHECK(!QMetaObject::invokeMethod(&shape, "add", Q_RETURN_ARG(double, wrongReturn), Q_ARG(int, 1), Q_ARG(int, 2)));
    int voidReturn = 0;
    CHECK(!QMetaObject::invokeMethod(&shape, "ping", Q_RETURN_ARG(int, voidReturn)));
    // A different spelling of the same type is accepted.
    CHECK(QMetaObject::invokeMethod(&shape, "add", Q_RETURN_ARG(qint32, sum), Q_ARG(qint32, 2), Q_ARG(qint32, 3)));
    CHECK(sum == 5);

    // QMetaMethod::invoke
    const QMetaMethod add = shape.metaObject()->method(shape.metaObject()->indexOfSlot("add(int,int)"));
    CHECK(add.invoke(&shape, Q_RETURN_ARG(int, sum), Q_ARG(int, 20), Q_ARG(int, 22)));
    CHECK(sum == 42);
    CHECK(add.invoke(&shape, Qt::DirectConnection, Q_ARG(int, 1), Q_ARG(int, 1)));
    CHECK(add.invoke(&shape, Q_ARG(int, 1), Q_ARG(int, 1)));
    CHECK(!add.invoke(&shape, Q_ARG(int, 1)));
    CHECK(!add.invoke(nullptr, Q_ARG(int, 1), Q_ARG(int, 2)));
    QTimer timer;
    CHECK(!add.invoke(&timer, Q_ARG(int, 1), Q_ARG(int, 2))); // not an instance of the class
    CHECK(!QMetaMethod().invoke(&shape));
    const QMetaMethod ctor = Shape::staticMetaObject.constructor(0);
    CHECK(!ctor.invoke(&shape));

    // Functors
    int called = 0;
    CHECK(QMetaObject::invokeMethod(&shape, [&] { ++called; }));
    CHECK(called == 1);
    int result = 0;
    CHECK(QMetaObject::invokeMethod(&shape, [] { return 17; }, &result));
    CHECK(result == 17);
    CHECK(QMetaObject::invokeMethod(&shape, [] { return 18; }, Qt::DirectConnection, &result));
    CHECK(result == 18);
    CHECK(!QMetaObject::invokeMethod(static_cast<QObject *>(nullptr), [] {}));
    CHECK(!QMetaObject::invokeMethod(&shape, [] { return 1; }, Qt::QueuedConnection, &result));
}

static void testQueuedInvoke() {
    Shape shape;
    CHECK(QMetaObject::invokeMethod(&shape, "setLabel", Qt::QueuedConnection, Q_ARG(QString, QStringLiteral("later"))));
    CHECK(shape.log.isEmpty());
    QCoreApplication::processEvents();
    CHECK(shape.log == "label:later;");
    CHECK(QMetaObject::invokeMethod(&shape, "ping", Qt::QueuedConnection));
    QCoreApplication::processEvents();
    CHECK(shape.log == "label:later;ping;");
    // The arguments are copies.
    {
        QString temp = QStringLiteral("copy");
        CHECK(QMetaObject::invokeMethod(&shape, "setLabel", Qt::QueuedConnection, Q_ARG(QString, temp)));
        temp = "changed";
    }
    QCoreApplication::processEvents();
    CHECK(shape.log.endsWith("label:copy;"));
    // No return values for queued calls.
    int sum = 0;
    CHECK(!QMetaObject::invokeMethod(&shape, "add", Qt::QueuedConnection, Q_RETURN_ARG(int, sum), Q_ARG(int, 1),
                                     Q_ARG(int, 2)));
    // A queued call whose object is deleted before the event arrives does nothing.
    auto *doomed = new Shape;
    QMetaObject::invokeMethod(doomed, "ping", Qt::QueuedConnection);
    delete doomed;
    QCoreApplication::processEvents();
    // Queued functor
    int ran = 0;
    CHECK(QMetaObject::invokeMethod(&shape, [&] { ++ran; }, Qt::QueuedConnection));
    CHECK(ran == 0);
    QCoreApplication::processEvents();
    CHECK(ran == 1);
}

static void testNewInstance() {
    QObject *object = Shape::staticMetaObject.newInstance(Q_ARG(QObject *, static_cast<QObject *>(nullptr)));
    CHECK(object != nullptr);
    CHECK(object->metaObject() == &Shape::staticMetaObject);
    delete object;
    QObject parent;
    object = Shape::staticMetaObject.newInstance(Q_ARG(int, 6), Q_ARG(QString, QStringLiteral("hex")),
                                                 Q_ARG(QObject *, &parent));
    auto *shape = qobject_cast<Shape *>(object);
    CHECK(shape && shape->sides() == 6 && shape->title() == "hex" && shape->parent() == &parent);
    // A shorter call, through the default argument
    object = Shape::staticMetaObject.newInstance(Q_ARG(int, 5), Q_ARG(QString, QStringLiteral("pent")));
    shape = qobject_cast<Shape *>(object);
    CHECK(shape && shape->sides() == 5 && shape->parent() == nullptr);
    delete object;
    CHECK(Shape::staticMetaObject.newInstance(Q_ARG(int, 5)) == nullptr);
    CHECK(Square::staticMetaObject.newInstance() == nullptr);
    object = Shape::staticMetaObject.constructor(0).newInstance(Q_ARG(QObject *, static_cast<QObject *>(nullptr)));
    CHECK(object != nullptr);
    delete object;
}

// String-based connections

static void testStringConnections() {
    Shape shape;
    Receiver r;
    QMetaObject::Connection c = QObject::connect(&shape, SIGNAL(sidesChanged(int)), &r, SLOT(onSides(int)));
    CHECK(c);
    shape.setSides(10);
    CHECK(r.log == "sides10;" && r.total == 10);
    CHECK(QObject::disconnect(c));
    shape.setSides(11);
    CHECK(r.total == 10);

    // The member can be written with other spacing and qualifiers.
    CHECK(QObject::connect(&shape, SIGNAL(titleChanged(QString)), &r, SLOT(onTitle(const QString &))));
    shape.setTitle("t");
    CHECK(r.log.endsWith("title:t;"));
    CHECK(QObject::connect(&shape, "2titleChanged( const QString & )", &r, "1onVoid()")); // fewer arguments
    shape.setTitle("u");
    CHECK(r.log.endsWith("title:u;void;") || r.log.endsWith("void;title:u;"));
    // A signal can be connected to another signal.
    r.log.clear();
    CHECK(QObject::connect(&shape, SIGNAL(sidesChanged(int)), &r, SIGNAL(relay(int))));
    int relayed = 0;
    QObject::connect(&r, &Receiver::relay, [&](int v) { relayed = v; });
    shape.setSides(33);
    CHECK(relayed == 33);

    // The cloned form of a signal with default arguments, and the full form.
    Receiver r2;
    CHECK(!QObject::connect(&shape, SIGNAL(multi(int, QString)), &r2,
                            SLOT(onTitle(QString)))); // the first argument is an int
    CHECK(QObject::connect(&shape, SIGNAL(multi(int, QString)), &r2, SLOT(onSides(int))));
    shape.fireMulti();
    CHECK(r2.total == 7);
    CHECK(QObject::connect(&shape, SIGNAL(multi(int, QString, double)), &r2, SLOT(onSides(int))));
    shape.fireMulti();
    CHECK(r2.total == 7 + 7 + 7);
    // A signal that ends with QPrivateSignal
    Receiver r3;
    CHECK(QObject::connect(&shape, SIGNAL(withPrivate(int)), &r3, SLOT(onSides(int))));
    shape.firePrivate();
    CHECK(r3.total == 9);
    CHECK(QObject::connect(&shape, SIGNAL(unnamed(int, std::string)), &r3, SLOT(onVoid())));
    shape.fireNameless();
    CHECK(r3.log.endsWith("void;"));

    // The receiver's connection goes away with it.
    {
        Receiver gone;
        CHECK(QObject::connect(&shape, SIGNAL(sidesChanged(int)), &gone, SLOT(onSides(int))));
    }
    shape.setSides(1);

    // Member form
    Receiver r4;
    CHECK(r4.connect(&shape, SIGNAL(sidesChanged(int)), SLOT(onSides(int))));
    shape.setSides(2);
    CHECK(r4.total == 2);

    // Failures print a warning and give an invalid connection.
    CHECK(!QObject::connect(&shape, SIGNAL(nope(int)), &r, SLOT(onSides(int))));
    CHECK(!QObject::connect(&shape, SIGNAL(sidesChanged(int)), &r, SLOT(nope(int))));
    CHECK(!QObject::connect(&shape, SIGNAL(sidesChanged(int)), &r, SLOT(onTitle(QString)))); // wrong type
    CHECK(
        !QObject::connect(&shape, SIGNAL(counterChanged()), &r, SLOT(onSides(int)))); // more arguments than the signal
    CHECK(!QObject::connect(&shape, "sidesChanged(int)", &r, SLOT(onSides(int))));    // no SIGNAL() code
    CHECK(!QObject::connect(&shape, SLOT(add(int, int)), &r, SLOT(onSides(int))));
    CHECK(!QObject::connect(&shape, SIGNAL(sidesChanged(int)), &r, "onSides(int)"));
    CHECK(!QObject::connect(nullptr, SIGNAL(sidesChanged(int)), &r, SLOT(onSides(int))));
    CHECK(!QObject::connect(&shape, nullptr, &r, SLOT(onSides(int))));
    CHECK(!QObject::connect(&shape, SIGNAL(sidesChanged(int)), nullptr, SLOT(onSides(int))));
    CHECK(!QObject::connect(&shape, SIGNAL(sidesChanged(int)), &r, SIGNAL(nope())));
    CHECK(!QObject::connect(&shape, "", &r, SLOT(onSides(int))));
    // A slot can not be used as a signal.
    CHECK(!QObject::connect(&shape, SIGNAL(ping()), &r, SLOT(onVoid())));
}

static void testDisconnect() {
    Shape shape;
    Receiver a, b;
    QObject::connect(&shape, SIGNAL(sidesChanged(int)), &a, SLOT(onSides(int)));
    QObject::connect(&shape, SIGNAL(sidesChanged(int)), &b, SLOT(onSides(int)));
    QObject::connect(&shape, SIGNAL(titleChanged(QString)), &a, SLOT(onTitle(QString)));
    CHECK(QObject::disconnect(&shape, SIGNAL(sidesChanged(int)), &a, SLOT(onSides(int))));
    CHECK(!QObject::disconnect(&shape, SIGNAL(sidesChanged(int)), &a, SLOT(onSides(int))));
    shape.setSides(5);
    CHECK(a.total == 0 && b.total == 5);
    shape.setTitle("x");
    CHECK(a.log == "title:x;");
    // Every connection to a receiver
    CHECK(shape.disconnect(&a));
    shape.setTitle("y");
    CHECK(a.log == "title:x;");
    // A signal and every receiver
    CHECK(shape.disconnect(SIGNAL(sidesChanged(int))));
    shape.setSides(6);
    CHECK(b.total == 5);
    QObject::connect(&shape, SIGNAL(sidesChanged(int)), &a, SLOT(onSides(int)));
    QObject::connect(&shape, SIGNAL(sidesChanged(int)), &b, SLOT(onSides(int)));
    CHECK(shape.disconnect(SIGNAL(sidesChanged(int)), &b, nullptr));
    shape.setSides(7);
    CHECK(a.total == 7 && b.total == 5);
    CHECK(shape.disconnect(&a, SLOT(onSides(int))));
    CHECK(!shape.disconnect(&a, SLOT(onSides(int))));
    CHECK(!shape.disconnect(SIGNAL(nope())));
    CHECK(!shape.disconnect(SLOT(sidesChanged(int))));
    CHECK(!QObject::disconnect(&shape, nullptr, nullptr, SLOT(onSides(int)))); // a member needs a receiver
    CHECK(!QObject::disconnect(static_cast<QObject *>(nullptr), nullptr, nullptr, nullptr));
    // Disconnecting everything
    QObject::connect(&shape, SIGNAL(sidesChanged(int)), &a, SLOT(onSides(int)));
    QObject::connect(&shape, SIGNAL(titleChanged(QString)), &b, SLOT(onTitle(QString)));
    CHECK(shape.disconnect());
    CHECK(!shape.disconnect());

    // QMetaMethod forms
    const QMetaMethod sig = QMetaMethod::fromSignal(&Shape::sidesChanged);
    const QMetaMethod slot = Receiver::staticMetaObject.method(Receiver::staticMetaObject.indexOfSlot("onSides(int)"));
    CHECK(QObject::connect(&shape, sig, &a, slot));
    shape.setSides(40);
    CHECK(a.total == 7 + 40);
    CHECK(QObject::disconnect(&shape, sig, &a, slot));
    CHECK(!QObject::connect(&shape, slot, &a, slot)); // not a signal
    CHECK(!QObject::connect(&shape, sig, &a, QMetaMethod()));
    const QMetaMethod wrongOwner =
        Receiver::staticMetaObject.method(Receiver::staticMetaObject.indexOfSlot("onVoid()"));
    CHECK(!QObject::connect(&a, sig, &shape, wrongOwner)); // the sender is not a Shape
    // The index forms
    const int signalIndex = Shape::staticMetaObject.indexOfSignal("sidesChanged(int)");
    const int slotIndex = Receiver::staticMetaObject.indexOfSlot("onSides(int)");
    CHECK(QMetaObject::connect(&shape, signalIndex, &a, slotIndex));
    shape.setSides(41);
    CHECK(a.total == 7 + 40 + 41);
    CHECK(QMetaObject::disconnect(&shape, signalIndex, &a, slotIndex));
    CHECK(!QMetaObject::disconnect(&shape, signalIndex, &a, slotIndex));
    CHECK(!QMetaObject::connect(&shape, -1, &a, slotIndex));
    CHECK(!QMetaObject::connect(nullptr, signalIndex, &a, slotIndex));
}

static void testQueuedStringConnection() {
    Shape shape;
    Receiver r;
    CHECK(QObject::connect(&shape, SIGNAL(sidesChanged(int)), &r, SLOT(onSides(int)), Qt::QueuedConnection));
    shape.setSides(21);
    CHECK(r.total == 0);
    QCoreApplication::processEvents();
    CHECK(r.total == 21);
}

static void testActivate() {
    Shape shape;
    Receiver r;
    QObject::connect(&shape, SIGNAL(sidesChanged(int)), &r, SLOT(onSides(int)));
    int value = 77;
    void *argv[] = {nullptr, &value};
    QMetaObject::activate(&shape, Shape::staticMetaObject.indexOfSignal("sidesChanged(int)"), argv);
    CHECK(r.total == 77);
    QMetaObject::activate(
        &shape, &Shape::staticMetaObject,
        Shape::staticMetaObject.indexOfSignal("sidesChanged(int)") - Shape::staticMetaObject.methodOffset(), argv);
    CHECK(r.total == 154);
    QMetaObject::activate(&shape, Shape::staticMetaObject.indexOfSlot("ping()"), argv); // a slot: ignored
    QMetaObject::activate(nullptr, 0, argv);
    // Queued delivery copies the arguments by their meta-types.
    Receiver q;
    QObject::connect(&shape, SIGNAL(sidesChanged(int)), &q, SLOT(onSides(int)), Qt::QueuedConnection);
    value = 5;
    QMetaObject::activate(&shape, Shape::staticMetaObject.indexOfSignal("sidesChanged(int)"), argv);
    value = 6;
    QCoreApplication::processEvents();
    CHECK(q.total == 5);
}

static void testConnectSlotsByName() {
    Receiver parent;
    parent.setObjectName("parent");
    auto *child = new Shape(&parent);
    child->setObjectName("child");
    QMetaObject::connectSlotsByName(&parent);
    child->setSides(15);
    CHECK(parent.log == "child15;");
    QMetaObject::connectSlotsByName(nullptr);
}

// Gadgets and namespaces

static void testGadget() {
    const QMetaObject *mo = &meta::Point3::staticMetaObject;
    CHECK(std::string(mo->className()) == "meta::Point3");
    CHECK(mo->superClass() == nullptr);
    CHECK(mo->propertyCount() == 3);
    meta::Point3 p;
    p.x = 3;
    p.y = 4;
    p.label = "pt";
    CHECK(mo->property(0).readOnGadget(&p).toInt() == 3);
    CHECK(mo->property(2).readOnGadget(&p).toString() == "pt");
    CHECK(mo->property(1).writeOnGadget(&p, 10));
    CHECK(p.y == 10);
    CHECK(!mo->property(1).writeOnGadget(&p, QStringLiteral("x")));
    CHECK(mo->property(0).readOnGadget(nullptr).isNull());
    CHECK(mo->indexOfMethod("sum()") == 0);
    // A gadget in a QVariant knows its meta-object.
    const QVariant v = QVariant::fromValue(p);
    CHECK(v.metaType().metaObject() == mo);
    CHECK(v.value<meta::Point3>().x == 3);
    CHECK(std::string(v.typeName()) == "meta::Point3");
}

// QMetaObject utilities

static void testNormalization() {
    CHECK(QMetaObject::normalizedSignature("  foo ( const QString & , unsigned int  )") == "foo(QString,uint)");
    CHECK(QMetaObject::normalizedSignature("foo()") == "foo()");
    CHECK(QMetaObject::normalizedSignature("foo(void)") == "foo()");
    CHECK(QMetaObject::normalizedSignature("foo(QString const &, long long, unsigned long long, unsigned char)") ==
          "foo(QString,qlonglong,qulonglong,uchar)");
    CHECK(QMetaObject::normalizedSignature("foo(QMap<int, QList<int> >, const char *)") ==
          "foo(QMap<int,QList<int>>,const char*)");
    CHECK(QMetaObject::normalizedSignature("foo(QString &, QString &&)") == "foo(QString&,QString&&)");
    CHECK(QMetaObject::normalizedSignature("foo(unsigned short, unsigned long, long, short, signed char)") ==
          "foo(ushort,ulong,long,short,signed char)");
    CHECK(QMetaObject::normalizedSignature("foo(struct Bar, enum Baz, ::Qux)") == "foo(Bar,Baz,Qux)");
    CHECK(QMetaObject::normalizedSignature("foo(const int)") == "foo(int)");
    CHECK(QMetaObject::normalizedSignature("foo(const QString *const)") == "foo(const QString*const)" ||
          QMetaObject::normalizedSignature("foo(const QString *const)") == "foo(const QString*)");
    CHECK(QMetaObject::normalizedSignature("foo") == "foo");
    CHECK(QMetaObject::normalizedSignature(nullptr).isEmpty());
    CHECK(QMetaObject::normalizedType("const QString &") == "QString");
    CHECK(QMetaObject::normalizedType("QString &") == "QString&");
    CHECK(QMetaObject::normalizedType("const QString*") == "const QString*");
    CHECK(QMetaObject::normalizedType("unsigned") == "uint");
    CHECK(QMetaObject::normalizedType("long unsigned int") == "ulong");
    CHECK(QMetaObject::normalizedType("  int  ") == "int");
    CHECK(QMetaObject::normalizedType("") == "");
    CHECK(QMetaObject::checkConnectArgs("a(int,QString)", "b(int)"));
    CHECK(QMetaObject::checkConnectArgs("a(int,QString)", "b()"));
    CHECK(QMetaObject::checkConnectArgs("a(int)", "b(const int &)"));
    CHECK(!QMetaObject::checkConnectArgs("a(int)", "b(QString)"));
    CHECK(!QMetaObject::checkConnectArgs("a()", "b(int)"));
    CHECK(!QMetaObject::checkConnectArgs(nullptr, "b()"));
    CHECK(QMetaObject::checkConnectArgs(QMetaMethod::fromSignal(&Shape::multi),
                                        QMetaMethod::fromSignal(&Shape::sidesChanged)));
}

// QTypeRevision

static void testTypeRevision() {
    QTypeRevision invalid;
    CHECK(!invalid.isValid() && !invalid.hasMajorVersion() && !invalid.hasMinorVersion());
    CHECK(invalid.majorVersion() == 0 && invalid.minorVersion() == 0);
    QTypeRevision r = QTypeRevision::fromVersion(2, 15);
    CHECK(r.isValid() && r.hasMajorVersion() && r.hasMinorVersion());
    CHECK(r.majorVersion() == 2 && r.minorVersion() == 15);
    CHECK(r.toEncodedVersion<quint16>() == 0x020f);
    CHECK(QTypeRevision::fromEncodedVersion(0x020f) == r);
    CHECK(QTypeRevision::fromMajorVersion(3).hasMajorVersion() &&
          !QTypeRevision::fromMajorVersion(3).hasMinorVersion());
    CHECK(QTypeRevision::fromMinorVersion(4).hasMinorVersion() &&
          !QTypeRevision::fromMinorVersion(4).hasMajorVersion());
    CHECK(QTypeRevision::fromMinorVersion(4).toEncodedVersion<quint16>() == 0xff04);
    CHECK(QTypeRevision::fromMajorVersion(3).toEncodedVersion<quint16>() == 0x03ff);
    CHECK(QTypeRevision::zero().isValid() && QTypeRevision::zero().majorVersion() == 0);
    CHECK(QTypeRevision::zero().toEncodedVersion<int>() == 0);
    CHECK(QTypeRevision::fromEncodedVersion(0xffff) == invalid);
    CHECK(QTypeRevision::fromVersion(1, 2) < QTypeRevision::fromVersion(1, 3));
    CHECK(QTypeRevision::fromVersion(1, 9) < QTypeRevision::fromVersion(2, 0));
    CHECK(QTypeRevision::fromVersion(2, 0) > QTypeRevision::fromVersion(1, 9));
    CHECK(QTypeRevision::fromVersion(2, 0) <= QTypeRevision::fromVersion(2, 0));
    CHECK(QTypeRevision::fromVersion(2, 0) >= QTypeRevision::fromVersion(2, 0));
    CHECK(QTypeRevision::fromVersion(2, 0) != QTypeRevision::fromVersion(2, 1));
    CHECK(qHash(r) == qHash(QTypeRevision::fromVersion(2, 15)));
    CHECK(qHash(r) != qHash(QTypeRevision::fromVersion(2, 16)));
    constexpr QTypeRevision constexprRevision = QTypeRevision::fromVersion(1, 0);
    static_assert(constexprRevision.majorVersion() == 1, "constexpr");
}

static void testUnusualDeclarations() {
    const QMetaObject *mo = &Concrete::staticMetaObject;
    CHECK(std::string(mo->className()) == "Concrete");
    CHECK(mo->superClass() == &AbstractBase::staticMetaObject);
    CHECK(std::string(AbstractBase::staticMetaObject.className()) == "AbstractBase");
    CHECK(AbstractBase::staticMetaObject.indexOfSlot("compute(int)") >= 0);
    CHECK(AbstractBase::staticMetaObject.indexOfSlot("notifyAll()") >= 0);
    // Pure virtual and overridden slots are called virtually.
    Concrete c;
    int result = 0;
    CHECK(QMetaObject::invokeMethod(&c, "compute", Q_RETURN_ARG(int, result), Q_ARG(int, 4)));
    CHECK(result == 12);
    CHECK(QMetaObject::invokeMethod(&c, "notifyAll"));
    CHECK(c.log == "concrete;");
    QObject *base = &c;
    CHECK(base->metaObject() == mo);

    // Default arguments with commas inside parentheses, brackets and strings
    CHECK(mo->indexOfMethod("make(int,QString,int)") >= 0);
    CHECK(mo->indexOfMethod("make(int,QString)") >= 0);
    CHECK(mo->indexOfMethod("make(int)") >= 0);
    CHECK(mo->indexOfMethod("defaults(int,QString)") >= 0);
    CHECK(mo->indexOfMethod("defaults(int)") >= 0);
    CHECK(mo->indexOfMethod("defaults()") >= 0);
    QList<int> made;
    CHECK(QMetaObject::invokeMethod(&c, "make", Q_RETURN_ARG(QList<int>, made), Q_ARG(int, 9)));
    CHECK((made == QList<int>{9, 2}));
    CHECK(QMetaObject::invokeMethod(&c, "defaults", Q_RETURN_ARG(int, result)));
    CHECK(result == 2 + 3);
    // Unusual return types and declaration forms
    using IntPair = std::pair<int, int>;
    qRegisterMetaType<IntPair>("IntPair");
    IntPair pair;
    CHECK(QMetaObject::invokeMethod(&c, "twoInts", Q_RETURN_ARG(IntPair, pair)));
    CHECK(pair.second == 2);
    CHECK(QMetaObject::invokeMethod(&c, "staticSlot", Q_RETURN_ARG(int, result), Q_ARG(int, 1)));
    CHECK(result == 2);
    QString text;
    CHECK(QMetaObject::invokeMethod(&c, "refResult", Q_RETURN_ARG(QString, text)));
    CHECK(text == "text");
    CHECK(mo->indexOfSlot("withFunctionPointer(void(*)(int),int)") >= 0);
    CHECK((mo->method(mo->indexOfSlot("withFunctionPointer(void(*)(int),int)")).parameterNames() ==
           QList<QByteArray>{"callback", "x"}));
    // Markers on single declarations
    CHECK(mo->indexOfSignal("inlineSignal(int)") >= 0);
    CHECK(mo->indexOfSlot("inlineSlot()") >= 0);
    CHECK(QMetaObject::invokeMethod(&c, "inlineSlot"));
    CHECK(QMetaObject::invokeMethod(&c, "marker"));
    CHECK(mo->method(mo->indexOfMethod("revised()")).revision() == ((255 << 8) | 2));
    CHECK(mo->indexOfMethod("notASlot()") == -1);
    CHECK(mo->indexOfMethod("notInvokable(int)") == -1);
    CHECK(mo->indexOfSignal("fake()") == -1);
    CHECK(mo->indexOfSignal("sig(QMap<int,QString>,QList<QList<int>>)") >= 0);
    CHECK(mo->indexOfSignal("sig(QMap<int,QString>)") >= 0);
    // Nested classes and namespaces
    CHECK(std::string(Concrete::Inner::staticMetaObject.className()) == "Concrete::Inner");
    Concrete::Inner inner;
    CHECK(QMetaObject::invokeMethod(&inner, "innerSlot", Q_RETURN_ARG(int, result)));
    CHECK(result == 8);
    CHECK(std::string(outer::inner::Deep::staticMetaObject.className()) == "outer::inner::Deep");
    StructObject so;
    CHECK(std::string(StructObject::staticMetaObject.className()) == "StructObject");
    CHECK(so.property("number").toInt() == 0);
    CHECK(QMetaObject::invokeMethod(&so, "bump"));
    CHECK(so.number == 1);
    CHECK(so.setProperty("number", 5) && so.number == 5);

    // Properties
    CHECK(mo->propertyCount() == mo->propertyOffset() + 8);
    CHECK(c.setProperty("value", 3) && c.value() == 3);
    CHECK(c.setProperty("multi", 2.5) && c.multi() == 2.5);
    CHECK(c.property("multi").toDouble() == 2.5);
    CHECK(c.setProperty("table", QVariantMap{{"k", QVariantList{1, 2}}}));
    CHECK((c.table().value("k") == QList<int>{1, 2}));
    CHECK(c.setProperty("mode", QStringLiteral("D")) && c.mode() == Concrete::D);
    CHECK(c.property("mode").toInt() == 10);
    CHECK(c.property("revised").toInt() == 3);
    CHECK(mo->property(mo->indexOfProperty("revised")).revision() == ((255 << 8) | 3));
    const QMetaProperty constant = mo->property(mo->indexOfProperty("constant"));
    CHECK(constant.isConstant() && !constant.isWritable() && constant.isReadable());
    CHECK(c.property("constant").toInt() == 5);
    const QMetaProperty required = mo->property(mo->indexOfProperty("required"));
    CHECK(required.isRequired() && required.isWritable());
    CHECK(!mo->property(mo->indexOfProperty("stored")).isStored(&c));
    // Class info with escapes
    CHECK(std::string(mo->classInfo(mo->indexOfClassInfo("text")).value()) == "a \"quoted\" value, with (parentheses)");
    CHECK(std::string(mo->classInfo(mo->indexOfClassInfo("empty")).value()).empty());
    // Enumerations
    const QMetaEnum mode = mo->enumerator(mo->indexOfEnumerator("Mode"));
    CHECK(mode.keyCount() == 4 && mode.keyToValue("B") == 8 && mode.keyToValue("C") == 8 && mode.keyToValue("D") == 10);
    CHECK(!mode.isScoped());
    const QMetaEnum strict = QMetaEnum::fromType<Concrete::Strict>();
    CHECK(strict.isScoped() && strict.keyToValue("On") == 1);
    const QMetaEnum opts = QMetaEnum::fromType<Concrete::Opts>();
    CHECK(opts.isFlag() && opts.keysToValue("X|Y") == 3);
    // Connections with string signatures use the normalized forms.
    int seen = 0;
    QObject::connect(&c, SIGNAL(valueChanged(int)), &c, SLOT(inlineSlot()));
    QObject::connect(&c, &Concrete::valueChanged, [&](int v) { seen = v; });
    c.setValue(11);
    CHECK(seen == 11 && c.log.endsWith("inline;"));
}

static void testLibraryClasses() {
    // The classes of Nt Core are described like any other.
    CHECK(QTimer::staticMetaObject.indexOfSignal("timeout()") >= 0);
    CHECK(QTimer::staticMetaObject.indexOfSlot("stop()") >= 0);
    QTimer timer;
    int fired = 0;
    QObject::connect(&timer, SIGNAL(timeout()), &timer, SLOT(stop()));
    QObject::connect(&timer, &QTimer::timeout, [&] { ++fired; });
    QMetaObject::invokeMethod(&timer, "start", Q_ARG(int, 1));
    CHECK(timer.isActive());
    QEventLoop loop;
    QTimer::singleShot(30, &loop, &QEventLoop::quit);
    loop.exec();
    CHECK(fired >= 1);
    CHECK(!timer.isActive());
    CHECK(QCoreApplication::staticMetaObject.superClass() == &QObject::staticMetaObject);
    CHECK(QBuffer::staticMetaObject.inherits(&QIODevice::staticMetaObject));
    CHECK(QEventLoop::staticMetaObject.indexOfSlot("quit()") >= 0);
}

class ThreadProbe : public QObject {
    Q_OBJECT
public:
    std::atomic<int> sum{0};
    std::atomic<bool> wrongThread{false};

public slots:
    int addAndReturn(int a, int b) {
        check();
        return a + b;
    }
    void add(int v) {
        check();
        sum += v;
    }

signals:
    void produced(int v);

private:
    void check() {
        if (QThread::currentThread() != thread())
            wrongThread = true;
    }
};

static void testConditionals() {
    const QMetaObject *mo = &Conditional::staticMetaObject;
    CHECK(mo->indexOfSignal("featured()") >= 0);
    CHECK(mo->indexOfSignal("notBuilt()") < 0);
    CHECK(mo->indexOfSlot("alsoNotBuilt()") < 0);
    CHECK(mo->indexOfSlot("always()") >= 0);
}

static void testThreads() {
    QThread thread;
    ThreadProbe probe;
    probe.moveToThread(&thread);
    thread.start();

    // Auto connection from another thread is queued, and a blocking call returns a value.
    int result = 0;
    CHECK(QMetaObject::invokeMethod(&probe, "addAndReturn", Qt::BlockingQueuedConnection, Q_RETURN_ARG(int, result),
                                    Q_ARG(int, 20), Q_ARG(int, 22)));
    CHECK(result == 42);
    CHECK(QMetaObject::invokeMethod(&probe, "add", Qt::AutoConnection, Q_ARG(int, 5)));
    CHECK(QMetaObject::invokeMethod(&probe, "add", Qt::QueuedConnection, Q_ARG(int, 6)));
    bool functorThreadOk = false;
    CHECK(QMetaObject::invokeMethod(
        &probe, [&] { functorThreadOk = QThread::currentThread() == probe.thread(); }, Qt::BlockingQueuedConnection));
    CHECK(functorThreadOk);
    // The blocking call is processed after the queued ones.
    CHECK(QMetaObject::invokeMethod(&probe, "addAndReturn", Qt::BlockingQueuedConnection, Q_RETURN_ARG(int, result),
                                    Q_ARG(int, 0), Q_ARG(int, 0)));
    CHECK(probe.sum == 11);

    // A string-based connection into the thread is queued by AutoConnection.
    CHECK(QObject::connect(&probe, SIGNAL(produced(int)), &probe, SLOT(add(int))));
    emit probe.produced(100);
    CHECK(QMetaObject::invokeMethod(&probe, "addAndReturn", Qt::BlockingQueuedConnection, Q_RETURN_ARG(int, result),
                                    Q_ARG(int, 0), Q_ARG(int, 0)));
    CHECK(probe.sum == 111);

    // The thread of the object decides, not the thread that made the call.
    std::thread other([&] {
        QMetaObject::invokeMethod(&probe, "add", Q_ARG(int, 1000));
        int r = 0;
        QMetaObject::invokeMethod(&probe, "addAndReturn", Qt::BlockingQueuedConnection, Q_RETURN_ARG(int, r),
                                  Q_ARG(int, 1), Q_ARG(int, 2));
    });
    other.join();
    CHECK(probe.sum == 1111);

    // Calling from the object's own thread directly runs the method before returning.
    int direct = 0;
    QMetaObject::invokeMethod(
        &probe,
        [&] {
            QMetaObject::invokeMethod(&probe, "addAndReturn", Qt::BlockingQueuedConnection, Q_RETURN_ARG(int, direct),
                                      Q_ARG(int, 3), Q_ARG(int, 4));
        },
        Qt::BlockingQueuedConnection);
    CHECK(direct == 7);
    CHECK(!probe.wrongThread);

    thread.quit();
    CHECK(thread.wait());
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testClassInfo();
    testMethods();
    testDynamicMetaObject();
    testAttributes();
    testPropertyDescriptions();
    testPropertyAccess();
    testObjectProperties();
    testEnums();
    testInvokeMethod();
    testQueuedInvoke();
    testNewInstance();
    testStringConnections();
    testDisconnect();
    testQueuedStringConnection();
    testActivate();
    testConnectSlotsByName();
    testGadget();
    testNormalization();
    testTypeRevision();
    testUnusualDeclarations();
    testLibraryClasses();
    testConditionals();
    testThreads();
    if (g_failures == 0)
        std::printf("meta: all tests passed\n");
    return g_failures == 0 ? 0 : 1;
}

#include "test_meta.moc" // NOLINT(bugprone-suspicious-include)
