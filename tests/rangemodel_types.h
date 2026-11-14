// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <optional>
#include <vector>

// A gadget whose properties are columns.
struct Point {
    Q_GADGET
    Q_PROPERTY(int x MEMBER x)
    Q_PROPERTY(int y MEMBER y)
public:
    int x = 0;
    int y = 0;
};

// A gadget that is one item with a role per property; its "display" property serves Qt::DisplayRole.
struct Book {
    Q_GADGET
    Q_PROPERTY(QString display MEMBER title)
    Q_PROPERTY(QString author MEMBER author)
public:
    QString title;
    QString author;
};

class Person : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(int age READ age WRITE setAge)
public:
    Person(const QString &name, int age) : m_name(name), m_age(age) {}
    QString name() const { return m_name; }
    int age() const { return m_age; }
    void setAge(int age) { m_age = age; }

private:
    QString m_name;
    int m_age;
};

// A tree row with the implicit tree protocol and the tuple protocol for its two columns.
class TreeRow;
using Tree = std::vector<TreeRow>;

class TreeRow {
public:
    TreeRow() = default;
    TreeRow(const QString &n, int v) : name(n), value(v) {}

    const TreeRow *parentRow() const { return parent; }
    void setParentRow(TreeRow *p) { parent = p; }
    const std::optional<Tree> &childRows() const { return children; }
    std::optional<Tree> &childRows() { return children; }

    void addChild(const QString &n, int v) {
        if (!children)
            children.emplace();
        children->push_back(TreeRow(n, v));
        children->back().parent = this;
    }

    template <std::size_t I> friend auto &get(TreeRow &row) {
        if constexpr (I == 0)
            return row.name;
        else
            return row.value;
    }
    template <std::size_t I> friend const auto &get(const TreeRow &row) {
        if constexpr (I == 0)
            return row.name;
        else
            return row.value;
    }

    QString name;
    int value = 0;
    TreeRow *parent = nullptr;
    std::optional<Tree> children;
};

namespace std {
template <> struct tuple_size<TreeRow> : std::integral_constant<std::size_t, 2> {};
template <> struct tuple_element<0, TreeRow> {
    using type = QString;
};
template <> struct tuple_element<1, TreeRow> {
    using type = int;
};
} // namespace std

// The same protocol as an object of its own.
struct TreeProtocol {
    const TreeRow *parentRow(const TreeRow &row) const { return row.parent; }
    std::optional<Tree> &childRows(TreeRow &row) const { return row.children; }
    void setParentRow(TreeRow &row, TreeRow *parent) const { row.parent = parent; }
};
