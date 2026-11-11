// MIT License - see LICENSE
//
// Tests for the model/view classes of Nt Widgets on the offscreen platform: QTreeView with models of Nt Core,
// the delegates and their editors, sorting from the header, and QTreeWidget. The tests play the window system
// through QWindowSystemInterface.
#include <QtCore>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qdatetimeedit.h>
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qspinbox.h>
#include <QtWidgets/qstyleditemdelegate.h>
#include <QtWidgets/qtreeview.h>
#include <QtWidgets/qtreewidget.h>

#include "platforms/offscreen/qoffscreenintegration_p.h"
#include "qpa/qwindowsysteminterface_p.h"

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

namespace {

void settle(QWidget *w) {
    for (int i = 0; i < 3; ++i) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::sendPostedEvents(w);
        if (w->windowHandle())
            QCoreApplication::sendPostedEvents(w->windowHandle());
    }
}

QImage grab(QWidget *w) {
    settle(w->window());
    return QOffscreenBackingStore::imageOf(w->window()->windowHandle());
}

QColor colorAt(const QImage &image, QPoint p) { return image.pixelColor(p.x(), p.y()); }

void mouse(QWidget *w, QEvent::Type type, QPoint local, Qt::MouseButton button = Qt::LeftButton,
           Qt::MouseButtons buttons = Qt::LeftButton, Qt::KeyboardModifiers mods = Qt::NoModifier) {
    QWindow *win = w->window()->windowHandle();
    const QPoint inWindow = w->mapTo(w->window(), local);
    QWindowSystemInterface::handleMouseEvent(win, QPointF(inWindow), QPointF(inWindow + w->window()->pos()), buttons,
                                             button, type, mods);
}

void click(QWidget *w, QPoint local, Qt::KeyboardModifiers mods = Qt::NoModifier) {
    mouse(w, QEvent::MouseButtonPress, local, Qt::LeftButton, Qt::LeftButton, mods);
    mouse(w, QEvent::MouseButtonRelease, local, Qt::LeftButton, Qt::NoButton, mods);
}

void doubleClick(QWidget *w, QPoint local) {
    click(w, local);
    mouse(w, QEvent::MouseButtonDblClick, local, Qt::LeftButton, Qt::LeftButton);
    mouse(w, QEvent::MouseButtonRelease, local, Qt::LeftButton, Qt::NoButton);
}

void key(QWidget *w, int k, const QString &text = QString(), Qt::KeyboardModifiers mods = Qt::NoModifier) {
    QWindow *win = w->window()->windowHandle();
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyPress, k, mods, text);
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyRelease, k, mods);
}

void type(QWidget *w, const QString &text) {
    for (QChar c : text)
        key(w, c.unicode() >= 'a' && c.unicode() <= 'z' ? c.unicode() - 32 : c.unicode(), QString(c));
}

// A tree of nodes with two columns: a name, which can be edited, and a value of any type. A node can have a
// check box in the first column.
class NodeModel : public QAbstractItemModel {
public:
    struct Node {
        QString name;
        QVariant value;
        Node *parent = nullptr;
        std::vector<std::unique_ptr<Node>> children;
        int check = -1; // -1: no check box
        bool editable = true;
        bool enabled = true;
    };

    NodeModel() { m_root = std::make_unique<Node>(); }

    Node *add(Node *parent, const QString &name, const QVariant &value = QVariant()) {
        Node *p = parent ? parent : m_root.get();
        const int row = static_cast<int>(p->children.size());
        beginInsertRows(indexOf(p), row, row);
        p->children.push_back(std::make_unique<Node>());
        Node *n = p->children.back().get();
        n->name = name;
        n->value = value;
        n->parent = p;
        endInsertRows();
        return n;
    }
    void remove(Node *node) {
        Node *p = node->parent;
        const int row = rowOf(node);
        beginRemoveRows(indexOf(p), row, row);
        p->children.erase(p->children.begin() + row);
        endRemoveRows();
    }
    Node *node(const QModelIndex &index) const {
        return index.isValid() ? static_cast<Node *>(index.internalPointer()) : m_root.get();
    }
    int rowOf(const Node *n) const {
        const auto &siblings = n->parent->children;
        for (std::size_t i = 0; i < siblings.size(); ++i)
            if (siblings[i].get() == n)
                return static_cast<int>(i);
        return -1;
    }
    QModelIndex indexOf(const Node *n, int column = 0) const {
        return !n || n == m_root.get() ? QModelIndex() : createIndex(rowOf(n), column, n);
    }
    void touch(Node *n) { emit dataChanged(indexOf(n, 0), indexOf(n, 1)); }

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override {
        const Node *p = node(parent);
        if (row < 0 || column < 0 || column > 1 || row >= static_cast<int>(p->children.size()))
            return QModelIndex();
        return createIndex(row, column, p->children[static_cast<std::size_t>(row)].get());
    }
    QModelIndex parent(const QModelIndex &child) const override {
        return child.isValid() ? indexOf(node(child)->parent) : QModelIndex();
    }
    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        return parent.column() > 0 ? 0 : static_cast<int>(node(parent)->children.size());
    }
    int columnCount(const QModelIndex & = QModelIndex()) const override { return 2; }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid())
            return QVariant();
        const Node *n = node(index);
        if (role == Qt::DisplayRole || role == Qt::EditRole)
            return index.column() == 0 ? QVariant(n->name) : n->value;
        if (role == Qt::CheckStateRole && index.column() == 0 && n->check >= 0)
            return n->check;
        if (role == Qt::ToolTipRole)
            return QString("tip of ") + n->name;
        return QVariant();
    }
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override {
        if (!index.isValid())
            return false;
        Node *n = node(index);
        if (role == Qt::CheckStateRole) {
            n->check = value.toInt();
        } else if (role == Qt::EditRole) {
            if (index.column() == 0)
                n->name = value.toString();
            else
                n->value = value;
        } else {
            return false;
        }
        ++sets;
        emit dataChanged(index, index, {role});
        return true;
    }
    Qt::ItemFlags flags(const QModelIndex &index) const override {
        if (!index.isValid())
            return Qt::ItemFlags();
        const Node *n = node(index);
        Qt::ItemFlags f = Qt::ItemIsSelectable;
        if (n->enabled)
            f |= Qt::ItemIsEnabled;
        if (n->editable)
            f |= Qt::ItemIsEditable;
        if (n->check >= 0)
            f |= Qt::ItemIsUserCheckable;
        return f;
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
            return QVariant();
        return section == 0 ? QString("Name") : QString("Value");
    }

    int sets = 0;

private:
    std::unique_ptr<Node> m_root;
};

struct ViewSignals {
    explicit ViewSignals(QAbstractItemView *view) {
        QObject::connect(view, &QAbstractItemView::pressed, [this](const QModelIndex &i) { log << "pressed:" + i.data().toString(); });
        QObject::connect(view, &QAbstractItemView::clicked, [this](const QModelIndex &i) { log << "clicked:" + i.data().toString(); });
        QObject::connect(view, &QAbstractItemView::doubleClicked,
                         [this](const QModelIndex &i) { log << "double:" + i.data().toString(); });
        QObject::connect(view, &QAbstractItemView::activated,
                         [this](const QModelIndex &i) { log << "activated:" + i.data().toString(); });
    }
    QStringList log;
};

QStringList rowsShown(const QTreeView &view) {
    QStringList names;
    for (QModelIndex i = view.indexAt(QPoint(view.header()->sectionSize(0) - 2, 2)); i.isValid(); i = view.indexBelow(i))
        names << i.sibling(i.row(), 0).data().toString();
    return names;
}

QStringList selectedNames(const QAbstractItemView &view) {
    QStringList names;
    for (const QModelIndex &i : view.selectionModel()->selectedRows())
        names << i.data().toString();
    names.sort();
    return names;
}

template <typename T> T *editorOf(QAbstractItemView &view) { return view.viewport()->findChild<T *>(); }

} // namespace

static void testFlatModel() {
    QStringListModel model;
    QStringList words;
    for (int i = 0; i < 100; ++i)
        words << QString("row %1").arg(i, 3, 10, QChar('0'));
    model.setStringList(words);

    QTreeView view;
    CHECK(view.model() == nullptr && view.selectionModel() == nullptr && !view.currentIndex().isValid());
    CHECK(view.selectionBehavior() == QAbstractItemView::SelectRows && view.selectionMode() == QAbstractItemView::SingleSelection);
    CHECK(view.itemDelegate() != nullptr && !view.indexAt(QPoint(5, 5)).isValid());
    view.setRootIsDecorated(false);
    view.resize(300, 220);
    view.setModel(&model);
    CHECK(view.model() == &model && view.selectionModel() != nullptr && view.selectionModel()->model() == &model);
    CHECK(view.header()->count() == 1 && !view.isHeaderHidden());
    view.show();
    settle(&view);
    ViewSignals spy(&view);

    // The rows are laid out below the header, one under the other, with the same height.
    const QModelIndex first = model.index(0);
    const QRect r0 = view.visualRect(first);
    const QRect r1 = view.visualRect(model.index(1));
    CHECK(r0.top() == 0 && r0.left() == 0 && r0.height() >= 16 && r1.top() == r0.bottom() + 1 && r1.height() == r0.height());
    CHECK(view.indexAt(r1.center()) == model.index(1) && view.indexBelow(first) == model.index(1));
    CHECK(view.indexAbove(model.index(1)) == first && !view.indexAbove(first).isValid());
    CHECK(view.viewport()->y() >= view.header()->height() && view.header()->height() > 0);
    CHECK(view.verticalScrollBar()->maximum() == 100 * r0.height() - view.viewport()->height());
    CHECK(!view.visualRect(QModelIndex()).isValid() || view.visualRect(QModelIndex()).isEmpty());

    // A click makes an item current and selects its row.
    click(view.viewport(), r1.center());
    CHECK(view.currentIndex() == model.index(1) && selectedNames(view) == QStringList{"row 001"});
    CHECK(spy.log == QStringList({"pressed:row 001", "clicked:row 001"}));
    spy.log.clear();

    // The keyboard.
    view.setFocus();
    key(&view, Qt::Key_Down);
    CHECK(view.currentIndex().row() == 2 && selectedNames(view) == QStringList{"row 002"});
    key(&view, Qt::Key_Up);
    key(&view, Qt::Key_Up);
    key(&view, Qt::Key_Up);
    CHECK(view.currentIndex().row() == 0);
    key(&view, Qt::Key_End);
    CHECK(view.currentIndex().row() == 99 && view.verticalScrollBar()->value() == view.verticalScrollBar()->maximum());
    CHECK(view.viewport()->rect().contains(view.visualRect(view.currentIndex())));
    key(&view, Qt::Key_PageUp);
    const int page = 99 - view.currentIndex().row();
    CHECK(page >= 3 && page <= view.viewport()->height() / r0.height());
    key(&view, Qt::Key_PageDown);
    key(&view, Qt::Key_Home);
    CHECK(view.currentIndex().row() == 0 && view.verticalScrollBar()->value() == 0);
    key(&view, Qt::Key_Return);
    CHECK(spy.log == QStringList{"activated:row 000"});
    spy.log.clear();

    // scrollTo() and the hints.
    view.scrollTo(model.index(50), QAbstractItemView::PositionAtTop);
    CHECK(view.visualRect(model.index(50)).top() == 0);
    view.scrollTo(model.index(50), QAbstractItemView::PositionAtBottom);
    CHECK(view.visualRect(model.index(50)).bottom() == view.viewport()->height() - 1);
    view.scrollTo(model.index(50), QAbstractItemView::PositionAtCenter);
    CHECK(qAbs(view.visualRect(model.index(50)).center().y() - view.viewport()->height() / 2) <= r0.height());
    const int kept = view.verticalScrollBar()->value();
    view.scrollTo(model.index(50));
    CHECK(view.verticalScrollBar()->value() == kept);
    view.scrollToBottom();
    CHECK(view.indexAt(QPoint(5, view.viewport()->height() - 2)).row() == 99);
    view.scrollToTop();
    CHECK(view.indexAt(QPoint(5, 2)).row() == 0);

    // Selection modes.
    view.setSelectionMode(QAbstractItemView::ExtendedSelection);
    click(view.viewport(), view.visualRect(model.index(1)).center());
    click(view.viewport(), view.visualRect(model.index(4)).center(), Qt::ShiftModifier);
    CHECK(selectedNames(view) == QStringList({"row 001", "row 002", "row 003", "row 004"}) && view.currentIndex().row() == 4);
    click(view.viewport(), view.visualRect(model.index(2)).center(), Qt::ControlModifier);
    CHECK(selectedNames(view) == QStringList({"row 001", "row 003", "row 004"}));
    click(view.viewport(), view.visualRect(model.index(6)).center(), Qt::ControlModifier);
    CHECK(selectedNames(view).size() == 4 && view.selectionModel()->isSelected(model.index(6)));
    key(&view, Qt::Key_Down, QString(), Qt::ShiftModifier);
    CHECK(selectedNames(view) == QStringList({"row 006", "row 007"}));
    key(&view, Qt::Key_A, "a", Qt::ControlModifier);
    CHECK(selectedNames(view).size() == 100);
    click(view.viewport(), view.visualRect(model.index(3)).center());
    CHECK(selectedNames(view) == QStringList{"row 003"});
    view.clearSelection();
    CHECK(selectedNames(view).isEmpty() && view.currentIndex().row() == 3);
    view.setSelectionMode(QAbstractItemView::MultiSelection);
    click(view.viewport(), view.visualRect(model.index(1)).center());
    click(view.viewport(), view.visualRect(model.index(3)).center());
    click(view.viewport(), view.visualRect(model.index(1)).center());
    CHECK(selectedNames(view) == QStringList{"row 003"});
    key(&view, Qt::Key_Down);
    CHECK(selectedNames(view) == QStringList{"row 003"} && view.currentIndex().row() == 2);
    key(&view, Qt::Key_Space, " ");
    CHECK(selectedNames(view) == QStringList({"row 002", "row 003"}));
    view.setSelectionMode(QAbstractItemView::NoSelection);
    CHECK(selectedNames(view).isEmpty());
    click(view.viewport(), view.visualRect(model.index(5)).center());
    CHECK(selectedNames(view).isEmpty() && view.currentIndex().row() == 5);
    view.selectAll();
    CHECK(selectedNames(view).isEmpty());
    view.setSelectionMode(QAbstractItemView::SingleSelection);
    view.setCurrentIndex(model.index(7));
    CHECK(selectedNames(view) == QStringList{"row 007"});
    // A click below the last row deselects.
    model.setStringList({"a", "b", "c"});
    settle(&view);
    CHECK(view.verticalScrollBar()->maximum() == 0 && !view.currentIndex().isValid());
    view.setCurrentIndex(model.index(1));
    click(view.viewport(), QPoint(20, view.viewport()->height() - 5));
    CHECK(selectedNames(view).isEmpty());

    // The model changes under the view.
    model.insertRows(1, 2);
    model.setData(model.index(1), "new");
    CHECK(rowsShown(view) == QStringList({"a", "new", "", "b", "c"}));
    view.setCurrentIndex(model.index(4));
    model.removeRows(3, 2);
    CHECK(rowsShown(view) == QStringList({"a", "new", ""}) && view.currentIndex().row() == 2);
    model.removeRows(0, 3);
    CHECK(rowsShown(view).isEmpty() && !view.currentIndex().isValid());

    // Another model, and none.
    QStringListModel other({"x", "y"});
    QItemSelectionModel *oldSelection = view.selectionModel();
    view.setModel(&other);
    CHECK(view.selectionModel() != oldSelection && view.selectionModel()->model() == &other && rowsShown(view) == QStringList({"x", "y"}));
    view.setModel(nullptr);
    CHECK(view.model() == nullptr && view.selectionModel() == nullptr && rowsShown(view).isEmpty() && view.header()->count() == 0);
    key(&view, Qt::Key_Down);
    click(view.viewport(), QPoint(10, 10));
    // A view outlives its model.
    {
        QStringListModel shortLived({"gone"});
        view.setModel(&shortLived);
        CHECK(rowsShown(view) == QStringList{"gone"});
    }
    CHECK(view.model() == nullptr);
    settle(&view);
    grab(&view);
}

static void testTree() {
    NodeModel model;
    auto *fruit = model.add(nullptr, "Fruit");
    auto *apple = model.add(fruit, "Apple", 3);
    model.add(fruit, "Pear", 5);
    auto *veg = model.add(nullptr, "Vegetables");
    auto *roots = model.add(veg, "Roots");
    auto *carrot = model.add(roots, "Carrot", 1);
    model.add(nullptr, "Empty");

    QTreeView view;
    view.resize(320, 240);
    view.setModel(&model);
    view.show();
    settle(&view);
    QStringList expandedLog;
    QObject::connect(&view, &QTreeView::expanded, [&](const QModelIndex &i) { expandedLog << "+" + i.data().toString(); });
    QObject::connect(&view, &QTreeView::collapsed, [&](const QModelIndex &i) { expandedLog << "-" + i.data().toString(); });

    CHECK(view.header()->count() == 2 && view.indentation() == 20 && view.rootIsDecorated() && view.itemsExpandable());
    CHECK(rowsShown(view) == QStringList({"Fruit", "Vegetables", "Empty"}));
    const QModelIndex fruitIndex = model.indexOf(fruit);
    CHECK(!view.isExpanded(fruitIndex) && view.visualRect(model.indexOf(apple)).isEmpty());
    // The first column starts after the room of the arrow; the second where the header says.
    CHECK(view.visualRect(fruitIndex).left() == 20 && view.visualRect(model.indexOf(fruit, 1)).left() == view.header()->sectionSize(0));
    CHECK(view.columnAt(5) == 0 && view.columnAt(view.header()->sectionSize(0) + 5) == 1 && view.columnViewportPosition(1) == view.columnWidth(0));

    view.expand(fruitIndex);
    CHECK(view.isExpanded(fruitIndex) && rowsShown(view) == QStringList({"Fruit", "Apple", "Pear", "Vegetables", "Empty"}));
    CHECK(view.visualRect(model.indexOf(apple)).left() == 40 && expandedLog == QStringList{"+Fruit"});
    view.expand(fruitIndex);
    view.expand(model.indexOf(apple)); // no children
    view.expand(QModelIndex());
    CHECK(expandedLog.size() == 1 && !view.isExpanded(model.indexOf(apple)));
    view.collapse(fruitIndex);
    view.collapse(fruitIndex);
    CHECK(expandedLog == QStringList({"+Fruit", "-Fruit"}) && rowsShown(view).size() == 3);
    expandedLog.clear();

    // The arrow, the double click and the keys.
    const QRect fruitRect = view.visualRect(fruitIndex);
    click(view.viewport(), QPoint(fruitRect.left() - 10, fruitRect.center().y()));
    CHECK(view.isExpanded(fruitIndex) && !view.currentIndex().isValid());
    click(view.viewport(), QPoint(fruitRect.left() - 10, fruitRect.center().y()));
    CHECK(!view.isExpanded(fruitIndex));
    view.setEditTriggers(QAbstractItemView::NoEditTriggers);
    doubleClick(view.viewport(), fruitRect.center());
    CHECK(view.isExpanded(fruitIndex) && view.currentIndex() == fruitIndex);
    view.setExpandsOnDoubleClick(false);
    doubleClick(view.viewport(), fruitRect.center());
    CHECK(view.isExpanded(fruitIndex));
    view.setExpandsOnDoubleClick(true);
    view.setFocus();
    key(&view, Qt::Key_Left);
    CHECK(!view.isExpanded(fruitIndex) && view.currentIndex() == fruitIndex);
    key(&view, Qt::Key_Right);
    CHECK(view.isExpanded(fruitIndex) && view.currentIndex() == fruitIndex);
    key(&view, Qt::Key_Right);
    CHECK(view.currentIndex() == model.indexOf(apple));
    key(&view, Qt::Key_Right); // a leaf: stays
    CHECK(view.currentIndex() == model.indexOf(apple));
    key(&view, Qt::Key_Left);
    CHECK(view.currentIndex() == fruitIndex && view.isExpanded(fruitIndex));
    key(&view, Qt::Key_Minus, "-");
    CHECK(!view.isExpanded(fruitIndex));
    key(&view, Qt::Key_Plus, "+");
    CHECK(view.isExpanded(fruitIndex));
    view.setItemsExpandable(false);
    key(&view, Qt::Key_Left);
    click(view.viewport(), QPoint(fruitRect.left() - 10, fruitRect.center().y()));
    CHECK(view.isExpanded(fruitIndex));
    view.setItemsExpandable(true);
    view.setCurrentIndex(model.indexOf(veg));
    key(&view, 0x2a, "*");
    CHECK(view.isExpanded(model.indexOf(veg)) && view.isExpanded(model.indexOf(roots)));
    expandedLog.clear();

    // All at once, and to a depth.
    view.collapseAll();
    CHECK(rowsShown(view).size() == 3 && expandedLog.size() == 3 && view.currentIndex() == model.indexOf(veg));
    view.expandAll();
    CHECK(rowsShown(view) == QStringList({"Fruit", "Apple", "Pear", "Vegetables", "Roots", "Carrot", "Empty"}));
    view.collapseAll();
    view.expandToDepth(0);
    CHECK(rowsShown(view) == QStringList({"Fruit", "Apple", "Pear", "Vegetables", "Roots", "Empty"}));
    view.collapseAll();
    view.expandRecursively(model.indexOf(veg), 0);
    CHECK(rowsShown(view) == QStringList({"Fruit", "Vegetables", "Roots", "Empty"}));
    view.expandRecursively(model.indexOf(veg));
    CHECK(view.isExpanded(model.indexOf(roots)));
    view.collapseAll();

    // scrollTo() and setCurrentIndex() open what is above an item.
    view.setCurrentIndex(model.indexOf(carrot));
    CHECK(view.isExpanded(model.indexOf(veg)) && view.isExpanded(model.indexOf(roots)) && !view.visualRect(model.indexOf(carrot)).isEmpty());
    CHECK(view.visualRect(model.indexOf(carrot)).left() == 60);
    // Collapsing above the current item moves it up to what still shows.
    view.collapse(model.indexOf(veg));
    CHECK(view.currentIndex() == model.indexOf(veg));

    // Rows come and go; what was expanded stays expanded.
    view.expandAll();
    auto *plum = model.add(fruit, "Plum", 9);
    CHECK(rowsShown(view) == QStringList({"Fruit", "Apple", "Pear", "Plum", "Vegetables", "Roots", "Carrot", "Empty"}));
    view.setCurrentIndex(model.indexOf(plum));
    model.remove(plum);
    CHECK(view.currentIndex().data().toString() == "Pear" && rowsShown(view).size() == 7);
    view.setCurrentIndex(model.indexOf(carrot));
    model.remove(roots);
    CHECK(view.currentIndex() == model.indexOf(veg) && rowsShown(view) == QStringList({"Fruit", "Apple", "Pear", "Vegetables", "Empty"}));
    auto *leaf = model.add(veg, "Leaf");
    CHECK(rowsShown(view).contains("Leaf")); // Vegetables was expanded before it lost its children
    model.remove(leaf);

    // A root other than the top of the model.
    view.setRootIndex(fruitIndex);
    CHECK(view.rootIndex() == fruitIndex && rowsShown(view) == QStringList({"Apple", "Pear"}));
    CHECK(view.visualRect(model.indexOf(apple)).left() == 20);
    view.setRootIndex(QModelIndex());
    view.setRootIsDecorated(false);
    CHECK(view.visualRect(fruitIndex).left() == 0 && view.visualRect(model.indexOf(apple)).left() == 20);
    view.setRootIsDecorated(true);
    view.setIndentation(30);
    CHECK(view.visualRect(model.indexOf(apple)).left() == 60);
    view.resetIndentation();
    CHECK(view.indentation() == 20);

    // Columns.
    view.setColumnWidth(0, 150);
    CHECK(view.columnWidth(0) == 150 && view.visualRect(model.indexOf(fruit, 1)).left() == 150);
    view.resizeColumnToContents(0);
    CHECK(view.columnWidth(0) < 150 && view.columnWidth(0) > 40);
    view.setHeaderHidden(true);
    settle(&view);
    CHECK(view.isHeaderHidden() && view.viewport()->y() <= 2);
    view.setHeaderHidden(false);
    settle(&view);
    CHECK(view.viewport()->y() >= view.header()->height());

    // Tool tips of the items, and reset().
    view.reset();
    CHECK(rowsShown(view).size() == 3 && !view.isExpanded(fruitIndex));
    view.setUniformRowHeights(true);
    CHECK(view.uniformRowHeights());
    grab(&view);
}

static void testEditing() {
    NodeModel model;
    auto *text = model.add(nullptr, "text", QString("hello"));
    auto *number = model.add(nullptr, "number", 42);
    auto *real = model.add(nullptr, "real", 2.5);
    auto *flag = model.add(nullptr, "flag", true);
    auto *date = model.add(nullptr, "date", QDate(2024, 2, 29));
    auto *time = model.add(nullptr, "time", QTime(13, 14));
    auto *stamp = model.add(nullptr, "stamp", QDateTime(QDate(2024, 2, 29), QTime(13, 14)));
    auto *fixed = model.add(nullptr, "fixed", QString("no"));
    fixed->editable = false;
    auto *off = model.add(nullptr, "off", QString("disabled"));
    off->enabled = false;
    auto *checked = model.add(nullptr, "checked", QString("box"));
    checked->check = Qt::Unchecked;

    QTreeView view;
    view.setRootIsDecorated(false);
    view.resize(420, 320);
    view.setModel(&model);
    view.setColumnWidth(0, 160);
    view.show();
    settle(&view);
    view.setFocus();
    ViewSignals spy(&view);
    const auto cell = [&](NodeModel::Node *n, int column) { return model.indexOf(n, column); };
    const auto center = [&](NodeModel::Node *n, int column) { return view.visualRect(cell(n, column)).center(); };

    const QAbstractItemView::EditTriggers usual = view.editTriggers();
    // A double click opens an editor of the kind that fits the value, over the item.
    CHECK(view.editTriggers() == (QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed));
    doubleClick(view.viewport(), center(text, 1));
    auto *line = editorOf<QLineEdit>(view);
    CHECK(line && line->text() == "hello" && line->hasFocus() && line->isVisible());
    CHECK(line && view.visualRect(cell(text, 1)).contains(line->geometry().center()));
    CHECK(spy.log.contains("double:hello") && !spy.log.contains("activated:hello"));
    // Typing replaces what was there; Return stores it.
    type(&view, "world");
    key(&view, Qt::Key_Return);
    settle(&view);
    CHECK(text->value.toString() == "world" && editorOf<QLineEdit>(view) == nullptr && view.hasFocus() && model.sets == 1);
    // Escape gives the edit up.
    doubleClick(view.viewport(), center(text, 1));
    type(&view, "nope");
    key(&view, Qt::Key_Escape);
    settle(&view);
    CHECK(text->value.toString() == "world" && editorOf<QLineEdit>(view) == nullptr && model.sets == 1);

    // Each type has its editor, and keeps its type.
    view.edit(cell(number, 1));
    auto *spin = editorOf<QSpinBox>(view);
    CHECK(spin && spin->value() == 42);
    if (spin)
        spin->setValue(-7);
    key(&view, Qt::Key_Return);
    settle(&view);
    CHECK(number->value.typeId() == QMetaType::Int && number->value.toInt() == -7);
    view.edit(cell(real, 1));
    auto *realSpin = editorOf<QDoubleSpinBox>(view);
    CHECK(realSpin && realSpin->value() == 2.5);
    if (realSpin)
        realSpin->setValue(0.125);
    key(&view, Qt::Key_Return);
    settle(&view);
    CHECK(real->value.typeId() == QMetaType::Double && real->value.toDouble() == 0.125);
    view.edit(cell(flag, 1));
    auto *combo = editorOf<QComboBox>(view);
    CHECK(combo && combo->currentIndex() == 1);
    if (combo)
        combo->setCurrentIndex(0);
    key(&view, Qt::Key_Return);
    settle(&view);
    CHECK(flag->value.typeId() == QMetaType::Bool && !flag->value.toBool());
    view.edit(cell(date, 1));
    auto *dateEdit = editorOf<QDateEdit>(view);
    CHECK(dateEdit && dateEdit->date() == QDate(2024, 2, 29));
    if (dateEdit)
        dateEdit->setDate(QDate(2025, 1, 2));
    key(&view, Qt::Key_Return);
    settle(&view);
    CHECK(date->value.typeId() == QMetaType::QDate && date->value.toDate() == QDate(2025, 1, 2));
    view.edit(cell(time, 1));
    auto *timeEdit = editorOf<QTimeEdit>(view);
    CHECK(timeEdit && timeEdit->time() == QTime(13, 14));
    key(&view, Qt::Key_Escape);
    settle(&view);
    view.edit(cell(stamp, 1));
    CHECK(editorOf<QDateTimeEdit>(view) && editorOf<QDateTimeEdit>(view)->dateTime() == stamp->value.toDateTime());
    key(&view, Qt::Key_Escape);
    settle(&view);
    CHECK(time->value.toTime() == QTime(13, 14) && view.viewport()->findChild<QWidget *>() == nullptr);

    // What cannot be edited is not: the double click activates it instead.
    spy.log.clear();
    doubleClick(view.viewport(), center(fixed, 1));
    CHECK(view.viewport()->findChild<QWidget *>() == nullptr && spy.log.contains("activated:no"));
    view.edit(cell(fixed, 1));
    view.edit(cell(off, 1));
    view.edit(QModelIndex());
    CHECK(view.viewport()->findChild<QWidget *>() == nullptr);
    click(view.viewport(), center(off, 0)); // a disabled item is not even selected
    CHECK(view.currentIndex() != cell(off, 0));

    // F2 and typing on the current item.
    view.setCurrentIndex(cell(text, 0));
    key(&view, Qt::Key_F2);
    line = editorOf<QLineEdit>(view);
    CHECK(line && line->text() == "text");
    key(&view, Qt::Key_Escape);
    settle(&view);
    view.setCurrentIndex(cell(text, 1));
    type(&view, "X");
    line = editorOf<QLineEdit>(view);
    CHECK(line && line->text() == "X");
    type(&view, "yz");
    CHECK(line && line->text() == "Xyz");
    // Tab stores and goes on to the next item, Shift+Tab back.
    key(&view, Qt::Key_Tab);
    settle(&view);
    CHECK(text->value.toString() == "Xyz" && view.currentIndex() == cell(number, 0));
    line = editorOf<QLineEdit>(view);
    CHECK(line && line->text() == "number");
    key(&view, Qt::Key_Backtab, QString(), Qt::ShiftModifier);
    settle(&view);
    CHECK(view.currentIndex() == cell(text, 1) && editorOf<QLineEdit>(view) && editorOf<QLineEdit>(view)->text() == "Xyz");
    // Another item becoming current stores the value and closes the editor.
    type(&view, "kept");
    click(view.viewport(), center(real, 0));
    settle(&view);
    CHECK(text->value.toString() == "kept" && view.viewport()->findChild<QWidget *>() == nullptr && view.currentIndex() == cell(real, 0));

    // The triggers.
    view.setEditTriggers(QAbstractItemView::NoEditTriggers);
    doubleClick(view.viewport(), center(text, 1));
    key(&view, Qt::Key_F2);
    type(&view, "q");
    CHECK(view.viewport()->findChild<QWidget *>() == nullptr && text->value.toString() == "kept");
    view.edit(cell(text, 1)); // the program still can
    CHECK(editorOf<QLineEdit>(view));
    key(&view, Qt::Key_Escape);
    settle(&view);
    view.setEditTriggers(QAbstractItemView::SelectedClicked);
    click(view.viewport(), center(number, 0)); // selects
    CHECK(view.viewport()->findChild<QWidget *>() == nullptr);
    click(view.viewport(), center(number, 0)); // was selected: edits
    CHECK(editorOf<QLineEdit>(view));
    key(&view, Qt::Key_Escape);
    settle(&view);
    view.setEditTriggers(QAbstractItemView::CurrentChanged);
    view.setCurrentIndex(cell(text, 0));
    CHECK(editorOf<QLineEdit>(view) && editorOf<QLineEdit>(view)->text() == "text");
    key(&view, Qt::Key_Escape);
    settle(&view);
    view.setEditTriggers(usual);

    // The editor follows its item when the view scrolls, and goes when its row does.
    view.resize(420, 140);
    settle(&view);
    view.edit(cell(text, 1));
    line = editorOf<QLineEdit>(view);
    const int before = line ? line->y() : 0;
    view.verticalScrollBar()->setValue(view.verticalScrollBar()->value() + 10);
    CHECK(line && line->y() == before - 10);
    view.verticalScrollBar()->setValue(0);
    model.remove(text);
    settle(&view);
    CHECK(view.viewport()->findChild<QWidget *>() == nullptr);
    view.resize(420, 320);
    settle(&view);

    // A check box: a click on it toggles the item, a click beside it does not, and Space does.
    const QRect checkedRect = view.visualRect(cell(checked, 0));
    const int sets = model.sets;
    click(view.viewport(), QPoint(checkedRect.left() + 8, checkedRect.center().y()));
    CHECK(checked->check == Qt::Checked && model.sets == sets + 1 && view.viewport()->findChild<QWidget *>() == nullptr);
    click(view.viewport(), QPoint(checkedRect.right() - 5, checkedRect.center().y()));
    CHECK(checked->check == Qt::Checked);
    view.setEditTriggers(QAbstractItemView::NoEditTriggers);
    view.setCurrentIndex(cell(checked, 0));
    key(&view, Qt::Key_Space, " ");
    CHECK(checked->check == Qt::Unchecked);
    view.setEditTriggers(usual);

    // A delegate of the application for one column.
    class Stars : public QStyledItemDelegate {
    public:
        mutable int created = 0;
        QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override {
            ++created;
            auto *box = new QSpinBox(parent);
            box->setRange(0, 5);
            return box;
        }
        QString displayText(const QVariant &value, const QLocale &) const override {
            return QString(value.toInt(), QChar('*'));
        }
    };
    Stars stars;
    view.setItemDelegateForColumn(1, &stars);
    CHECK(view.itemDelegateForColumn(1) == &stars && view.itemDelegateForIndex(cell(number, 1)) == &stars);
    CHECK(view.itemDelegateForIndex(cell(number, 0)) == view.itemDelegate() && view.itemDelegateForColumn(0) == nullptr);
    number->value = 3;
    model.touch(number);
    view.edit(cell(number, 1));
    spin = editorOf<QSpinBox>(view);
    CHECK(spin && spin->maximum() == 5 && spin->value() == 3 && stars.created == 1);
    if (spin)
        spin->setValue(99);
    key(&view, Qt::Key_Return);
    settle(&view);
    CHECK(number->value.toInt() == 5);
    QStyleOptionViewItem option;
    CHECK(stars.sizeHint(option, cell(number, 1)).width() > stars.sizeHint(option, cell(checked, 1)).width() - 200);
    view.setItemDelegateForColumn(1, nullptr);
    QStyledItemDelegate rowDelegate;
    view.setItemDelegateForRow(0, &rowDelegate);
    CHECK(view.itemDelegateForRow(0) == &rowDelegate && view.itemDelegateForIndex(model.index(0, 1)) == &rowDelegate);
    view.setItemDelegateForRow(0, nullptr);
    QStyledItemDelegate whole;
    view.setItemDelegate(&whole);
    CHECK(view.itemDelegate() == &whole);
    view.setItemDelegate(nullptr);
    CHECK(view.itemDelegate() != nullptr && view.itemDelegate() != &whole);

    // What the default delegate shows.
    const QStyledItemDelegate plain;
    const QLocale c = QLocale::c();
    CHECK(plain.displayText(42, c) == "42" && plain.displayText(QString("x"), c) == "x" && plain.displayText(2.5, c) == "2.5");
    CHECK(!plain.displayText(QDate(2024, 2, 29), c).isEmpty() && plain.displayText(QVariant(), c).isEmpty());
    CHECK(view.sizeHintForIndex(cell(number, 0)).height() >= 16 && !view.sizeHintForIndex(QModelIndex()).isValid());

    // A selected row is drawn in the highlight color, and the others are not.
    view.setCurrentIndex(cell(real, 0));
    view.clearFocus();
    const QImage image = grab(&view);
    const QPoint inRow = view.viewport()->mapTo(&view, view.visualRect(cell(real, 1)).topRight() + QPoint(-3, 3));
    const QPoint inOther = view.viewport()->mapTo(&view, view.visualRect(cell(flag, 1)).topRight() + QPoint(-3, 3));
    const QPalette palette = view.palette();
    CHECK(colorAt(image, inRow) == palette.color(QPalette::Inactive, QPalette::Highlight) ||
          colorAt(image, inRow) == palette.color(QPalette::Active, QPalette::Highlight));
    CHECK(colorAt(image, inOther) == palette.color(QPalette::Active, QPalette::Base) ||
          colorAt(image, inOther) == palette.color(QPalette::Inactive, QPalette::Base));
    view.setAlternatingRowColors(true);
    const QImage striped = grab(&view);
    const QPoint oddRow = view.viewport()->mapTo(&view, view.visualRect(model.index(3, 1)).topRight() + QPoint(-3, 3));
    CHECK(colorAt(striped, oddRow) == palette.color(QPalette::Active, QPalette::AlternateBase) ||
          colorAt(striped, oddRow) == palette.color(QPalette::Inactive, QPalette::AlternateBase));
}

static void testSorting() {
    QStringListModel source({"pear", "Apple", "fig", "banana"});
    QSortFilterProxyModel proxy;
    proxy.setSourceModel(&source);
    proxy.setSortCaseSensitivity(Qt::CaseInsensitive);

    QTreeView view;
    view.setRootIsDecorated(false);
    view.resize(300, 200);
    view.setModel(&proxy);
    view.show();
    settle(&view);
    QHeaderView *header = view.header();
    QList<QPair<int, Qt::SortOrder>> changes;
    QObject::connect(header, &QHeaderView::sortIndicatorChanged,
                     [&](int section, Qt::SortOrder order) { changes.append(qMakePair(section, order)); });

    CHECK(!view.isSortingEnabled() && !header->isSortIndicatorShown() && header->sortIndicatorSection() == -1);
    CHECK(rowsShown(view) == QStringList({"pear", "Apple", "fig", "banana"}));
    // Without sorting enabled a click on the header changes nothing.
    click(header, QPoint(20, header->height() / 2));
    CHECK(rowsShown(view).first() == "pear" && changes.isEmpty());

    view.setSortingEnabled(true);
    CHECK(view.isSortingEnabled() && header->isSortIndicatorShown() && header->sectionsClickable());
    CHECK(rowsShown(view).first() == "pear"); // nothing is sorted by yet
    click(header, QPoint(20, header->height() / 2));
    CHECK(rowsShown(view) == QStringList({"Apple", "banana", "fig", "pear"}));
    CHECK(header->sortIndicatorSection() == 0 && header->sortIndicatorOrder() == Qt::AscendingOrder && changes.size() == 1);
    click(header, QPoint(20, header->height() / 2));
    CHECK(rowsShown(view) == QStringList({"pear", "fig", "banana", "Apple"}) && header->sortIndicatorOrder() == Qt::DescendingOrder);
    click(header, QPoint(20, header->height() / 2));
    CHECK(rowsShown(view).first() == "Apple" && changes.size() == 3);

    // The selection and the current item stay with their items.
    view.setCurrentIndex(proxy.index(1, 0));
    CHECK(view.currentIndex().data().toString() == "banana");
    view.sortByColumn(0, Qt::DescendingOrder);
    CHECK(view.currentIndex().data().toString() == "banana" && view.currentIndex().row() == 2);
    CHECK(selectedNames(view) == QStringList{"banana"} && header->sortIndicatorOrder() == Qt::DescendingOrder);
    // New rows are sorted in by the proxy.
    source.insertRows(0, 1);
    source.setData(source.index(0), "cherry");
    CHECK(rowsShown(view) == QStringList({"pear", "fig", "cherry", "banana", "Apple"}));

    // From the program, with sorting by the user off.
    view.setSortingEnabled(false);
    CHECK(!header->isSortIndicatorShown());
    view.sortByColumn(0, Qt::AscendingOrder);
    CHECK(rowsShown(view).first() == "Apple" && header->sortIndicatorSection() == 0);
    view.sortByColumn(-1, Qt::AscendingOrder);
    CHECK(header->sortIndicatorSection() == -1);
    header->setSortIndicator(7, Qt::AscendingOrder); // no such section
    CHECK(header->sortIndicatorSection() == -1);
    // Turning it on sorts by what the header says.
    header->setSortIndicator(0, Qt::DescendingOrder);
    view.setSortingEnabled(true);
    CHECK(rowsShown(view).first() == "pear");
    grab(&view);
}

static void testTreeWidget() {
    QTreeWidget tree;
    tree.resize(360, 260);
    CHECK(tree.columnCount() == 1 && tree.topLevelItemCount() == 0 && tree.currentItem() == nullptr && tree.currentColumn() == -1);
    CHECK(tree.invisibleRootItem() != nullptr && tree.headerItem() != nullptr && tree.selectedItems().isEmpty());
    tree.setHeaderLabels({"Name", "Count"});
    CHECK(tree.columnCount() == 2 && tree.headerItem()->text(1) == "Count" && tree.header()->count() == 2);
    CHECK(tree.model()->headerData(0, Qt::Horizontal).toString() == "Name");

    QStringList log;
    QObject::connect(&tree, &QTreeWidget::itemChanged, [&](QTreeWidgetItem *i, int c) { log << QString("changed:%1/%2").arg(i->text(0)).arg(c); });
    QObject::connect(&tree, &QTreeWidget::itemClicked, [&](QTreeWidgetItem *i, int c) { log << QString("clicked:%1/%2").arg(i->text(0)).arg(c); });
    QObject::connect(&tree, &QTreeWidget::itemPressed, [&](QTreeWidgetItem *i, int) { log << "pressed:" + i->text(0); });
    QObject::connect(&tree, &QTreeWidget::itemDoubleClicked, [&](QTreeWidgetItem *i, int) { log << "double:" + i->text(0); });
    QObject::connect(&tree, &QTreeWidget::itemActivated, [&](QTreeWidgetItem *i, int) { log << "activated:" + i->text(0); });
    QObject::connect(&tree, &QTreeWidget::itemExpanded, [&](QTreeWidgetItem *i) { log << "expanded:" + i->text(0); });
    QObject::connect(&tree, &QTreeWidget::itemCollapsed, [&](QTreeWidgetItem *i) { log << "collapsed:" + i->text(0); });
    QObject::connect(&tree, &QTreeWidget::currentItemChanged, [&](QTreeWidgetItem *now, QTreeWidgetItem *before) {
        log << QString("current:%1<-%2").arg(now ? now->text(0) : "none", before ? before->text(0) : "none");
    });
    int selectionChanges = 0;
    QObject::connect(&tree, &QTreeWidget::itemSelectionChanged, [&] { ++selectionChanges; });

    // Building the tree.
    auto *fruit = new QTreeWidgetItem(&tree, {"Fruit", "2"});
    auto *apple = new QTreeWidgetItem(fruit, {"Apple", "10"});
    auto *pear = new QTreeWidgetItem(fruit, QStringList{"Pear"});
    auto *veg = new QTreeWidgetItem(QStringList{"Vegetables", "1"});
    tree.addTopLevelItem(veg);
    auto *carrot = new QTreeWidgetItem;
    carrot->setText(0, "Carrot");
    carrot->setText(1, "9");
    veg->addChild(carrot);
    CHECK(tree.topLevelItemCount() == 2 && tree.topLevelItem(0) == fruit && tree.topLevelItem(1) == veg && tree.topLevelItem(2) == nullptr);
    CHECK(tree.indexOfTopLevelItem(veg) == 1 && tree.indexOfTopLevelItem(apple) == -1);
    CHECK(fruit->treeWidget() == &tree && apple->treeWidget() == &tree && carrot->treeWidget() == &tree);
    CHECK(fruit->parent() == nullptr && apple->parent() == fruit && fruit->childCount() == 2 && fruit->child(1) == pear);
    CHECK(fruit->child(2) == nullptr && fruit->indexOfChild(pear) == 1 && fruit->indexOfChild(carrot) == -1);
    CHECK(apple->text(1) == "10" && pear->text(1).isEmpty() && apple->columnCount() == 2 && pear->columnCount() == 1);
    CHECK(apple->type() == QTreeWidgetItem::Type && QTreeWidgetItem(QTreeWidgetItem::UserType + 1).type() == 1001);
    CHECK(log.isEmpty()); // nothing changed yet: the items came with their texts
    // An item cannot be added twice, or to itself, or below its own child.
    tree.addTopLevelItem(apple);
    fruit->addChild(fruit);
    apple->addChild(fruit);
    QTreeWidgetItem lost(QStringList{"lost"});
    fruit->insertChild(9, &lost); // out of range: not added
    CHECK(lost.treeWidget() == nullptr && lost.parent() == nullptr);
    CHECK(tree.topLevelItemCount() == 2 && fruit->childCount() == 2 && apple->childCount() == 0);

    tree.show();
    settle(&tree);
    // The model behind it.
    const QModelIndex appleIndex = tree.indexFromItem(apple, 1);
    CHECK(appleIndex.isValid() && appleIndex.row() == 0 && appleIndex.column() == 1 && appleIndex.data().toString() == "10");
    CHECK(tree.itemFromIndex(appleIndex) == apple && tree.itemFromIndex(QModelIndex()) == nullptr);
    CHECK(appleIndex.parent() == tree.indexFromItem(fruit) && !tree.indexFromItem(nullptr).isValid());
    QTreeWidgetItem stray;
    CHECK(!tree.indexFromItem(&stray).isValid() && !tree.indexFromItem(apple, 7).isValid());
    CHECK(tree.model()->rowCount() == 2 && tree.model()->rowCount(tree.indexFromItem(fruit)) == 2 && tree.model()->hasChildren(tree.indexFromItem(veg)));

    // Expanding.
    CHECK(!fruit->isExpanded() && tree.visualItemRect(apple).isEmpty());
    fruit->setExpanded(true);
    tree.expandItem(veg);
    CHECK(fruit->isExpanded() && veg->isExpanded() && log == QStringList({"expanded:Fruit", "expanded:Vegetables"}));
    CHECK(tree.itemBelow(fruit) == apple && tree.itemAbove(veg) == pear && tree.itemAbove(fruit) == nullptr && tree.itemBelow(carrot) == nullptr);
    tree.collapseItem(veg);
    CHECK(!veg->isExpanded() && log.last() == "collapsed:Vegetables" && tree.itemBelow(veg) == nullptr);
    tree.expandAll();
    log.clear();
    const QRect appleRect = tree.visualItemRect(apple);
    CHECK(!appleRect.isEmpty() && appleRect.width() > tree.header()->sectionSize(0) && tree.itemAt(appleRect.center()) == apple);
    CHECK(tree.itemAt(5, tree.viewport()->height() - 3) == nullptr);

    // Current item, selection and the signals of the mouse.
    click(tree.viewport(), tree.visualRect(tree.indexFromItem(apple, 1)).center());
    CHECK(tree.currentItem() == apple && tree.currentColumn() == 1 && apple->isSelected() && !pear->isSelected());
    CHECK(log == QStringList({"pressed:Apple", "current:Apple<-none", "clicked:Apple/1"}) && selectionChanges == 1);
    CHECK(tree.selectedItems() == QList<QTreeWidgetItem *>{apple});
    tree.setCurrentItem(pear);
    CHECK(tree.currentItem() == pear && pear->isSelected() && !apple->isSelected() && log.last() == "current:Pear<-Apple");
    tree.setSelectionMode(QAbstractItemView::ExtendedSelection);
    apple->setSelected(true);
    carrot->setSelected(true);
    CHECK(tree.selectedItems().size() == 3 && tree.selectedItems().contains(carrot));
    pear->setSelected(false);
    CHECK(tree.selectedItems() == QList<QTreeWidgetItem *>({apple, carrot}));
    tree.setCurrentItem(carrot, 1);
    CHECK(tree.currentColumn() == 1 && tree.selectedItems() == QList<QTreeWidgetItem *>{carrot});
    tree.setCurrentItem(nullptr);
    CHECK(tree.currentItem() == nullptr);
    log.clear();
    tree.setEditTriggers(QAbstractItemView::NoEditTriggers);
    doubleClick(tree.viewport(), tree.visualRect(tree.indexFromItem(apple)).center());
    CHECK(log.contains("double:Apple") && log.contains("activated:Apple"));
    tree.setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    // Data, and itemChanged() for every change.
    log.clear();
    apple->setText(1, "11");
    apple->setText(1, "11"); // no change
    CHECK(log == QStringList{"changed:Apple/1"} && tree.indexFromItem(apple, 1).data().toString() == "11");
    apple->setToolTip(0, "an apple");
    apple->setTextAlignment(1, Qt::AlignRight);
    apple->setForeground(0, QBrush(QColor(200, 0, 0)));
    apple->setBackground(1, QBrush(QColor(0, 0, 200)));
    QFont bold;
    bold.setBold(true);
    apple->setFont(0, bold);
    apple->setData(0, Qt::UserRole, 77);
    CHECK(log.size() == 7 && apple->toolTip(0) == "an apple" && apple->textAlignment(1) == Qt::AlignRight);
    CHECK(apple->foreground(0).color() == QColor(200, 0, 0) && apple->background(1).color() == QColor(0, 0, 200));
    CHECK(apple->font(0).bold() && apple->data(0, Qt::UserRole).toInt() == 77 && apple->background(0).style() == Qt::NoBrush);
    CHECK(apple->data(0, Qt::EditRole).toString() == "Apple" && !apple->data(5, Qt::DisplayRole).isValid() && !apple->data(-1, 0).isValid());
    apple->setData(-1, Qt::DisplayRole, "x");
    // More columns than the tree has: the tree grows.
    pear->setText(2, "third");
    CHECK(tree.columnCount() == 3 && tree.header()->count() == 3 && pear->columnCount() == 3);
    tree.setColumnCount(2);
    CHECK(tree.columnCount() == 2 && pear->text(2) == "third"); // the item keeps what it had
    tree.setHeaderLabel("Item");
    CHECK(tree.headerItem()->text(0) == "Item" && tree.headerItem()->text(1) == "Count");
    // The item is drawn in its colors.
    tree.setCurrentItem(nullptr);
    tree.clearSelection();
    const QImage image = grab(&tree);
    const QPoint cellPoint = tree.viewport()->mapTo(&tree, tree.visualRect(tree.indexFromItem(apple, 1)).topLeft() + QPoint(3, 3));
    CHECK(colorAt(image, cellPoint) == QColor(0, 0, 200));

    // Check boxes.
    log.clear();
    CHECK(apple->checkState(0) == Qt::Unchecked && !tree.indexFromItem(apple).data(Qt::CheckStateRole).isValid());
    apple->setCheckState(0, Qt::Unchecked);
    CHECK(tree.indexFromItem(apple).data(Qt::CheckStateRole).isValid() && log == QStringList{"changed:Apple/0"});
    const QRect checkRect = tree.visualRect(tree.indexFromItem(apple));
    click(tree.viewport(), QPoint(checkRect.left() + 8, checkRect.center().y()));
    CHECK(apple->checkState(0) == Qt::Checked && log.contains("changed:Apple/0") && log.count("changed:Apple/0") == 2);
    apple->setFlags(apple->flags() & ~Qt::ItemFlags(Qt::ItemIsUserCheckable));
    click(tree.viewport(), QPoint(checkRect.left() + 8, checkRect.center().y()));
    CHECK(apple->checkState(0) == Qt::Checked);

    // Editing needs the flag.
    tree.editItem(apple, 0);
    CHECK(editorOf<QLineEdit>(tree) == nullptr);
    apple->setFlags(apple->flags() | Qt::ItemIsEditable);
    tree.editItem(apple, 0);
    auto *line = editorOf<QLineEdit>(tree);
    CHECK(line && line->text() == "Apple");
    type(&tree, "Cox");
    key(&tree, Qt::Key_Return);
    settle(&tree);
    CHECK(apple->text(0) == "Cox" && log.contains("changed:Cox/0") && editorOf<QLineEdit>(tree) == nullptr);
    apple->setText(0, "Apple");
    apple->setDisabled(true);
    CHECK(apple->isDisabled() && !(tree.model()->flags(tree.indexFromItem(apple)) & Qt::ItemIsEnabled));
    tree.editItem(apple, 0);
    CHECK(editorOf<QLineEdit>(tree) == nullptr);
    apple->setDisabled(false);

    // Finding.
    new QTreeWidgetItem(&tree, {"apple pie", "3"});
    CHECK(tree.findItems("Fruit", Qt::MatchExactly).size() == 1 && tree.findItems("fruit", Qt::MatchExactly).isEmpty());
    CHECK(tree.findItems("fruit", Qt::MatchFixedString).size() == 1 && tree.findItems("fruit", Qt::MatchFixedString | Qt::MatchCaseSensitive).isEmpty());
    CHECK(tree.findItems("apple", Qt::MatchContains).size() == 1); // top level only
    CHECK(tree.findItems("apple", Qt::MatchContains | Qt::MatchRecursive).size() == 2);
    CHECK(tree.findItems("apple", Qt::MatchStartsWith | Qt::MatchRecursive | Qt::MatchCaseSensitive).size() == 1);
    CHECK(tree.findItems("PIE", Qt::MatchEndsWith).size() == 1 && tree.findItems("*r*", Qt::MatchWildcard | Qt::MatchRecursive).size() == 3);
    CHECK(tree.findItems("^[AP]", Qt::MatchRegularExpression | Qt::MatchRecursive | Qt::MatchCaseSensitive).size() == 2);
    CHECK(tree.findItems("1[01]", Qt::MatchRegularExpression | Qt::MatchRecursive, 1).size() == 1);

    // Sorting: by text, and by number where the data are numbers.
    tree.sortItems(0, Qt::AscendingOrder);
    CHECK(tree.sortColumn() == 0 && tree.topLevelItem(0)->text(0) == "Fruit" && tree.topLevelItem(2)->text(0) == "apple pie");
    CHECK(fruit->child(0) == apple && fruit->isExpanded() && tree.itemBelow(fruit) == apple); // expanded items stay expanded
    tree.sortItems(0, Qt::DescendingOrder);
    CHECK(tree.topLevelItem(0)->text(0) == "apple pie" && fruit->child(0) == pear && tree.indexOfTopLevelItem(fruit) == 2);
    tree.setCurrentItem(apple);
    apple->setData(1, Qt::DisplayRole, 9);
    pear->setData(1, Qt::DisplayRole, 10);
    fruit->sortChildren(1, Qt::AscendingOrder);
    CHECK(fruit->child(0) == apple && tree.currentItem() == apple); // 9 before 10, as numbers
    apple->setText(1, "9");
    pear->setText(1, "10");
    fruit->sortChildren(1, Qt::AscendingOrder);
    CHECK(fruit->child(0) == pear && tree.currentItem() == apple); // "10" before "9", as text
    tree.setSortingEnabled(true);
    CHECK(tree.topLevelItem(0)->text(0) == "apple pie"); // descending, as the header said
    click(tree.header(), QPoint(20, tree.header()->height() / 2));
    CHECK(tree.sortColumn() == 0 && tree.topLevelItem(0)->text(0) == "Fruit");
    tree.setSortingEnabled(false);
    // An item that sorts in its own way.
    class Reversed : public QTreeWidgetItem {
    public:
        using QTreeWidgetItem::QTreeWidgetItem;
        bool operator<(const QTreeWidgetItem &other) const override { return text(0) > other.text(0); }
    };
    QTreeWidgetItem holder;
    new Reversed(&holder, QStringList{"a"});
    new Reversed(&holder, QStringList{"c"});
    new Reversed(&holder, QStringList{"b"});
    holder.sortChildren(0, Qt::AscendingOrder);
    CHECK(holder.child(0)->text(0) == "c" && holder.child(2)->text(0) == "a");

    // Taking, deleting and clearing.
    log.clear();
    tree.setCurrentItem(pear);
    QTreeWidgetItem *taken = fruit->takeChild(fruit->indexOfChild(pear));
    CHECK(taken == pear && pear->treeWidget() == nullptr && pear->parent() == nullptr && fruit->childCount() == 1);
    CHECK(tree.currentItem() == apple && !tree.indexFromItem(pear).isValid());
    veg->insertChild(0, pear);
    CHECK(pear->parent() == veg && pear->treeWidget() == &tree && veg->child(0) == pear);
    veg->removeChild(pear);
    delete pear;
    CHECK(veg->childCount() == 1);
    delete carrot; // an item that is deleted leaves its tree
    CHECK(veg->childCount() == 0 && !tree.model()->hasChildren(tree.indexFromItem(veg)));
    veg->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    CHECK(tree.model()->hasChildren(tree.indexFromItem(veg)) && veg->childIndicatorPolicy() == QTreeWidgetItem::ShowIndicator);
    veg->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicatorWhenChildless);
    QList<QTreeWidgetItem *> more{new QTreeWidgetItem(QStringList{"m1"}), new QTreeWidgetItem(QStringList{"m2"})};
    veg->addChildren(more);
    veg->insertChildren(1, {new QTreeWidgetItem(QStringList{"between"})});
    CHECK(veg->childCount() == 3 && veg->child(1)->text(0) == "between");
    const QList<QTreeWidgetItem *> all = veg->takeChildren();
    CHECK(all.size() == 3 && veg->childCount() == 0 && all.first()->treeWidget() == nullptr);
    qDeleteAll(all);
    QTreeWidgetItem *top = tree.takeTopLevelItem(tree.indexOfTopLevelItem(veg));
    CHECK(top == veg && tree.topLevelItemCount() == 2 && tree.takeTopLevelItem(9) == nullptr);
    tree.insertTopLevelItem(0, veg);
    tree.insertTopLevelItems(1, {new QTreeWidgetItem(QStringList{"t1"}), new QTreeWidgetItem(QStringList{"t2"})});
    tree.addTopLevelItems({new QTreeWidgetItem(QStringList{"last"})});
    CHECK(tree.topLevelItem(0) == veg && tree.topLevelItem(2)->text(0) == "t2" && tree.topLevelItem(tree.topLevelItemCount() - 1)->text(0) == "last");

    // Copies.
    std::unique_ptr<QTreeWidgetItem> copy(fruit->clone());
    CHECK(copy->text(0) == "Fruit" && copy->childCount() == 1 && copy->child(0)->text(0) == "Apple" && copy->treeWidget() == nullptr);
    CHECK(copy->child(0)->parent() == copy.get() && copy->child(0)->checkState(0) == Qt::Checked);
    QTreeWidgetItem assigned;
    assigned = *apple;
    CHECK(assigned.text(0) == "Apple" && assigned.flags() == apple->flags() && assigned.childCount() == 0);

    tree.scrollToItem(apple);
    tree.clear();
    CHECK(tree.topLevelItemCount() == 0 && tree.currentItem() == nullptr && tree.columnCount() == 2 && tree.headerItem()->text(1) == "Count");
    CHECK(tree.model()->rowCount() == 0 && tree.selectedItems().isEmpty());
    new QTreeWidgetItem(&tree, QStringList{"again"});
    CHECK(tree.topLevelItemCount() == 1);
    grab(&tree);
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    testFlatModel();
    testTree();
    testEditing();
    testSorting();
    testTreeWidget();
    if (g_failures == 0)
        std::puts("all item view tests passed");
    return g_failures == 0 ? 0 : 1;
}
