// MIT License - see LICENSE
#pragma once

#include <QtCore>

#include <string>

namespace meta {

Q_NAMESPACE
enum class Level { Low, Mid = 5, High };
Q_ENUM_NS(Level)

#define MY_TAG

// Q_SCRIPTABLE and a tag in front of the return type.
class Tagged : public QObject {
    Q_OBJECT
public slots:
    Q_SCRIPTABLE int scripted() { return 1; }
    MY_TAG void tagged() {}
    Q_SCRIPTABLE MY_TAG QString both() { return QString(); }
    void plain(int = 0) {}
signals:
    Q_SCRIPTABLE void scriptedSignal();

public:
    Q_SCRIPTABLE int invokable() { return 2; }
};

class Shape : public QObject {
    Q_OBJECT
    Q_CLASSINFO("Author", "Nt")
    Q_CLASSINFO("Version", "1.0")
    Q_PROPERTY(int sides READ sides WRITE setSides NOTIFY sidesChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle RESET resetTitle NOTIFY titleChanged)
    Q_PROPERTY(Kind kind READ kind WRITE setKind)
    Q_PROPERTY(Features features READ features WRITE setFeatures)
    Q_PROPERTY(double area READ area STORED false SCRIPTABLE false)
    Q_PROPERTY(int counter MEMBER m_counter NOTIFY counterChanged)
    Q_PROPERTY(QList<int> values READ values WRITE setValues)
    Q_PROPERTY(bool special READ special DESIGNABLE isSpecial USER true)
    Q_PROPERTY(int fixed READ fixed CONSTANT FINAL REVISION(1, 2))
    Q_PROPERTY(QVariant any READ any WRITE setAny)
    Q_PROPERTY(Shape *partner MEMBER m_partner)

public:
    enum Kind { Triangle, Square = 4, Circle = 100 };
    Q_ENUM(Kind)
    enum Feature { Filled = 1, Outlined = 2, Shadowed = 4 };
    Q_DECLARE_FLAGS(Features, Feature)
    Q_FLAG(Features)
    enum class Color : int { Red, Green = 7 };
    Q_ENUM(Color)

    Q_INVOKABLE explicit Shape(QObject *parent = nullptr) : QObject(parent) {}
    Q_INVOKABLE Shape(int sides, const QString &title, QObject *parent = nullptr)
        : QObject(parent), m_sides(sides), m_title(title) {}

    int sides() const { return m_sides; }
    void setSides(int s) {
        if (s == m_sides)
            return;
        m_sides = s;
        emit sidesChanged(s);
    }
    QString title() const { return m_title; }
    void setTitle(const QString &t) {
        m_title = t;
        emit titleChanged(t);
    }
    void resetTitle() { setTitle(QStringLiteral("untitled")); }
    Kind kind() const { return m_kind; }
    void setKind(Kind k) { m_kind = k; }
    Features features() const { return m_features; }
    void setFeatures(Features f) { m_features = f; }
    double area() const { return 1.5; }
    QList<int> values() const { return m_values; }
    void setValues(const QList<int> &v) { m_values = v; }
    bool special() const { return true; }
    bool isSpecial() const { return m_designable; }
    int fixed() const { return 42; }
    QVariant any() const { return m_any; }
    void setAny(const QVariant &v) { m_any = v; }

    bool m_designable = true;
    int m_counter = 0;
    Shape *m_partner = nullptr;
    QString log;

public slots:
    QString describe(int verbosity = 1) const { return QStringLiteral("shape v%1").arg(verbosity); }
    int add(int a, int b) { return a + b; }
    void setLabel(const QString &label) { log += "label:" + label + ";"; }
    void ping() { log += "ping;"; }
    void ping(int n) { log += "ping" + QString::number(n) + ";"; }
    void byRef(QString &s) { s += "!"; }
    void many(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j) {
        log += QString::number(a + b + c + d + e + f + g + h + i + j) + ";";
    }

protected slots:
    void protectedSlot() { log += "protected;"; }

private slots:
    void secret() { log += "secret;"; }

signals:
    void sidesChanged(int sides);
    void titleChanged(const QString &title);
    void counterChanged();
    void multi(int a, const QString &b, double c = 1.5);
    void withPrivate(int v, QPrivateSignal);
    void unnamed(int, const std::string &);

public:
    Q_INVOKABLE static int twice(int x) { return 2 * x; }
    Q_INVOKABLE QString kindName(Kind k) const { return QMetaEnum::fromType<Kind>().valueToKey(k); }
    Q_INVOKABLE void fireMulti() { emit multi(7, QStringLiteral("seven"), 7.5); }
    Q_INVOKABLE void firePrivate() { emit withPrivate(9, QPrivateSignal()); }
    Q_INVOKABLE void fireNameless() { emit unnamed(1, "x"); }
    Q_INVOKABLE void bumpCounter() {
        ++m_counter;
        emit counterChanged();
    }

private:
    int m_sides = 3;
    QString m_title = QStringLiteral("shape");
    Kind m_kind = Triangle;
    Features m_features;
    QList<int> m_values;
    QVariant m_any;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(Shape::Features)

class Square : public Shape {
    Q_OBJECT
    Q_PROPERTY(double side READ side WRITE setSide)

public:
    explicit Square(QObject *parent = nullptr) : Shape(4, QStringLiteral("square"), parent) {}
    double side() const { return m_side; }
    void setSide(double s) { m_side = s; }

public slots:
    int extra() { return 99; }

signals:
    void sideChanged(double);

private:
    double m_side = 2;
};

// Declared Q_OBJECT but not processed by ntmoc in the same way: it lives in an unnamed namespace
// of the test and is only seen through the generated file.
class Receiver : public QObject {
    Q_OBJECT
public:
    QString log;
    int total = 0;

public slots:
    void onSides(int v) {
        total += v;
        log += "sides" + QString::number(v) + ";";
    }
    void onTitle(const QString &t) { log += "title:" + t + ";"; }
    void onVoid() { log += "void;"; }
    void on_child_sidesChanged(int v) { log += "child" + QString::number(v) + ";"; }

signals:
    void relay(int v);
};

struct Point3 {
    Q_GADGET
    Q_PROPERTY(int x MEMBER x)
    Q_PROPERTY(int y MEMBER y)
    Q_PROPERTY(QString label MEMBER label)

public:
    int x = 0;
    int y = 0;
    QString label;
    Q_INVOKABLE int sum() const { return x + y; }
    Q_INVOKABLE void move(int dx, int dy) {
        x += dx;
        y += dy;
    }
    Q_INVOKABLE QString describe(const QString &prefix) const { return prefix + label; }
};

} // namespace meta
Q_DECLARE_METATYPE(meta::Point3)
