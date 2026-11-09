// MIT License - see LICENSE
#include <QtCore>

#include <cstdio>
#include <memory>
#include <vector>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

using namespace Qt::StringLiterals;

namespace {

// A read-only tree: each node has a name and children.
struct Node {
    QString name;
    Node *parent = nullptr;
    std::vector<std::unique_ptr<Node>> children;
    Node *add(const QString &n) {
        children.push_back(std::make_unique<Node>());
        children.back()->name = n;
        children.back()->parent = this;
        return children.back().get();
    }
    int row() const {
        if (!parent)
            return 0;
        for (std::size_t i = 0; i < parent->children.size(); ++i)
            if (parent->children[i].get() == this)
                return static_cast<int>(i);
        return 0;
    }
};

class TreeModel : public QAbstractItemModel {
public:
    Node root;
    Node *node(const QModelIndex &i) const {
        return i.isValid() ? static_cast<Node *>(i.internalPointer()) : const_cast<Node *>(&root);
    }
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override {
        if (!hasIndex(row, column, parent))
            return QModelIndex();
        return createIndex(row, column, node(parent)->children[static_cast<std::size_t>(row)].get());
    }
    QModelIndex parent(const QModelIndex &child) const override {
        Node *p = child.isValid() ? node(child)->parent : nullptr;
        return !p || p == &root ? QModelIndex() : createIndex(p->row(), 0, p);
    }
    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        return parent.column() > 0 ? 0 : static_cast<int>(node(parent)->children.size());
    }
    int columnCount(const QModelIndex & = QModelIndex()) const override { return 1; }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        return index.isValid() && role == Qt::DisplayRole ? QVariant(node(index)->name) : QVariant();
    }
};

// A table whose cell (r, c) holds prefix + r * 10 + c.
class Table : public QAbstractTableModel {
public:
    Table(int rows, int columns, int base) : m_rows(rows), m_columns(columns), m_base(base) {}
    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return parent.isValid() ? 0 : m_rows; }
    int columnCount(const QModelIndex &parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : m_columns;
    }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        return role == Qt::DisplayRole ? QVariant(m_base + index.row() * 10 + index.column()) : QVariant();
    }
    QVariant headerData(int section, Qt::Orientation o, int role = Qt::DisplayRole) const override {
        if (role != Qt::DisplayRole)
            return QVariant();
        return o == Qt::Horizontal ? QVariant(u"C%1"_s.arg(section)) : QVariant(m_base + section);
    }
    void appendRow() {
        beginInsertRows(QModelIndex(), m_rows, m_rows);
        ++m_rows;
        endInsertRows();
    }

private:
    int m_rows;
    int m_columns;
    int m_base;
};

QStringList column0(const QAbstractItemModel &m, const QModelIndex &parent = QModelIndex()) {
    QStringList r;
    for (int row = 0; row < m.rowCount(parent); ++row)
        r << m.index(row, 0, parent).data().toString();
    return r;
}

} // namespace

static void testStringListModel() {
    QStringListModel model({u"b"_s, u"c"_s, u"a"_s});
    CHECK(model.rowCount() == 3);
    CHECK(model.index(1, 0).data().toString() == u"c"_s);
    CHECK(model.flags(model.index(0, 0)).testFlag(Qt::ItemIsEditable));

    QPersistentModelIndex c = model.index(1, 0);
    int inserted = 0;
    QObject::connect(&model, &QAbstractItemModel::rowsInserted,
                     [&](const QModelIndex &, int first, int last) { inserted += last - first + 1; });
    CHECK(model.insertRows(0, 2));
    CHECK(inserted == 2);
    CHECK(c.row() == 3);
    CHECK(model.setData(model.index(0, 0), u"z"_s));
    CHECK(model.stringList().first() == u"z"_s);

    CHECK(model.removeRows(0, 2));
    CHECK(c.row() == 1);
    model.sort(0);
    CHECK(model.stringList() == QStringList({u"a"_s, u"b"_s, u"c"_s}));
    CHECK(c.row() == 2 && c.data().toString() == u"c"_s);

    CHECK(model.moveRows(QModelIndex(), 2, 1, QModelIndex(), 0));
    CHECK(model.stringList() == QStringList({u"c"_s, u"a"_s, u"b"_s}));
    CHECK(c.row() == 0);

    CHECK(model.removeRows(0, 1));
    CHECK(!c.isValid());

    const QModelIndexList hits = model.match(model.index(0, 0), Qt::DisplayRole, u"b"_s);
    CHECK(hits.size() == 1 && hits.first().row() == 1);
}

static void testSelectionModel() {
    Table table(4, 3, 0);
    QItemSelectionModel sel(&table);
    CHECK(!sel.hasSelection());

    int changes = 0;
    QObject::connect(&sel, &QItemSelectionModel::selectionChanged,
                     [&](const QItemSelection &, const QItemSelection &) { ++changes; });
    sel.select(table.index(1, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
    CHECK(changes == 1);
    CHECK(sel.isRowSelected(1));
    CHECK(sel.isSelected(table.index(1, 2)));
    CHECK(!sel.isSelected(table.index(0, 0)));
    CHECK(sel.selectedIndexes().size() == 3);
    CHECK(sel.selectedRows().size() == 1);

    sel.select(table.index(1, 1), QItemSelectionModel::Deselect);
    CHECK(!sel.isRowSelected(1));
    CHECK(sel.rowIntersectsSelection(1));
    CHECK(sel.selectedIndexes().size() == 2);

    sel.select(QItemSelection(table.index(0, 1), table.index(3, 1)), QItemSelectionModel::ClearAndSelect);
    CHECK(sel.isColumnSelected(1));
    CHECK(sel.selectedColumns().size() == 1);
    CHECK(sel.selectedIndexes().size() == 4);

    sel.select(table.index(0, 1), QItemSelectionModel::Toggle);
    CHECK(!sel.isSelected(table.index(0, 1)));
    CHECK(sel.isSelected(table.index(1, 1)));

    QModelIndex current;
    QObject::connect(&sel, &QItemSelectionModel::currentChanged,
                     [&](const QModelIndex &now, const QModelIndex &) { current = now; });
    sel.setCurrentIndex(table.index(2, 2), QItemSelectionModel::NoUpdate);
    CHECK(current == table.index(2, 2));
    CHECK(sel.currentIndex() == table.index(2, 2));

    sel.clearSelection();
    CHECK(!sel.hasSelection());
    sel.clear();
    CHECK(!sel.currentIndex().isValid());

    QItemSelectionRange range(table.index(0, 0), table.index(1, 1));
    CHECK(range.width() == 2 && range.height() == 2);
    CHECK(range.contains(table.index(1, 0)));
    CHECK(range.intersects(QItemSelectionRange(table.index(1, 1), table.index(3, 2))));
    CHECK(range.intersected(QItemSelectionRange(table.index(1, 1), table.index(3, 2))) ==
          QItemSelectionRange(table.index(1, 1)));

    // The selection follows rows when the model moves them.
    QStringListModel list({u"a"_s, u"b"_s, u"c"_s});
    QItemSelectionModel listSel(&list);
    listSel.select(list.index(2, 0), QItemSelectionModel::Select);
    list.insertRows(0, 1);
    CHECK(listSel.isSelected(list.index(3, 0)));
    list.removeRows(3, 1);
    CHECK(!listSel.hasSelection());
}

static void testIdentityProxy() {
    QStringListModel source({u"one"_s, u"two"_s});
    QIdentityProxyModel proxy;
    proxy.setSourceModel(&source);
    CHECK(proxy.sourceModel() == &source);
    CHECK(column0(proxy) == QStringList({u"one"_s, u"two"_s}));
    CHECK(proxy.mapToSource(proxy.index(1, 0)) == source.index(1, 0));
    CHECK(proxy.mapFromSource(source.index(0, 0)) == proxy.index(0, 0));

    int inserted = 0;
    QObject::connect(&proxy, &QAbstractItemModel::rowsInserted, [&] { ++inserted; });
    QPersistentModelIndex two = proxy.index(1, 0);
    source.insertRows(0, 1);
    CHECK(inserted == 1);
    CHECK(proxy.rowCount() == 3);
    CHECK(two.row() == 2);

    CHECK(proxy.setData(proxy.index(0, 0), u"zero"_s));
    CHECK(source.stringList().first() == u"zero"_s);
    source.sort(0);
    CHECK(two.data().toString() == u"two"_s);
    CHECK(two.row() == 1);

    CHECK(proxy.removeRows(0, 1));
    CHECK(source.rowCount() == 2);

    {
        auto temporary = std::make_unique<QStringListModel>(QStringList{u"x"_s});
        proxy.setSourceModel(temporary.get());
        CHECK(proxy.rowCount() == 1);
    }
    CHECK(proxy.sourceModel() == nullptr);
    CHECK(proxy.rowCount() == 0);
}

static void testSortFilterProxy() {
    QStringListModel source({u"pear"_s, u"Apple"_s, u"banana"_s, u"cherry"_s, u"apricot"_s});
    QSortFilterProxyModel proxy;
    proxy.setSourceModel(&source);
    CHECK(column0(proxy) == source.stringList());

    proxy.sort(0);
    CHECK(column0(proxy) == QStringList({u"Apple"_s, u"apricot"_s, u"banana"_s, u"cherry"_s, u"pear"_s}));
    CHECK(proxy.sortColumn() == 0 && proxy.sortOrder() == Qt::AscendingOrder);
    proxy.setSortCaseSensitivity(Qt::CaseInsensitive);
    CHECK(proxy.sortCaseSensitivity() == Qt::CaseInsensitive);
    CHECK(column0(proxy).first() == u"Apple"_s);

    proxy.sort(0, Qt::DescendingOrder);
    CHECK(column0(proxy).first() == u"pear"_s);

    QPersistentModelIndex pear = proxy.index(0, 0);
    proxy.setFilterFixedString(u"ap"_s);
    proxy.setFilterCaseSensitivity(Qt::CaseInsensitive);
    CHECK(column0(proxy) == QStringList({u"apricot"_s, u"Apple"_s}));
    CHECK(!pear.isValid());

    proxy.setFilterCaseSensitivity(Qt::CaseSensitive);
    CHECK(column0(proxy) == QStringList({u"apricot"_s}));

    proxy.setFilterRegularExpression(u"^[bc]"_s);
    CHECK(column0(proxy) == QStringList({u"cherry"_s, u"banana"_s}));
    QPersistentModelIndex cherry = proxy.index(0, 0);
    CHECK(proxy.mapToSource(cherry) == source.index(3, 0));
    CHECK(proxy.mapFromSource(source.index(2, 0)) == proxy.index(1, 0));
    CHECK(!proxy.mapFromSource(source.index(0, 0)).isValid());

    // Dynamic sorting and filtering follow source edits.
    source.setData(source.index(0, 0), u"date"_s);
    CHECK(column0(proxy) == QStringList({u"cherry"_s, u"banana"_s}));
    source.setData(source.index(0, 0), u"coconut"_s);
    CHECK(column0(proxy) == QStringList({u"coconut"_s, u"cherry"_s, u"banana"_s}));
    proxy.setFilterWildcard(u"*an*"_s);
    CHECK(column0(proxy) == QStringList({u"banana"_s}));

    source.insertRows(0, 1);
    source.setData(source.index(0, 0), u"mango"_s);
    CHECK(column0(proxy) == QStringList({u"mango"_s, u"banana"_s}));

    proxy.setFilterFixedString(QString());
    proxy.sort(-1);
    CHECK(column0(proxy) == source.stringList());

    // Editing through the proxy reaches the source.
    CHECK(proxy.setData(proxy.index(0, 0), u"kiwi"_s));
    CHECK(source.stringList().first() == u"kiwi"_s);
    CHECK(proxy.removeRows(0, 1));
    CHECK(source.rowCount() == 5);
}

static void testSortFilterRecursive() {
    TreeModel tree;
    Node *fruit = tree.root.add(u"fruit"_s);
    fruit->add(u"apple"_s);
    Node *berries = fruit->add(u"berries"_s);
    berries->add(u"strawberry"_s);
    berries->add(u"blueberry"_s);
    Node *veg = tree.root.add(u"vegetables"_s);
    veg->add(u"carrot"_s);

    QSortFilterProxyModel proxy;
    proxy.setSourceModel(&tree);
    CHECK(proxy.rowCount() == 2);
    const QModelIndex pFruit = proxy.index(0, 0);
    CHECK(proxy.rowCount(pFruit) == 2);
    CHECK(proxy.parent(proxy.index(1, 0, pFruit)) == pFruit);

    proxy.setFilterFixedString(u"blue"_s);
    CHECK(proxy.rowCount() == 0);
    proxy.setRecursiveFilteringEnabled(true);
    CHECK(proxy.rowCount() == 1);
    const QModelIndex f = proxy.index(0, 0);
    CHECK(f.data().toString() == u"fruit"_s);
    CHECK(column0(proxy, f) == QStringList({u"berries"_s}));
    CHECK(column0(proxy, proxy.index(0, 0, f)) == QStringList({u"blueberry"_s}));

    proxy.setFilterFixedString(u"berries"_s);
    CHECK(proxy.rowCount(proxy.index(0, 0, proxy.index(0, 0))) == 0);
    proxy.setAutoAcceptChildRows(true);
    CHECK(proxy.rowCount(proxy.index(0, 0, proxy.index(0, 0))) == 2);

    proxy.setFilterFixedString(QString());
    proxy.sort(0, Qt::DescendingOrder);
    CHECK(column0(proxy) == QStringList({u"vegetables"_s, u"fruit"_s}));
    CHECK(column0(proxy, proxy.index(1, 0)) == QStringList({u"berries"_s, u"apple"_s}));
}

static void testTransposeProxy() {
    Table table(2, 3, 100);
    QTransposeProxyModel proxy;
    proxy.setSourceModel(&table);
    CHECK(proxy.rowCount() == 3 && proxy.columnCount() == 2);
    CHECK(proxy.index(2, 1).data().toInt() == 112);
    CHECK(proxy.headerData(1, Qt::Vertical).toString() == u"C1"_s);
    CHECK(proxy.headerData(1, Qt::Horizontal).toInt() == 101);
    CHECK(proxy.mapToSource(proxy.index(2, 1)) == table.index(1, 2));

    int inserted = 0;
    QObject::connect(&proxy, &QAbstractItemModel::columnsInserted, [&] { ++inserted; });
    table.appendRow();
    CHECK(inserted == 1);
    CHECK(proxy.columnCount() == 3);
    CHECK(proxy.index(0, 2).data().toInt() == 120);
}

static void testConcatenateProxy() {
    Table a(2, 3, 0);
    Table b(3, 2, 1000);
    QConcatenateTablesProxyModel proxy;
    proxy.addSourceModel(&a);
    CHECK(proxy.rowCount() == 2 && proxy.columnCount() == 3);
    proxy.addSourceModel(&b);
    CHECK(proxy.sourceModels().size() == 2);
    CHECK(proxy.rowCount() == 5 && proxy.columnCount() == 2);
    CHECK(proxy.index(1, 1).data().toInt() == 11);
    CHECK(proxy.index(2, 0).data().toInt() == 1000);
    CHECK(proxy.index(4, 1).data().toInt() == 1021);
    CHECK(proxy.headerData(0, Qt::Horizontal).toString() == u"C0"_s);
    CHECK(proxy.headerData(3, Qt::Vertical).toInt() == 1001);
    CHECK(proxy.mapToSource(proxy.index(3, 1)) == b.index(1, 1));
    CHECK(proxy.mapFromSource(b.index(2, 0)) == proxy.index(4, 0));

    QPersistentModelIndex last = proxy.index(4, 0);
    int inserted = -1;
    QObject::connect(&proxy, &QAbstractItemModel::rowsInserted,
                     [&](const QModelIndex &, int first) { inserted = first; });
    a.appendRow();
    CHECK(inserted == 2);
    CHECK(proxy.rowCount() == 6);
    CHECK(last.row() == 5);
    CHECK(last.data().toInt() == 1020);

    proxy.removeSourceModel(&a);
    CHECK(proxy.rowCount() == 3);
    CHECK(proxy.index(0, 0).data().toInt() == 1000);
    {
        Table c(1, 2, 5000);
        proxy.addSourceModel(&c);
        CHECK(proxy.rowCount() == 4);
    }
    CHECK(proxy.rowCount() == 3);
}

static void testPersistentAndMime() {
    QStringListModel model({u"a"_s, u"b"_s});
    QPersistentModelIndex p(model.index(1, 0));
    QModelIndex plain = p;
    CHECK(plain == model.index(1, 0));
    CHECK(p == model.index(1, 0));
    model.setStringList({u"x"_s});
    CHECK(!p.isValid());

    QMimeData mime;
    mime.setText(u"hello"_s);
    CHECK(mime.hasText() && mime.text() == u"hello"_s);
    mime.setData(u"application/x-test"_s, QByteArray("raw"));
    CHECK(mime.hasFormat(u"application/x-test"_s));
    CHECK(mime.data(u"application/x-test"_s) == QByteArray("raw"));
    CHECK(mime.formats().size() == 2);
    mime.removeFormat(u"text/plain"_s);
    CHECK(!mime.hasText());

    std::unique_ptr<QMimeData> dragged(model.mimeData({model.index(0, 0)}));
    CHECK(dragged != nullptr);
    CHECK(dragged && dragged->hasFormat(model.mimeTypes().first()));
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testStringListModel();
    testSelectionModel();
    testIdentityProxy();
    testSortFilterProxy();
    testSortFilterRecursive();
    testTransposeProxy();
    testConcatenateProxy();
    testPersistentAndMime();
    if (g_failures == 0)
        std::printf("models: all tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
