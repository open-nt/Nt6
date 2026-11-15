// MIT License - see LICENSE
//
// Tests for the model/view classes of Nt Widgets on the offscreen platform: QTreeView, QListView and QTableView
// with models of Nt Core, the delegates and their editors, sorting from the header, and QTreeWidget. The tests play the window system
// through QWindowSystemInterface.
#include <QtCore>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qcheckbox.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qdatawidgetmapper.h>
#include <QtWidgets/qdatetimeedit.h>
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qitemdelegate.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qplaintextedit.h>
#include <QtWidgets/qspinbox.h>
#include <QtWidgets/qstyleditemdelegate.h>
#include <QtWidgets/qtableview.h>
#include <QtWidgets/qtreeview.h>
#include <QtWidgets/qtreewidget.h>

#include "platforms/offscreen/qoffscreenintegration_p.h"
#include "qpa/qwindowsysteminterface_p.h"

#include <algorithm>
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

// A table of strings, "r00c0" and so on, that can be edited, sorted by a column, and grow and shrink.
class GridModel : public QAbstractTableModel {
public:
    GridModel(int rows, int columns) {
        for (int r = 0; r < rows; ++r)
            cells.push_back(makeRow(r, columns));
        for (int c = 0; c < columns; ++c)
            headers << QString("C%1").arg(c);
    }
    static std::vector<QString> makeRow(int r, int columns) {
        std::vector<QString> row;
        for (int c = 0; c < columns; ++c)
            row.push_back(QString("r%1c%2").arg(r, 2, 10, QChar('0')).arg(c));
        return row;
    }
    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(cells.size());
    }
    int columnCount(const QModelIndex &parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(headers.size());
    }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid())
            return QVariant();
        const QString &cell = cells[static_cast<std::size_t>(index.row())][static_cast<std::size_t>(index.column())];
        if (role == Qt::DisplayRole || role == Qt::EditRole)
            return cell;
        if (role == Qt::ToolTipRole)
            return "tip " + cell;
        return QVariant();
    }
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override {
        if (!index.isValid() || role != Qt::EditRole)
            return false;
        cells[static_cast<std::size_t>(index.row())][static_cast<std::size_t>(index.column())] = value.toString();
        emit dataChanged(index, index, {role});
        return true;
    }
    Qt::ItemFlags flags(const QModelIndex &index) const override {
        return index.isValid() ? Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable : Qt::ItemFlags();
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 && section < headers.size())
            return headers.at(section);
        return QVariant();
    }
    void sort(int column, Qt::SortOrder order) override {
        emit layoutAboutToBeChanged();
        const auto c = static_cast<std::size_t>(column);
        std::stable_sort(cells.begin(), cells.end(), [c, order](const auto &a, const auto &b) {
            return order == Qt::AscendingOrder ? a[c] < b[c] : b[c] < a[c];
        });
        ++sorts;
        emit layoutChanged();
    }
    void addRow(int at, const QString &prefix) {
        beginInsertRows(QModelIndex(), at, at);
        std::vector<QString> row;
        for (int c = 0; c < headers.size(); ++c)
            row.push_back(prefix + QString::number(c));
        cells.insert(cells.begin() + at, row);
        endInsertRows();
    }
    void removeRow(int at) {
        beginRemoveRows(QModelIndex(), at, at);
        cells.erase(cells.begin() + at);
        endRemoveRows();
    }
    void addColumn(int at, const QString &name) {
        beginInsertColumns(QModelIndex(), at, at);
        headers.insert(at, name);
        for (auto &row : cells)
            row.insert(row.begin() + at, name);
        endInsertColumns();
    }

    std::vector<std::vector<QString>> cells;
    QStringList headers;
    int sorts = 0;
};

// A list of items with a red square as their decoration.
class IconModel : public QAbstractListModel {
public:
    explicit IconModel(int count) : m_count(count) {
        m_icon = QPixmap(16, 16);
        m_icon.fill(QColor(255, 0, 0));
    }
    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return parent.isValid() ? 0 : m_count; }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid())
            return QVariant();
        if (role == Qt::DisplayRole)
            return QString("icon %1").arg(index.row());
        if (role == Qt::DecorationRole)
            return QVariant::fromValue(m_icon);
        return QVariant();
    }

private:
    int m_count;
    QPixmap m_icon;
};

// A delegate that remembers the options that the view drew its items with.
class ProbeDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        last = option;
        QStyledItemDelegate::paint(painter, option, index);
    }
    mutable QStyleOptionViewItem last;
};

QList<int> rowsOf(const QModelIndexList &indexes) {
    QList<int> rows;
    for (const QModelIndex &i : indexes)
        rows << i.row();
    std::sort(rows.begin(), rows.end());
    return rows;
}

void drag(QWidget *w, QPoint from, QPoint to) {
    mouse(w, QEvent::MouseButtonPress, from);
    mouse(w, QEvent::MouseMove, (from + to) / 2, Qt::NoButton, Qt::LeftButton);
    mouse(w, QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton);
    mouse(w, QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
}

} // namespace

static void testListView() {
    QStringListModel model;
    QStringList words;
    for (int i = 0; i < 50; ++i)
        words << QString("item %1").arg(i, 2, 10, QChar('0'));
    model.setStringList(words);

    QListView view;
    CHECK(view.viewMode() == QListView::ListMode && view.flow() == QListView::TopToBottom && !view.isWrapping());
    CHECK(view.movement() == QListView::Static && view.spacing() == 0 && view.modelColumn() == 0 &&
          !view.gridSize().isValid());
    CHECK(view.resizeMode() == QListView::Fixed && view.layoutMode() == QListView::SinglePass &&
          view.batchSize() == 100);
    CHECK(!view.uniformItemSizes() && !view.wordWrap() && !view.isSelectionRectVisible() && !view.iconSize().isValid());
    CHECK(!view.indexAt(QPoint(5, 5)).isValid() && view.selectionMode() == QAbstractItemView::SingleSelection);
    view.resize(220, 160);
    view.setModel(&model);
    view.show();
    settle(&view);
    ViewSignals spy(&view);

    // One item under the other, as wide as the viewport.
    const QRect r0 = view.visualRect(model.index(0));
    const QRect r1 = view.visualRect(model.index(1));
    CHECK(r0.topLeft() == QPoint(0, 0) && r0.height() >= 10 && r0.width() == view.viewport()->width());
    CHECK(r1.top() == r0.bottom() + 1 && r1.height() == r0.height());
    CHECK(view.indexAt(r1.center()) == model.index(1) && !view.indexAt(QPoint(5, 50 * r0.height() + 5)).isValid());
    CHECK(view.verticalScrollBar()->maximum() == 50 * r0.height() - view.viewport()->height());
    CHECK(view.horizontalScrollBar()->maximum() == 0);

    click(view.viewport(), r1.center());
    CHECK(view.currentIndex() == model.index(1) && rowsOf(view.selectionModel()->selectedIndexes()) == QList<int>{1});
    CHECK(spy.log == QStringList({"pressed:item 01", "clicked:item 01"}));
    spy.log.clear();

    // The keyboard.
    view.setFocus();
    key(&view, Qt::Key_Down);
    CHECK(view.currentIndex().row() == 2);
    key(&view, Qt::Key_Right); // nothing to the right in a list
    CHECK(view.currentIndex().row() == 2);
    key(&view, Qt::Key_End);
    CHECK(view.currentIndex().row() == 49 && view.verticalScrollBar()->value() == view.verticalScrollBar()->maximum());
    CHECK(view.viewport()->rect().contains(view.visualRect(view.currentIndex())));
    key(&view, Qt::Key_PageUp);
    const int page = 49 - view.currentIndex().row();
    CHECK(page >= 3 && page <= view.viewport()->height() / r0.height());
    key(&view, Qt::Key_Home);
    CHECK(view.currentIndex().row() == 0 && view.verticalScrollBar()->value() == 0);
    key(&view, Qt::Key_PageDown);
    CHECK(view.currentIndex().row() == page);
    key(&view, Qt::Key_Return);
    CHECK(spy.log == QStringList{"activated:" + words.at(page)});
    spy.log.clear();

    // scrollTo().
    view.scrollTo(model.index(30), QAbstractItemView::PositionAtTop);
    CHECK(view.visualRect(model.index(30)).top() == 0);
    view.scrollTo(model.index(30), QAbstractItemView::PositionAtBottom);
    CHECK(view.visualRect(model.index(30)).bottom() == view.viewport()->height() - 1);
    view.scrollToTop();

    // Spacing around each item.
    view.setSpacing(4);
    CHECK(view.visualRect(model.index(0)).topLeft() == QPoint(4, 4));
    CHECK(view.visualRect(model.index(1)).top() == view.visualRect(model.index(0)).bottom() + 1 + 4);
    CHECK(view.visualRect(model.index(0)).width() == view.viewport()->width() - 8);
    view.setSpacing(0);

    // Hidden rows.
    view.setRowHidden(1, true);
    CHECK(view.isRowHidden(1) && !view.isRowHidden(2) && view.visualRect(model.index(1)).isEmpty());
    CHECK(view.visualRect(model.index(2)).top() == r0.bottom() + 1);
    view.setCurrentIndex(model.index(0));
    key(&view, Qt::Key_Down);
    CHECK(view.currentIndex().row() == 2);
    model.insertRows(0, 1); // the hidden row moves with its item
    CHECK(view.isRowHidden(2) && !view.isRowHidden(1));
    model.removeRows(0, 1);
    CHECK(view.isRowHidden(1));
    view.clearPropertyFlags();
    CHECK(!view.isRowHidden(1) && view.visualRect(model.index(1)) == r1);

    // Selection: Shift, Control, dragging and everything.
    view.setSelectionMode(QAbstractItemView::ExtendedSelection);
    click(view.viewport(), view.visualRect(model.index(1)).center());
    click(view.viewport(), view.visualRect(model.index(4)).center(), Qt::ShiftModifier);
    CHECK(rowsOf(view.selectionModel()->selectedIndexes()) == QList<int>({1, 2, 3, 4}));
    click(view.viewport(), view.visualRect(model.index(2)).center(), Qt::ControlModifier);
    CHECK(rowsOf(view.selectionModel()->selectedIndexes()) == QList<int>({1, 3, 4}));
    drag(view.viewport(), view.visualRect(model.index(5)).center(), view.visualRect(model.index(2)).center());
    CHECK(rowsOf(view.selectionModel()->selectedIndexes()) == QList<int>({2, 3, 4, 5}) &&
          view.currentIndex().row() == 2);
    key(&view, Qt::Key_A, "a", Qt::ControlModifier);
    CHECK(view.selectionModel()->selectedIndexes().size() == 50);
    view.setRowHidden(3, true);
    view.selectAll();
    CHECK(view.selectionModel()->selectedIndexes().size() == 49 && !view.selectionModel()->isSelected(model.index(3)));
    view.clearPropertyFlags();
    click(view.viewport(), QPoint(5, view.viewport()->height() + 100)); // outside the viewport: nothing
    view.setSelectionMode(QAbstractItemView::SingleSelection);

    // The model changes under the view.
    view.setCurrentIndex(model.index(10));
    model.removeRows(10, 1);
    CHECK(view.currentIndex().row() == 10 && view.currentIndex().data().toString() == "item 11");
    model.insertRows(0, 2);
    CHECK(model.rowCount() == 51 && view.visualRect(model.index(50)).isValid());
    CHECK(view.visualRect(model.index(2)).top() == 2 * r0.height() - view.verticalScrollBar()->value());

    // Editing in place.
    view.scrollToTop();
    doubleClick(view.viewport(), view.visualRect(model.index(2)).center());
    QLineEdit *editor = editorOf<QLineEdit>(view);
    CHECK(editor != nullptr && editor && editor->text() == "item 00");
    if (editor) {
        editor->selectAll();
        type(editor, "hazel");
        key(editor, Qt::Key_Return);
    }
    CHECK(model.index(2).data().toString() == "hazel");

    // From left to right, without wrapping: a row that scrolls sideways.
    view.setFlow(QListView::LeftToRight);
    const QRect h0 = view.visualRect(model.index(0)), h1 = view.visualRect(model.index(1));
    CHECK(h0.topLeft() == QPoint(0, 0) && h1.left() == h0.right() + 1 && h1.top() == 0);
    CHECK(view.horizontalScrollBar()->maximum() > 0 && view.verticalScrollBar()->maximum() == 0);
    view.setCurrentIndex(model.index(0));
    key(&view, Qt::Key_Right);
    CHECK(view.currentIndex().row() == 1);
    key(&view, Qt::Key_Down);
    CHECK(view.currentIndex().row() == 1);
    view.setFlow(QListView::TopToBottom);

    // Another column of a model.
    NodeModel nodes;
    nodes.add(nullptr, "walnut", 12);
    nodes.add(nullptr, "pecan", 7);
    view.setModel(&nodes);
    view.setModelColumn(1);
    CHECK(view.modelColumn() == 1 && view.visualRect(nodes.index(1, 1)).isValid() &&
          view.visualRect(nodes.index(1, 0)).isEmpty());
    CHECK(view.indexAt(view.visualRect(nodes.index(1, 1)).center()) == nodes.index(1, 1));
    view.setModelColumn(5); // no such column: nothing shows
    CHECK(!view.indexAt(QPoint(5, 5)).isValid());
    view.setModelColumn(0);
    view.setModel(nullptr);
    CHECK(view.model() == nullptr && !view.indexAt(QPoint(5, 5)).isValid());
    key(&view, Qt::Key_Down);
    grab(&view);
}

static void testIconMode() {
    IconModel model(30);
    QListView view;
    auto *probe = new ProbeDelegate(&view);
    view.setItemDelegate(probe);
    view.resize(260, 200);
    view.setModel(&model);
    view.setViewMode(QListView::IconMode);
    CHECK(view.viewMode() == QListView::IconMode && view.flow() == QListView::LeftToRight && view.isWrapping());
    CHECK(view.movement() == QListView::Free);
    view.setGridSize(QSize(60, 70));
    view.show();
    settle(&view);

    // Cells of the grid, in rows that wrap at the width of the viewport.
    const int perRow = view.viewport()->width() / 60;
    CHECK(perRow >= 2 && view.visualRect(model.index(0)) == QRect(0, 0, 60, 70));
    CHECK(view.visualRect(model.index(1)) == QRect(60, 0, 60, 70));
    CHECK(view.visualRect(model.index(perRow)) == QRect(0, 70, 60, 70));
    CHECK(view.horizontalScrollBar()->maximum() == 0 && view.verticalScrollBar()->maximum() > 0);
    CHECK(view.indexAt(QPoint(65, 75)) == model.index(perRow + 1));

    // The arrow keys go to the neighbours.
    view.setFocus();
    view.setCurrentIndex(model.index(0));
    key(&view, Qt::Key_Right);
    CHECK(view.currentIndex().row() == 1);
    key(&view, Qt::Key_Down);
    CHECK(view.currentIndex().row() == perRow + 1);
    key(&view, Qt::Key_Left);
    CHECK(view.currentIndex().row() == perRow);
    key(&view, Qt::Key_Up);
    CHECK(view.currentIndex().row() == 0);
    key(&view, Qt::Key_Up);
    CHECK(view.currentIndex().row() == 0);
    key(&view, Qt::Key_End);
    CHECK(view.currentIndex().row() == 29 && view.viewport()->rect().contains(view.visualRect(view.currentIndex())));

    // The decoration goes above the text, in the icon size.
    grab(&view);
    CHECK(probe->last.decorationPosition == QStyleOptionViewItem::Top && probe->last.decorationSize == QSize(32, 32));
    CHECK(probe->last.displayAlignment == (Qt::AlignHCenter | Qt::AlignTop));
    QList<QSize> sizes;
    QObject::connect(&view, &QAbstractItemView::iconSizeChanged, [&sizes](const QSize &s) { sizes << s; });
    view.setIconSize(QSize(48, 48));
    view.setIconSize(QSize(48, 48));
    CHECK(view.iconSize() == QSize(48, 48) && sizes == QList<QSize>{QSize(48, 48)});
    view.scrollToTop();
    view.viewport()->update();
    grab(&view);
    CHECK(probe->last.decorationSize == QSize(48, 48));

    // Without a grid, each item has the size the delegate wants: room for the icon above the text.
    view.setGridSize(QSize());
    const QRect item = view.visualRect(model.index(0));
    CHECK(item.height() > 48 + view.fontMetrics().height() && item.width() >= 48);
    // The icon is drawn above the text, in the middle.
    view.setIconSize(QSize(16, 16));
    view.scrollToTop();
    view.setCurrentIndex(QModelIndex());
    view.clearSelection();
    const QImage image = grab(&view);
    const QRect first = view.visualRect(model.index(0));
    const QPoint iconCenter = view.viewport()->mapTo(&view, QPoint(first.center().x(), first.top() + 3 + 8));
    CHECK(colorAt(image, view.mapTo(view.window(), iconCenter)) == QColor(255, 0, 0));

    // Back to a list: the decoration is on the left.
    view.setViewMode(QListView::ListMode);
    CHECK(view.flow() == QListView::TopToBottom && !view.isWrapping() && view.movement() == QListView::Static);
    view.setIconSize(QSize());
    grab(&view);
    CHECK(probe->last.decorationPosition == QStyleOptionViewItem::Left);
}

static void testTableView() {
    GridModel model(40, 5);
    QTableView view;
    CHECK(view.showGrid() && view.gridStyle() == Qt::SolidLine && !view.isSortingEnabled() && view.wordWrap());
    CHECK(view.isCornerButtonEnabled() && view.horizontalHeader() && view.verticalHeader());
    CHECK(view.selectionBehavior() == QAbstractItemView::SelectItems && view.horizontalHeader()->count() == 0);
    view.resize(420, 300);
    view.setModel(&model);
    view.show();
    settle(&view);
    ViewSignals spy(&view);
    QHeaderView *columns = view.horizontalHeader(), *rows = view.verticalHeader();
    CHECK(columns->count() == 5 && rows->count() == 40 && columns->hiddenSectionCount() == 0);
    const int w = view.columnWidth(0), h = view.rowHeight(0);
    CHECK(w == 100 && h == 30);

    // Cells in rows and columns, below and right of the headers.
    CHECK(view.visualRect(model.index(0, 0)) == QRect(0, 0, w, h));
    CHECK(view.visualRect(model.index(1, 2)) == QRect(2 * w, h, w, h));
    CHECK(view.indexAt(QPoint(2 * w + 10, h + 10)) == model.index(1, 2) && view.rowAt(h + 10) == 1 &&
          view.columnAt(2 * w + 10) == 2);
    CHECK(view.rowViewportPosition(3) == 3 * h && view.columnViewportPosition(4) == 4 * w);
    CHECK(view.viewport()->x() >= rows->width() && view.viewport()->y() >= columns->height() && rows->width() > 0);
    CHECK(view.verticalScrollBar()->maximum() == 40 * h - view.viewport()->height());
    CHECK(view.horizontalScrollBar()->maximum() == 5 * w - view.viewport()->width());

    click(view.viewport(), QPoint(2 * w + 10, h + 10));
    CHECK(view.currentIndex() == model.index(1, 2) &&
          view.selectionModel()->selectedIndexes() == QModelIndexList{model.index(1, 2)});
    CHECK(spy.log == QStringList({"pressed:r01c2", "clicked:r01c2"}));

    // The keyboard.
    view.setFocus();
    key(&view, Qt::Key_Right);
    CHECK(view.currentIndex() == model.index(1, 3));
    key(&view, Qt::Key_Down);
    CHECK(view.currentIndex() == model.index(2, 3));
    key(&view, Qt::Key_Home);
    CHECK(view.currentIndex() == model.index(2, 0));
    key(&view, Qt::Key_End);
    CHECK(view.currentIndex() == model.index(2, 4) &&
          view.horizontalScrollBar()->value() == view.horizontalScrollBar()->maximum());
    key(&view, Qt::Key_End, QString(), Qt::ControlModifier);
    CHECK(view.currentIndex() == model.index(39, 4) &&
          view.viewport()->rect().contains(view.visualRect(view.currentIndex())));
    CHECK(view.selectionModel()->selectedIndexes() == QModelIndexList{model.index(39, 4)});
    key(&view, Qt::Key_PageUp);
    CHECK(view.currentIndex().row() < 39 && view.currentIndex().row() >= 39 - view.viewport()->height() / h);
    key(&view, Qt::Key_Home, QString(), Qt::ControlModifier);
    CHECK(view.currentIndex() == model.index(0, 0) && view.verticalScrollBar()->value() == 0 &&
          view.horizontalScrollBar()->value() == 0);

    // A block of cells, with Shift and by dragging.
    view.setSelectionMode(QAbstractItemView::ExtendedSelection);
    click(view.viewport(), view.visualRect(model.index(1, 1)).center());
    click(view.viewport(), view.visualRect(model.index(3, 2)).center(), Qt::ShiftModifier);
    CHECK(view.selectionModel()->selectedIndexes().size() == 6 && view.selectionModel()->isSelected(model.index(2, 2)));
    click(view.viewport(), view.visualRect(model.index(0, 3)).center(), Qt::ControlModifier);
    CHECK(view.selectionModel()->selectedIndexes().size() == 7);
    drag(view.viewport(), view.visualRect(model.index(0, 0)).center(), view.visualRect(model.index(2, 1)).center());
    CHECK(view.selectionModel()->selectedIndexes().size() == 6 && view.currentIndex() == model.index(2, 1));
    key(&view, Qt::Key_Down, QString(), Qt::ShiftModifier);
    CHECK(view.selectionModel()->selectedIndexes().size() == 8);

    // Rows and columns.
    view.selectRow(5);
    CHECK(view.selectionModel()->isRowSelected(5, QModelIndex()) &&
          view.selectionModel()->selectedIndexes().size() == 5);
    CHECK(view.currentIndex().row() == 5);
    view.selectColumn(2);
    CHECK(view.selectionModel()->isColumnSelected(2, QModelIndex()) &&
          view.selectionModel()->selectedIndexes().size() == 40);
    click(rows, QPoint(5, 2 * h + 10));
    CHECK(view.selectionModel()->isRowSelected(2, QModelIndex()) &&
          view.selectionModel()->selectedIndexes().size() == 5);
    click(columns, QPoint(w + 10, 5));
    CHECK(view.selectionModel()->isColumnSelected(1, QModelIndex()));
    key(&view, Qt::Key_A, "a", Qt::ControlModifier);
    CHECK(view.selectionModel()->selectedIndexes().size() == 200);
    view.clearSelection();
    click(&view, QPoint(view.frameWidth() + 2, view.frameWidth() + 2)); // the corner button
    CHECK(view.selectionModel()->selectedIndexes().size() == 200);
    view.clearSelection();
    view.setCornerButtonEnabled(false);
    click(&view, QPoint(view.frameWidth() + 2, view.frameWidth() + 2));
    CHECK(view.selectionModel()->selectedIndexes().isEmpty());
    view.setSelectionMode(QAbstractItemView::SingleSelection);
    view.clearSelection();
    view.selectColumn(1); // one item at a time: no columns
    CHECK(view.selectionModel()->selectedIndexes().isEmpty());
    view.setSelectionBehavior(QAbstractItemView::SelectRows);
    click(view.viewport(), view.visualRect(model.index(4, 3)).center());
    CHECK(view.selectionModel()->isRowSelected(4, QModelIndex()) &&
          view.selectionModel()->selectedIndexes().size() == 5);
    view.setSelectionBehavior(QAbstractItemView::SelectItems);

    // Hidden rows and columns.
    view.horizontalScrollBar()->setValue(0);
    view.hideColumn(1);
    CHECK(view.isColumnHidden(1) && columns->isSectionHidden(1) && columns->hiddenSectionCount() == 1 &&
          view.columnWidth(1) == 0);
    CHECK(view.visualRect(model.index(0, 1)).isEmpty() && view.visualRect(model.index(0, 2)).left() == w);
    CHECK(view.columnAt(w + 10) == 2 &&
          view.horizontalScrollBar()->maximum() == std::max(0, 4 * w - view.viewport()->width()));
    view.setCurrentIndex(model.index(0, 0));
    key(&view, Qt::Key_Right);
    CHECK(view.currentIndex() == model.index(0, 2));
    view.hideRow(1);
    key(&view, Qt::Key_Down);
    CHECK(view.currentIndex() == model.index(2, 2) && view.isRowHidden(1) &&
          view.visualRect(model.index(2, 2)).top() == h);
    model.addColumn(0, "new"); // the hidden column moves with its column
    CHECK(columns->count() == 6 && view.isColumnHidden(2) && !view.isColumnHidden(1));
    model.addRow(0, "first"); // and the hidden row with its row
    CHECK(rows->count() == 41 && view.isRowHidden(2) && !view.isRowHidden(1));
    model.removeRow(0);
    CHECK(rows->count() == 40 && view.isRowHidden(1));
    view.showRow(1);
    view.showColumn(2);
    CHECK(!view.isRowHidden(1) && columns->hiddenSectionCount() == 0 && view.columnWidth(2) == w);

    // Sizes.
    view.setColumnWidth(0, 60);
    view.setRowHeight(0, 40);
    CHECK(view.visualRect(model.index(0, 1)) == QRect(60, 0, w, 40) && view.visualRect(model.index(1, 0)).top() == 40);
    view.resizeColumnToContents(1);
    CHECK(view.columnWidth(1) >= view.fontMetrics().horizontalAdvance("r00c0") && view.columnWidth(1) < w);
    view.resizeRowsToContents();
    CHECK(view.rowHeight(0) >= view.fontMetrics().height() && view.rowHeight(0) <= 40);

    // Sorting from the header.
    view.setSortingEnabled(true);
    CHECK(view.isSortingEnabled() && columns->isSortIndicatorShown());
    view.clearSelection();
    click(columns, QPoint(view.columnViewportPosition(2) + 10, 5));
    CHECK(model.sorts == 1 && columns->sortIndicatorSection() == 2 &&
          columns->sortIndicatorOrder() == Qt::AscendingOrder);
    CHECK(view.selectionModel()->selectedIndexes().isEmpty()); // a click sorts instead of selecting
    click(columns, QPoint(view.columnViewportPosition(2) + 10, 5));
    // Column 2 holds what was column 1 before the column "new" came in at 0.
    CHECK(columns->sortIndicatorOrder() == Qt::DescendingOrder && model.index(0, 2).data().toString() == "r39c1");
    view.sortByColumn(1, Qt::AscendingOrder);
    CHECK(model.index(0, 1).data().toString() == "r00c0" && columns->sortIndicatorSection() == 1);
    view.setSortingEnabled(false);

    // Editing in place.
    view.scrollToTop();
    doubleClick(view.viewport(), view.visualRect(model.index(1, 1)).center());
    QLineEdit *editor = editorOf<QLineEdit>(view);
    CHECK(editor != nullptr);
    if (editor) {
        editor->selectAll();
        type(editor, "cashew");
        key(editor, Qt::Key_Return);
    }
    CHECK(model.index(1, 1).data().toString() == "cashew");

    // The grid between the cells.
    view.setCurrentIndex(QModelIndex());
    view.clearSelection();
    view.setColumnWidth(0, w);
    view.scrollToTop();
    const QRect cell = view.visualRect(model.index(5, 0));
    const auto at = [&view](QPoint p) { return view.viewport()->mapTo(view.window(), p); };
    QImage image = grab(&view);
    const QColor inside = colorAt(image, at(cell.center() + QPoint(30, 0)));
    CHECK(colorAt(image, at(QPoint(cell.right(), cell.center().y()))) != inside);
    CHECK(colorAt(image, at(QPoint(cell.center().x() + 30, cell.bottom()))) != inside);
    view.setShowGrid(false);
    image = grab(&view);
    CHECK(colorAt(image, at(QPoint(cell.right(), cell.center().y()))) == inside);
    view.setShowGrid(true);
    view.setGridStyle(Qt::DotLine);
    grab(&view);

    // The rows of the model come and go; a reset forgets the sizes.
    view.setCurrentIndex(model.index(3, 1));
    model.removeRow(3);
    CHECK(view.currentIndex().row() == 3 && view.currentIndex().data().toString() == "r04c0");
    view.setColumnWidth(4, 30);
    view.reset();
    CHECK(view.columnWidth(4) == w && rows->count() == 39);
    view.setModel(nullptr);
    CHECK(columns->count() == 0 && rows->count() == 0 && !view.indexAt(QPoint(5, 5)).isValid());
    key(&view, Qt::Key_Down);
    grab(&view);
}

static void testDataWidgetMapper() {
    GridModel model(5, 3);
    QWidget form;
    auto *name = new QLineEdit(&form);
    auto *other = new QLineEdit(&form);
    auto *notes = new QPlainTextEdit(&form);
    name->setGeometry(0, 0, 100, 24);
    other->setGeometry(0, 30, 100, 24);
    notes->setGeometry(0, 60, 100, 60);
    form.resize(120, 130);
    form.show();
    settle(&form);

    QDataWidgetMapper mapper;
    CHECK(mapper.model() == nullptr && mapper.currentIndex() == -1 && mapper.orientation() == Qt::Horizontal);
    CHECK(mapper.submitPolicy() == QDataWidgetMapper::AutoSubmit && mapper.itemDelegate() != nullptr);
    QList<int> moves;
    QObject::connect(&mapper, &QDataWidgetMapper::currentIndexChanged, [&moves](int i) { moves << i; });
    mapper.setModel(&model);
    mapper.addMapping(name, 0);
    mapper.addMapping(other, 1);
    mapper.addMapping(notes, 2, "plainText");
    CHECK(mapper.mappedSection(name) == 0 && mapper.mappedSection(notes) == 2 && mapper.mappedSection(&form) == -1);
    CHECK(mapper.mappedWidgetAt(1) == other && mapper.mappedWidgetAt(7) == nullptr);
    CHECK(mapper.mappedPropertyName(name) == "text" && mapper.mappedPropertyName(notes) == "plainText");
    CHECK(name->text().isEmpty()); // nothing is current yet

    // Going through the rows fills the widgets.
    mapper.toFirst();
    CHECK(mapper.currentIndex() == 0 && name->text() == "r00c0" && other->text() == "r00c1" &&
          notes->toPlainText() == "r00c2");
    mapper.toNext();
    mapper.toNext();
    CHECK(mapper.currentIndex() == 2 && name->text() == "r02c0");
    mapper.toLast();
    mapper.toNext(); // past the end: nothing
    CHECK(mapper.currentIndex() == 4 && other->text() == "r04c1");
    mapper.toPrevious();
    mapper.setCurrentIndex(9);
    CHECK(mapper.currentIndex() == 3 && moves == QList<int>({0, 1, 2, 4, 3}));
    mapper.setCurrentModelIndex(model.index(1, 2));
    CHECK(mapper.currentIndex() == 1 && name->text() == "r01c0");

    // A change of the model shows at once.
    model.setData(model.index(1, 1), "walnut");
    CHECK(other->text() == "walnut");

    // Auto submit: leaving a widget writes it, Return too, and Escape brings the model's value back.
    name->setFocus();
    name->setText("hazel");
    other->setFocus();
    CHECK(model.cells[1][0] == "hazel");
    other->setText("pecan");
    key(other, Qt::Key_Return);
    CHECK(model.cells[1][1] == "pecan");
    other->setText("typo");
    key(other, Qt::Key_Escape);
    CHECK(other->text() == "pecan" && model.cells[1][1] == "pecan");

    // Manual submit: nothing reaches the model until submit().
    mapper.setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    name->setFocus();
    name->setText("almond");
    notes->setPlainText("crunchy");
    other->setFocus();
    CHECK(model.cells[1][0] == "hazel");
    mapper.revert();
    CHECK(name->text() == "hazel" && notes->toPlainText() == "r01c2");
    name->setText("almond");
    notes->setPlainText("crunchy");
    CHECK(mapper.submit() && model.cells[1][0] == "almond" && model.cells[1][2] == "crunchy");

    // Any widget, through its user property.
    auto *box = new QCheckBox(&form);
    GridModel flags(2, 2);
    flags.cells[0][1] = "true";
    QDataWidgetMapper boxes;
    boxes.setModel(&flags);
    boxes.addMapping(box, 1);
    boxes.toFirst();
    CHECK(box->isChecked() && boxes.mappedPropertyName(box) == "checked");
    box->setChecked(false);
    CHECK(boxes.submit() && flags.cells[0][1] == "false");

    // The vertical orientation goes through the columns.
    QDataWidgetMapper columns;
    columns.setOrientation(Qt::Vertical);
    columns.setModel(&model);
    auto *cell = new QLineEdit(&form);
    columns.addMapping(cell, 3);
    columns.toLast();
    CHECK(columns.currentIndex() == 2 && cell->text() == "r03c2");

    // Mappings go away.
    mapper.removeMapping(other);
    CHECK(mapper.mappedSection(other) == -1 && mapper.mappedWidgetAt(1) == nullptr);
    mapper.toFirst();
    CHECK(other->text() == "pecan" && name->text() == "r00c0");
    mapper.clearMapping();
    mapper.toNext();
    CHECK(name->text() == "r00c0" && mapper.currentIndex() == 1);
    mapper.setModel(nullptr);
    CHECK(mapper.currentIndex() == -1 && !mapper.submit());
}

// A QItemDelegate that writes down what it is asked to draw, and where.
class LoggingItemDelegate : public QItemDelegate {
public:
    using QItemDelegate::doLayout;
    using QItemDelegate::QItemDelegate;
    using QItemDelegate::rect;
    mutable QStringList texts;
    mutable QRect lastText, lastCheck, lastDecoration;
    mutable int checks = 0, focus = 0;

protected:
    void drawDisplay(QPainter *painter, const QStyleOptionViewItem &option, const QRect &r,
                     const QString &text) const override {
        texts << text;
        lastText = r;
        QItemDelegate::drawDisplay(painter, option, r, text);
    }
    void drawCheck(QPainter *painter, const QStyleOptionViewItem &option, const QRect &r,
                   Qt::CheckState state) const override {
        ++checks;
        lastCheck = r;
        QItemDelegate::drawCheck(painter, option, r, state);
    }
    void drawDecoration(QPainter *painter, const QStyleOptionViewItem &option, const QRect &r,
                        const QPixmap &pixmap) const override {
        if (!pixmap.isNull())
            lastDecoration = r;
        QItemDelegate::drawDecoration(painter, option, r, pixmap);
    }
    void drawFocus(QPainter *painter, const QStyleOptionViewItem &option, const QRect &r) const override {
        if (option.state & QStyle::State_HasFocus)
            ++focus;
        QItemDelegate::drawFocus(painter, option, r);
    }
};

static void testItemDelegate() {
    // The parts of an item, as doLayout() places them.
    IconModel icons(1);
    LoggingItemDelegate delegate;
    CHECK(delegate.hasClipping());
    delegate.setClipping(false);
    CHECK(!delegate.hasClipping());
    delegate.setClipping(true);
    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, 200, 60);
    option.font = QApplication::font();
    option.fontMetrics = QFontMetrics(option.font);
    option.state = QStyle::State_Enabled;
    const QModelIndex icon = icons.index(0);
    CHECK(delegate.rect(option, icon, Qt::DecorationRole).size() == QSize(16, 16));
    CHECK(delegate.rect(option, icon, Qt::CheckStateRole).isEmpty());
    CHECK(delegate.rect(option, icon, Qt::DisplayRole).width() > option.fontMetrics.horizontalAdvance("icon 0"));
    QImage image(200, 60, QImage::Format_ARGB32);
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        delegate.paint(&painter, option, icon);
    }
    CHECK(delegate.texts == QStringList{"icon 0"} && delegate.lastDecoration.left() < delegate.lastText.left());
    CHECK(delegate.lastDecoration.top() == 22 && delegate.lastDecoration.height() == 16 && delegate.checks == 0);
    CHECK(image.pixelColor(delegate.lastDecoration.center().x(), delegate.lastDecoration.center().y()) ==
          QColor(255, 0, 0));
    const QSize beside = delegate.sizeHint(option, icon);
    CHECK(beside.width() >= 16 + option.fontMetrics.horizontalAdvance("icon 0") && beside.height() >= 16);
    option.decorationPosition = QStyleOptionViewItem::Top;
    const QSize above = delegate.sizeHint(option, icon);
    CHECK(above.height() > beside.height() && above.width() < beside.width());
    {
        QPainter painter(&image);
        delegate.paint(&painter, option, icon);
    }
    CHECK(delegate.lastDecoration.bottom() < delegate.lastText.top());
    option.decorationPosition = QStyleOptionViewItem::Right;
    {
        QPainter painter(&image);
        delegate.paint(&painter, option, icon);
    }
    CHECK(delegate.lastDecoration.left() > delegate.lastText.right() - 1 && delegate.lastDecoration.right() <= 199);

    // In a view: check boxes, focus, and editing in place.
    NodeModel model;
    NodeModel::Node *a = model.add(nullptr, "acorn", 3);
    a->check = Qt::Unchecked;
    model.add(nullptr, "beech", 4);
    QTreeView view;
    view.setRootIsDecorated(false);
    auto *logging = new LoggingItemDelegate(&view);
    view.setItemDelegate(logging);
    view.resize(300, 150);
    view.setModel(&model);
    view.show();
    settle(&view);
    view.setFocus();
    view.setCurrentIndex(model.index(0, 0));
    grab(&view);
    CHECK(logging->texts.contains("acorn") && logging->texts.contains("4") && logging->checks >= 1 &&
          logging->focus >= 1);
    const QRect check = logging->lastCheck;
    CHECK(check.isValid() && view.visualRect(model.index(0, 0)).contains(check));
    click(view.viewport(), check.center());
    CHECK(a->check == Qt::Checked);
    key(&view, Qt::Key_Space, " ");
    CHECK(a->check == Qt::Unchecked);
    doubleClick(view.viewport(), view.visualRect(model.index(1, 0)).center());
    QLineEdit *editor = editorOf<QLineEdit>(view);
    CHECK(editor != nullptr && editor && editor->text() == "beech" &&
          editor->x() >= view.visualRect(model.index(1, 0)).left());
    if (editor) {
        editor->selectAll();
        type(editor, "birch");
        key(editor, Qt::Key_Return);
    }
    settle(&view); // the editor is deleted later
    CHECK(model.index(1, 0).data().toString() == "birch" && editorOf<QLineEdit>(view) == nullptr);
    doubleClick(view.viewport(), view.visualRect(model.index(1, 0)).center());
    if (QLineEdit *again = editorOf<QLineEdit>(view)) {
        again->setText("gone");
        key(again, Qt::Key_Escape);
    }
    CHECK(model.index(1, 0).data().toString() == "birch");
}

// The tree finds the column under the mouse while it is scrolled sideways.
static void testTreeScrolledColumns() {
    NodeModel model;
    model.add(nullptr, "almond", 1);
    QTreeView view;
    view.setRootIsDecorated(false);
    view.resize(220, 120);
    view.setModel(&model);
    view.header()->setStretchLastSection(false);
    view.setColumnWidth(0, 300);
    view.setColumnWidth(1, 300);
    view.show();
    settle(&view);
    view.horizontalScrollBar()->setValue(250);
    CHECK(view.horizontalScrollBar()->value() == 250);
    CHECK(view.columnAt(10) == 0 && view.columnAt(60) == 1 && view.columnViewportPosition(1) == 50);
    CHECK(view.indexAt(QPoint(60, 5)) == model.index(0, 1));
}

// Where the delegate puts the decoration of an item.
static void testDecorationPosition() {
    IconModel model(1);
    QStyledItemDelegate delegate;
    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, 120, 80);
    option.font = QApplication::font();
    option.fontMetrics = QFontMetrics(option.font);
    option.state = QStyle::State_Enabled;
    const QModelIndex index = model.index(0);
    const QSize left = delegate.sizeHint(option, index);
    option.decorationPosition = QStyleOptionViewItem::Top;
    const QSize top = delegate.sizeHint(option, index);
    CHECK(top.height() > left.height() && top.width() < left.width());
    const auto redAt = [&](QStyleOptionViewItem::Position position, QPoint p) {
        QImage image(120, 80, QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        QStyleOptionViewItem o = option;
        o.decorationPosition = position;
        delegate.paint(&painter, o, index);
        painter.end();
        return image.pixelColor(p.x(), p.y()) == QColor(255, 0, 0);
    };
    CHECK(redAt(QStyleOptionViewItem::Top, QPoint(60, 10)) && !redAt(QStyleOptionViewItem::Top, QPoint(10, 40)));
    CHECK(redAt(QStyleOptionViewItem::Bottom, QPoint(60, 70)) && !redAt(QStyleOptionViewItem::Bottom, QPoint(60, 10)));
    CHECK(redAt(QStyleOptionViewItem::Left, QPoint(10, 40)) && !redAt(QStyleOptionViewItem::Left, QPoint(110, 40)));
    CHECK(redAt(QStyleOptionViewItem::Right, QPoint(110, 40)) && !redAt(QStyleOptionViewItem::Right, QPoint(10, 40)));
}

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
    testListView();
    testIconMode();
    testTableView();
    testTreeScrolledColumns();
    testDecorationPosition();
    testDataWidgetMapper();
    testItemDelegate();
    if (g_failures == 0)
        std::puts("all item view tests passed");
    return g_failures == 0 ? 0 : 1;
}
