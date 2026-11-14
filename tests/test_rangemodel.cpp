// MIT License - see LICENSE
// QRangeModel over lists, tables, gadgets, objects, role maps and trees.
#include "rangemodel_types.h"

#include <QCoreApplication>
#include <QPersistentModelIndex>
#include <QRangeModel>
#include <QSignalSpy>

#include <array>
#include <cstdio>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                       \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

template <> struct QRangeModel::RowOptions<Book> {
    static constexpr auto rowCategory = QRangeModel::RowCategory::MultiRoleItem;
};

static void testList() {
    std::vector<int> numbers = {1, 2, 3};
    QRangeModel model(std::ref(numbers));
    CHECK(model.rowCount() == 3 && model.columnCount() == 1);
    CHECK(model.data(model.index(1, 0)).toInt() == 2);
    CHECK(model.data(model.index(1, 0), Qt::EditRole).toInt() == 2);
    CHECK(!model.data(model.index(1, 0), Qt::ToolTipRole).isValid());
    CHECK(!model.index(3, 0).isValid() && !model.index(0, 1).isValid());
    CHECK(model.flags(model.index(0, 0)).testFlag(Qt::ItemIsEditable));
    CHECK(model.flags(model.index(0, 0)).testFlag(Qt::ItemNeverHasChildren));
    CHECK(!model.hasChildren(model.index(0, 0)) && !model.parent(model.index(0, 0)).isValid());
    CHECK(model.sibling(2, 0, model.index(0, 0)) == model.index(2, 0));

    int changes = 0;
    QObject::connect(&model, &QAbstractItemModel::dataChanged,
                     [&](const QModelIndex &a, const QModelIndex &b, const QList<int> &roles) {
                         ++changes;
                         CHECK(a == b && roles.contains(Qt::DisplayRole));
                     });
    CHECK(model.setData(model.index(0, 0), 10));
    CHECK(numbers[0] == 10 && changes == 1);
    CHECK(model.setData(model.index(0, 0), QStringLiteral("11")));
    CHECK(numbers[0] == 11);
    CHECK(!model.setData(model.index(0, 0), QStringLiteral("x")));
    CHECK(!model.setData(model.index(0, 0), 5, Qt::ToolTipRole));
    CHECK(model.itemData(model.index(0, 0)).value(Qt::DisplayRole).toInt() == 11);

    CHECK(model.insertRows(1, 2));
    CHECK(numbers.size() == 5 && numbers[1] == 0 && numbers[3] == 2);
    CHECK(model.removeRows(1, 2));
    CHECK((numbers == std::vector<int>{11, 2, 3}));
    CHECK(!model.removeRows(2, 2));
    CHECK(model.moveRows(QModelIndex(), 0, 1, QModelIndex(), 3));
    CHECK((numbers == std::vector<int>{2, 3, 11}));
    CHECK(model.moveRows(QModelIndex(), 2, 1, QModelIndex(), 0));
    CHECK((numbers == std::vector<int>{11, 2, 3}));
    CHECK(!model.insertColumns(0, 1));

    // A copy: the caller's range does not change.
    QList<QString> names = {QStringLiteral("a"), QStringLiteral("b")};
    QRangeModel copy(names);
    CHECK(copy.setData(copy.index(0, 0), QStringLiteral("z")));
    CHECK(copy.data(copy.index(0, 0)).toString() == QStringLiteral("z") && names[0] == QStringLiteral("a"));

    // A const range is read-only, and a fixed-size one cannot grow.
    const std::vector<int> fixed = {1, 2};
    QRangeModel readOnly(&fixed);
    CHECK(!readOnly.flags(readOnly.index(0, 0)).testFlag(Qt::ItemIsEditable));
    CHECK(!readOnly.setData(readOnly.index(0, 0), 5) && fixed[0] == 1);
    CHECK(!readOnly.insertRows(0, 1) && !readOnly.removeRows(0, 1));
    std::array<int, 3> array = {1, 2, 3};
    QRangeModel arrayModel(std::ref(array));
    CHECK(arrayModel.rowCount() == 3 && !arrayModel.insertRows(0, 1));
    CHECK(arrayModel.setData(arrayModel.index(2, 0), 9) && array[2] == 9);
}

static void testTables() {
    std::vector<std::tuple<QString, int, double>> rows = {{QStringLiteral("a"), 1, 1.5}, {QStringLiteral("b"), 2, 2.5}};
    QRangeModel tuples(std::ref(rows));
    CHECK(tuples.rowCount() == 2 && tuples.columnCount() == 3);
    CHECK(tuples.data(tuples.index(1, 0)).toString() == QStringLiteral("b"));
    CHECK(tuples.data(tuples.index(1, 2)).toDouble() == 2.5);
    CHECK(tuples.setData(tuples.index(0, 1), 42) && std::get<1>(rows[0]) == 42);
    CHECK(tuples.insertRows(2, 1) && rows.size() == 3);
    CHECK(tuples.headerData(1, Qt::Horizontal).toInt() == 2);

    QList<QList<int>> grid = {{1, 2, 3}, {4, 5, 6}};
    QRangeModel lists(&grid);
    CHECK(lists.rowCount() == 2 && lists.columnCount() == 3);
    CHECK(lists.data(lists.index(1, 2)).toInt() == 6);
    CHECK(lists.insertRows(1, 1) && grid.size() == 3 && grid[1].size() == 3);
    CHECK(lists.insertColumns(1, 2) && lists.columnCount() == 5 && grid[0] == (QList<int>{1, 0, 0, 2, 3}));
    CHECK(lists.removeColumns(1, 2) && grid[0] == (QList<int>{1, 2, 3}));
    CHECK(lists.moveColumns(QModelIndex(), 0, 1, QModelIndex(), 3) && grid[2] == (QList<int>{5, 6, 4}));
    CHECK(lists.removeRows(1, 1) && grid.size() == 2);

    std::vector<std::pair<QString, bool>> pairs = {{QStringLiteral("x"), true}};
    QRangeModel pairModel(std::move(pairs));
    CHECK(pairModel.columnCount() == 2 && pairModel.data(pairModel.index(0, 1)).toBool());
}

static void testGadgetsAndObjects() {
    // Gadget rows: one column per property, with the names as the header.
    std::vector<Point> points = {{1, 2}, {3, 4}};
    QRangeModel model(std::ref(points));
    CHECK(model.columnCount() == 2);
    CHECK(model.headerData(0, Qt::Horizontal).toString() == QStringLiteral("x"));
    CHECK(model.headerData(1, Qt::Horizontal).toString() == QStringLiteral("y"));
    CHECK(model.data(model.index(1, 1)).toInt() == 4);
    CHECK(model.setData(model.index(1, 1), 40) && points[1].y == 40);
    CHECK(model.flags(model.index(0, 0)).testFlag(Qt::ItemIsEditable));

    // Multi-role gadget rows: one column, the properties are roles.
    QList<Book> books = {{QStringLiteral("Dune"), QStringLiteral("Herbert")}};
    QRangeModel bookModel(&books);
    CHECK(bookModel.columnCount() == 1);
    const QHash<int, QByteArray> names = bookModel.roleNames();
    CHECK(names.value(Qt::DisplayRole) == QByteArray("display"));
    const int authorRole = names.key(QByteArray("author"), -1);
    CHECK(authorRole >= Qt::UserRole);
    CHECK(bookModel.data(bookModel.index(0, 0), authorRole).toString() == QStringLiteral("Herbert"));
    // The property "display" serves Qt::DisplayRole.
    CHECK(bookModel.data(bookModel.index(0, 0)).toString() == QStringLiteral("Dune"));
    CHECK(bookModel.setData(bookModel.index(0, 0), QStringLiteral("Asimov"), authorRole));
    CHECK(books[0].author == QStringLiteral("Asimov"));
    const QMap<int, QVariant> item = bookModel.itemData(bookModel.index(0, 0));
    CHECK(item.value(authorRole).toString() == QStringLiteral("Asimov"));

    // Custom role names.
    QSignalSpy spy(&bookModel, &QRangeModel::roleNamesChanged);
    QHash<int, QByteArray> custom = names;
    custom.insert(Qt::UserRole + 50, QByteArray("title"));
    bookModel.setRoleNames(custom);
    CHECK(spy.count() == 1 && bookModel.roleNames().value(Qt::UserRole + 50) == QByteArray("title"));
    bookModel.resetRoleNames();
    CHECK(spy.count() == 2 && bookModel.roleNames() == names);

    // QObject rows by pointer: the properties of QObject itself are left out.
    Person ann(QStringLiteral("Ann"), 30), bo(QStringLiteral("Bo"), 40);
    std::vector<Person *> people = {&ann, &bo};
    QRangeModel personModel(std::ref(people));
    CHECK(personModel.columnCount() == 2);
    CHECK(personModel.headerData(0, Qt::Horizontal).toString() == QStringLiteral("name"));
    CHECK(personModel.data(personModel.index(1, 1)).toInt() == 40);
    CHECK(personModel.setData(personModel.index(0, 1), 31) && ann.age() == 31);
    CHECK(!personModel.flags(personModel.index(0, 0)).testFlag(Qt::ItemIsEditable));
    CHECK(!personModel.setData(personModel.index(0, 0), QStringLiteral("x")));

    // Rows held by smart pointers are made by the model.
    std::vector<std::unique_ptr<Point>> owned;
    owned.push_back(std::make_unique<Point>(Point{5, 6}));
    QRangeModel ownedModel(std::ref(owned));
    CHECK(ownedModel.data(ownedModel.index(0, 0)).toInt() == 5);
    CHECK(ownedModel.insertRows(1, 1) && owned.size() == 2 && owned[1] != nullptr);
    CHECK(ownedModel.removeRows(0, 1) && owned.size() == 1);
}

static void testRoleMaps() {
    std::vector<QMap<int, QVariant>> items(2);
    items[0][Qt::DisplayRole] = QStringLiteral("first");
    items[0][Qt::ToolTipRole] = QStringLiteral("tip");
    QRangeModel model(std::ref(items));
    CHECK(model.columnCount() == 1);
    CHECK(model.data(model.index(0, 0), Qt::ToolTipRole).toString() == QStringLiteral("tip"));
    CHECK(model.setData(model.index(1, 0), QStringLiteral("deco"), Qt::DecorationRole));
    CHECK(items[1].value(Qt::DecorationRole).toString() == QStringLiteral("deco"));
    CHECK(model.itemData(model.index(0, 0)).size() == 2);
    CHECK(model.clearItemData(model.index(0, 0)) && items[0].isEmpty());

    std::vector<std::map<QString, QVariant>> named(1);
    named[0][QStringLiteral("display")] = 7;
    QRangeModel namedModel(std::ref(named));
    CHECK(namedModel.data(namedModel.index(0, 0)).toInt() == 7);
    CHECK(namedModel.setData(namedModel.index(0, 0), 8, Qt::ToolTipRole) &&
          named[0][QStringLiteral("toolTip")].toInt() == 8);
}

static void testTree() {
    Tree tree;
    tree.push_back(TreeRow(QStringLiteral("root"), 1));
    tree.push_back(TreeRow(QStringLiteral("leaf"), 5));
    tree[0].addChild(QStringLiteral("child a"), 2);
    tree[0].addChild(QStringLiteral("child b"), 3);
    tree[0].children->at(1).addChild(QStringLiteral("grandchild"), 4);

    QRangeModel model(std::ref(tree));
    CHECK(model.rowCount() == 2 && model.columnCount() == 2);
    const QModelIndex root = model.index(0, 0);
    CHECK(model.hasChildren(root) && model.rowCount(root) == 2);
    CHECK(!model.hasChildren(model.index(1, 0)));
    const QModelIndex childB = model.index(1, 0, root);
    CHECK(model.data(childB).toString() == QStringLiteral("child b"));
    CHECK(model.data(model.index(1, 1, root)).toInt() == 3);
    CHECK(model.parent(childB) == root);
    const QModelIndex grand = model.index(0, 0, childB);
    CHECK(model.data(grand).toString() == QStringLiteral("grandchild"));
    CHECK(model.parent(grand) == childB && model.parent(model.parent(grand)) == root);
    CHECK(!model.flags(grand).testFlag(Qt::ItemNeverHasChildren));
    CHECK(model.setData(model.index(0, 1, childB), 44) && tree[0].children->at(1).children->at(0).value == 44);

    // Inserting moves the rows of a vector in memory; the persistent indexes below them follow.
    QPersistentModelIndex persistentGrand(grand);
    CHECK(model.insertRows(0, 3, root));
    CHECK(model.rowCount(root) == 5);
    CHECK(persistentGrand.isValid() && persistentGrand.data().toString() == QStringLiteral("grandchild"));
    CHECK(persistentGrand.parent().row() == 4);
    CHECK(model.data(model.index(4, 0, root)).toString() == QStringLiteral("child b"));
    CHECK(tree[0].children->at(4).children->at(0).parent == &tree[0].children->at(4));

    // A row without children gets them on insertion.
    const QModelIndex leaf = model.index(1, 0);
    CHECK(model.insertRows(0, 1, leaf) && model.rowCount(leaf) == 1);
    CHECK(model.parent(model.index(0, 0, leaf)) == leaf);

    CHECK(model.removeRows(0, 3, root));
    CHECK(persistentGrand.isValid() && persistentGrand.parent().row() == 1);
    CHECK(model.moveRows(root, 1, 1, root, 0));
    CHECK(persistentGrand.isValid() && persistentGrand.parent().row() == 0);
    CHECK(model.data(persistentGrand.parent()).toString() == QStringLiteral("child b"));
    CHECK(!model.moveRows(root, 0, 1, QModelIndex(), 0));
    CHECK(model.removeRows(0, 1, root));
    CHECK(!persistentGrand.isValid());

    // The same tree through a protocol object.
    Tree other;
    other.push_back(TreeRow(QStringLiteral("p"), 1));
    other[0].addChild(QStringLiteral("c"), 2);
    QRangeModel viaProtocol(std::ref(other), TreeProtocol{});
    const QModelIndex p = viaProtocol.index(0, 0);
    CHECK(viaProtocol.rowCount(p) == 1);
    CHECK(viaProtocol.parent(viaProtocol.index(0, 0, p)) == p);
    CHECK(viaProtocol.data(viaProtocol.index(0, 1, p)).toInt() == 2);
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testList();
    testTables();
    testGadgetsAndObjects();
    testRoleMaps();
    testTree();
    if (g_failures == 0)
        std::printf("All range model tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
