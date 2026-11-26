// MIT License - see LICENSE
#include <QtWidgets/qmaterialstyle.h>
#include <QtWidgets/qcombobox.h>
#include <QtCore/qcoreapplication.h>
#include <QtWidgets/qlineedit.h>

#include "qwidget_p.h"

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtGui/qstandarditemmodel.h>
#include <QtWidgets/qabstractitemdelegate.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>
#include <cstdlib>

// The list that drops down from a combo box. It is a child of the window of the combo box, above its other
// children, and not a window of its own.
class QComboPopup : public QWidget {
public:
    QComboPopup(QComboBox *combo, QWidget *parent) : QWidget(parent), m_combo(combo) {
        setFocusPolicy(Qt::ClickFocus);
        setMouseTracking(true);
        hide();
    }

    bool material() const { return qobject_cast<const QMaterialStyle *>(m_combo->style()) != nullptr; }

    int itemHeight() const {
        // Material's rows are high enough for a finger.
        int height = material() ? 48 : fontMetrics().height() + 6;
        // A delegate sizes the items; the list takes the highest.
        if (QAbstractItemDelegate *delegate = m_combo->itemDelegate()) {
            QStyleOptionViewItem option;
            option.initFrom(m_combo);
            for (int i = 0; i < m_combo->count(); ++i)
                height = std::max(height, delegate->sizeHint(option, m_combo->itemIndex(i)).height());
        }
        return height;
    }
    int visibleItems() const { return std::min(m_combo->count(), m_visible); }

    void open() {
        QWidget *top = m_combo->window();
        if (parentWidget() != top)
            setParent(top);
        setFont(m_combo->font());
        QPalette colors = m_combo->palette();
        if (const auto *style = qobject_cast<const QMaterialStyle *>(m_combo->style())) {
            // Material marks the current row of a menu quietly; the rows and their delegate take it from the palette.
            colors.setColor(QPalette::Highlight, style->color(QMaterialStyle::Role::SecondaryContainer));
            colors.setColor(QPalette::HighlightedText, style->color(QMaterialStyle::Role::OnSecondaryContainer));
        }
        setPalette(colors);
        const int rows = std::max(1, std::min(m_combo->count(), m_combo->maxVisibleItems()));
        const QPoint below = m_combo->mapTo(top, QPoint(0, m_combo->height()));
        const int above = below.y() - m_combo->height();
        const int room = std::max(0, top->height() - below.y());
        // Open below the combo box, or above it if there is more room there.
        const bool downwards = rows * itemHeight() + 2 <= room || room >= above;
        m_visible = std::max(1, std::min(rows, ((downwards ? room : above) - 2) / itemHeight()));
        const int height = m_visible * itemHeight() + 2;
        // As wide as the combo box, or as the minimum width given to its view(), within the window.
        const int width = std::min(top->width() - below.x(), std::max(m_combo->width(), m_combo->view()->minimumWidth()));
        setGeometry(QRect(QPoint(below.x(), downwards ? below.y() : above - height), QSize(width, height)));
        m_hover = m_combo->currentIndex();
        m_top = 0;
        ensureVisible(m_hover);
        raise();
        show();
        setFocus();
        QtPrivate::WidgetAccess::setPopup(this, this);
        m_combo->update();
    }

    void close() { hide(); }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setFont(font());
        const QPalette::ColorGroup group = QPalette::Active;
        if (material()) {
            // The panel of a Material menu.
            QStyleOption panel;
            panel.initFrom(this);
            panel.rect = rect();
            m_combo->style()->drawPrimitive(QStyle::PE_PanelMenu, &panel, &painter, this);
        } else {
            painter.fillRect(rect(), palette().color(group, QPalette::Base));
            painter.setPen(palette().color(group, QPalette::Window).darker(150));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(rect().adjusted(0, 0, -1, -1));
        }
        const int h = itemHeight();
        for (int row = 0; row < visibleItems(); ++row) {
            const int index = m_top + row;
            const QRect item(1, 1 + row * h, width() - 2, h);
            if (m_combo->isSeparator(index)) {
                painter.setPen(palette().color(group, QPalette::Window).darker(130));
                painter.drawLine(item.left() + 4, item.center().y(), item.right() - 4, item.center().y());
                continue;
            }
            const bool enabled = m_combo->isItemEnabled(index);
            const bool hot = index == m_hover && enabled;
            if (QAbstractItemDelegate *delegate = m_combo->itemDelegate()) {
                QStyleOptionViewItem option;
                option.initFrom(this);
                option.rect = item;
                option.state &= ~QStyle::State_Enabled;
                if (enabled)
                    option.state |= QStyle::State_Enabled;
                if (hot)
                    option.state |= QStyle::State_Selected | QStyle::State_MouseOver;
                option.decorationSize = m_combo->iconSize();
                painter.save();
                delegate->paint(&painter, option, m_combo->itemIndex(index));
                painter.restore();
                continue;
            }
            if (hot)
                painter.fillRect(item, palette().color(group, QPalette::Highlight));
            painter.setPen(enabled ? palette().color(group, hot ? QPalette::HighlightedText : QPalette::Text)
                                   : palette().color(QPalette::Disabled, QPalette::Text));
            int left = material() ? 16 : 7;
            const QIcon icon = m_combo->itemIcon(index);
            if (!icon.isNull()) {
                const QSize size = m_combo->iconSize();
                icon.paint(&painter, QRect(item.left() + left, item.top(), size.width(), item.height()),
                           Qt::AlignCenter,
                           !enabled ? QIcon::Disabled
                           : hot    ? QIcon::Selected
                                    : QIcon::Normal);
                left += size.width() + 4;
            }
            painter.drawText(item.adjusted(left, 0, -4, 0), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                             m_combo->itemText(index));
        }
    }
    void mouseMoveEvent(QMouseEvent *event) override { hover(indexAt(event->pos())); }
    void mousePressEvent(QMouseEvent *event) override {
        hover(indexAt(event->pos()));
        event->accept();
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        const int index = indexAt(event->pos());
        if (index >= 0 && rect().contains(event->pos()) && m_combo->isItemEnabled(index))
            commit(index);
        event->accept();
    }
    void wheelEvent(QWheelEvent *event) override {
        const int notches = event->angleDelta().y() / 120;
        m_top = std::clamp(m_top - 3 * notches, 0, std::max(0, m_combo->count() - visibleItems()));
        update();
        event->accept();
    }
    void keyPressEvent(QKeyEvent *event) override {
        const int count = m_combo->count();
        switch (event->key()) {
        case Qt::Key_Down:
            hover(m_combo->nextEnabled(m_hover + 1, 1));
            break;
        case Qt::Key_Up:
            hover(m_combo->nextEnabled(m_hover - 1, -1));
            break;
        case Qt::Key_PageDown:
            hover(m_combo->nextEnabled(std::min(count - 1, m_hover + visibleItems()), -1));
            break;
        case Qt::Key_PageUp:
            hover(m_combo->nextEnabled(std::max(0, m_hover - visibleItems()), 1));
            break;
        case Qt::Key_Home:
            hover(m_combo->nextEnabled(0, 1));
            break;
        case Qt::Key_End:
            hover(m_combo->nextEnabled(count - 1, -1));
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            if (m_hover >= 0 && m_combo->isItemEnabled(m_hover))
                commit(m_hover);
            break;
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            close();
            break;
        default:
            event->ignore();
            return;
        }
        event->accept();
    }
    void hideEvent(QHideEvent *) override {
        QtPrivate::WidgetAccess::setPopup(this, nullptr);
        m_combo->setFocus();
        m_combo->update();
    }

private:
    int indexAt(const QPoint &pos) const {
        const int row = (pos.y() - 1) / itemHeight();
        const int index = m_top + row;
        return pos.y() >= 1 && row < visibleItems() && index < m_combo->count() ? index : -1;
    }
    void ensureVisible(int index) {
        if (index < 0)
            return;
        if (index < m_top)
            m_top = index;
        else if (index >= m_top + visibleItems())
            m_top = index - visibleItems() + 1;
    }
    void hover(int index) {
        if (index < 0 || index == m_hover || !m_combo->isItemEnabled(index))
            return;
        m_hover = index;
        ensureVisible(index);
        update();
        emit m_combo->highlighted(index);
        emit m_combo->textHighlighted(m_combo->itemText(index));
    }
    void commit(int index) {
        close();
        m_combo->choose(index);
    }

    QComboBox *m_combo;
    int m_hover = -1;
    int m_top = 0;
    int m_visible = 1;
};

/*!
    \class QComboBox
    \brief The QComboBox class is a button that drops down a list of items to choose from.

    A combo box shows the current item. Clicking it opens a list in which the user chooses another one, and the
    arrow keys and the mouse wheel change it without opening the list. Connect to currentIndexChanged() or
    activated():

    \code
    QComboBox *font = new QComboBox(window);
    font->addItems({"Small", "Medium", "Large"});
    connect(font, &QComboBox::currentIndexChanged, [](int index) { qDebug() << index; });
    \endcode

    The list is a widget inside the window of the combo box, not a window of its own, so it never reaches outside
    the window. It opens below the combo box, or above it if there is more room there.

    Combo boxes: closed, disabled, and open, with the list below it.

    \image widgets/qcombobox.png QComboBox in the light color scheme

    \differences
    The items are rows of a model, a QStandardItemModel unless setModel() sets another: disabled items are shown
    grey and cannot be chosen. The list is drawn by the combo box, or by the delegate of setItemDelegate(): a view
    set with setView() keeps its settings but does not draw the list, and labelDrawingMode() is only kept. The size
    hint follows sizeAdjustPolicy() and minimumContentsLength(), with AdjustToContentsOnFirstShow sized as
    AdjustToContents. Typing does not jump to an item of a combo box that is not editable.
    \enddifferences

    \sa QSpinBox, QRadioButton
*/

/*!
    Constructs an empty combo box with the given \a parent.
*/
QComboBox::QComboBox(QWidget *parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_model = new QStandardItemModel(0, 1, this);
    connectModel();
}

/*!
    Destroys the combo box.
*/
QComboBox::~QComboBox() {
    for (const QMetaObject::Connection &connection : std::as_const(m_modelConnections))
        QObject::disconnect(connection);
    delete m_popup.data();
}

/*!
    Returns the number of items.
*/
int QComboBox::count() const { return m_model ? m_model->rowCount(m_root) : 0; }

QModelIndex QComboBox::itemIndex(int row) const {
    return m_model ? m_model->index(row, m_modelColumn, m_root) : QModelIndex();
}

bool QComboBox::isItemEnabled(int row) const {
    const QModelIndex index = itemIndex(row);
    return index.isValid() && (m_model->flags(index) & Qt::ItemIsEnabled);
}

int QComboBox::nextEnabled(int from, int step) const {
    for (int row = from; row >= 0 && row < count(); row += step)
        if (isItemEnabled(row))
            return row;
    return -1;
}

void QComboBox::connectModel() {
    for (const QMetaObject::Connection &connection : std::as_const(m_modelConnections))
        QObject::disconnect(connection);
    m_modelConnections.clear();
    if (!m_model)
        return;
    m_modelConnections = {
        QObject::connect(m_model, &QAbstractItemModel::rowsInserted, this,
                         [this](const QModelIndex &parent, int first, int last) { rowsInserted(parent, first, last); }),
        QObject::connect(m_model, &QAbstractItemModel::rowsRemoved, this,
                         [this](const QModelIndex &parent, int first, int last) { rowsRemoved(parent, first, last); }),
        QObject::connect(
            m_model, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) { itemsChanged(topLeft, bottomRight); }),
        QObject::connect(m_model, &QAbstractItemModel::modelReset, this, [this] { modelWasReset(); }),
        QObject::connect(m_model, &QAbstractItemModel::layoutChanged, this, [this] { modelWasReset(); }),
        QObject::connect(m_model, &QObject::destroyed, this,
                         [this] {
                             m_model = nullptr;
                             m_modelConnections.clear();
                             modelWasReset();
                         }),
    };
}

void QComboBox::announceCurrent() {
    if (m_edit && m_current >= 0)
        m_edit->setText(itemText(m_current));
    updateGeometry();
    update();
    emit currentIndexChanged(m_current);
    m_announcedText = currentText();
    emit currentTextChanged(m_announcedText);
}

void QComboBox::rowsInserted(const QModelIndex &parent, int first, int last) {
    if (parent != QModelIndex(m_root))
        return;
    updateGeometry();
    update();
    const int added = last - first + 1;
    if (m_current < 0 && added == count()) {
        // The first items of an empty combo box: the first one becomes current.
        m_current = 0;
        announceCurrent();
    } else if (m_current >= first) {
        m_current += added;
    }
}

void QComboBox::rowsRemoved(const QModelIndex &parent, int first, int last) {
    if (parent != QModelIndex(m_root))
        return;
    updateGeometry();
    update();
    if (m_current > last) {
        m_current -= last - first + 1;
    } else if (m_current >= first) {
        // The current item went: the one that takes its place becomes current.
        m_current = count() > 0 ? std::min(first, count() - 1) : -1;
        if (count() == 0)
            hidePopup();
        announceCurrent();
    }
}

void QComboBox::itemsChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) {
    if (topLeft.parent() != QModelIndex(m_root))
        return;
    updateGeometry();
    update();
    if (m_popup)
        m_popup->update();
    if (!m_edit && m_current >= topLeft.row() && m_current <= bottomRight.row() && currentText() != m_announcedText) {
        m_announcedText = currentText();
        emit currentTextChanged(m_announcedText);
    }
}

void QComboBox::modelWasReset() {
    hidePopup();
    const int current = m_current < count() ? m_current : (count() > 0 ? 0 : -1);
    const bool changed = current != m_current || currentText() != m_announcedText;
    m_current = current;
    updateGeometry();
    update();
    if (changed)
        announceCurrent();
}

/*!
    \fn QAbstractItemModel *QComboBox::model() const
    \since 6.0

    Returns the model that holds the items. Unless setModel() set another, it is a QStandardItemModel of the combo
    box, so that its items can be changed as QStandardItem objects:

    \code
    auto *model = qobject_cast<QStandardItemModel *>(combo->model());
    model->item(1)->setEnabled(false); // shown grey; it cannot be chosen
    \endcode
*/

/*!
    \since 6.0

    Sets the model that holds the items to \a model, which the combo box does not take ownership of. The items are
    the rows of the modelColumn(); the first one becomes current. The model the combo box made is deleted when its
    parent is.

    \sa model()
*/
void QComboBox::setModel(QAbstractItemModel *model) {
    if (!model || model == m_model)
        return;
    hidePopup();
    m_model = model;
    if (auto *list = qobject_cast<QListView *>(m_view))
        list->setModel(model);
    connectModel();
    m_current = -1;
    modelWasReset();
    if (m_current < 0 && count() > 0) {
        m_current = 0;
        announceCurrent();
    }
}

/*!
    \fn int QComboBox::modelColumn() const
    \since 6.0

    Returns the column of the model whose items the combo box shows, 0 by default.
*/

/*!
    \since 6.0

    Sets the column of the model whose items the combo box shows to \a visibleColumn.
*/
void QComboBox::setModelColumn(int visibleColumn) {
    if (visibleColumn == m_modelColumn || visibleColumn < 0)
        return;
    m_modelColumn = visibleColumn;
    updateGeometry();
    update();
    if (currentText() != m_announcedText) {
        m_announcedText = currentText();
        emit currentTextChanged(m_announcedText);
    }
}

/*!
    Returns the text of the item at \a index, or an empty string if there is no such item.

    \sa setItemText()
*/
QString QComboBox::itemText(int index) const { return itemData(index, Qt::DisplayRole).toString(); }

/*!
    Sets the text of the item at \a index to \a text. Nothing happens if there is no such item.

    \sa itemText()
*/
void QComboBox::setItemText(int index, const QString &text) {
    if (index >= 0 && index < count() && itemText(index) != text)
        m_model->setData(itemIndex(index), text, Qt::DisplayRole);
}

/*!
    \fn int QComboBox::currentIndex() const

    Returns the index of the current item, or -1 if there is none. The first item added becomes the current one.

    \sa setCurrentIndex()
*/

/*!
    Returns the index of the first item whose text matches \a text as \a flags say, or -1 if there is none. By
    default the text must be the same.

    \sa findData()
*/
int QComboBox::findText(const QString &text, Qt::MatchFlags flags) const {
    return findData(text, Qt::DisplayRole, flags);
}

/*!
    \since 6.0

    Sets the text shown while there is no current item to \a placeholderText.
*/
void QComboBox::setPlaceholderText(const QString &placeholderText) {
    m_placeholder = placeholderText;
    updateGeometry();
    update();
}

/*!
    \fn QString QComboBox::placeholderText() const
    \since 6.0

    Returns the text shown while there is no current item.
*/

/*!
    \since 6.0

    Returns the view of the list. Nt draws the list itself: the view is a hidden QListView of the model, whose
    minimum width the list takes, so \c{view()->setMinimumWidth(280)} makes the list that wide.
*/
QAbstractItemView *QComboBox::view() const {
    if (!m_view) {
        auto *self = const_cast<QComboBox *>(this); // NOLINT(cppcoreguidelines-pro-type-const-cast): made on demand
        auto *list = new QListView(self);
        list->hide();
        if (m_model) {
            list->setModel(m_model);
            list->setRootIndex(m_root);
        }
        m_view = list;
    }
    return m_view;
}

/*!
    \since 6.0

    Returns the delegate that draws the items of the list, or \c nullptr if the combo box draws them.
*/
QAbstractItemDelegate *QComboBox::itemDelegate() const { return m_delegate.data(); }

/*!
    \since 6.0

    Makes \a delegate draw and size the items of the list. The combo box does not take ownership of it.
*/
void QComboBox::setItemDelegate(QAbstractItemDelegate *delegate) {
    m_delegate = delegate;
    if (m_popup)
        m_popup->update();
}

/*!
    \fn int QComboBox::maxVisibleItems() const

    Returns how many items the list shows at most, 10 by default. A longer list scrolls.

    \sa setMaxVisibleItems()
*/

/*!
    Sets the number of items that the list shows at most to \a count.

    \sa maxVisibleItems()
*/
void QComboBox::setMaxVisibleItems(int count) { m_maxVisible = std::max(1, count); }

/*!
    Returns whether the list of items is open.

    \sa showPopup()
*/
bool QComboBox::isPopupVisible() const { return m_popup && !m_popup->isHidden(); }

/*!
    Adds an item with \a text at the end, with \a userData as its data for Qt::UserRole.

    \sa insertItem(), itemData()
*/
void QComboBox::addItem(const QString &text, const QVariant &userData) { insertItem(count(), text, userData); }

/*!
    Returns the data of the item at \a index for \a role: Qt::UserRole by default, the data given to addItem(). For
    Qt::DisplayRole and Qt::EditRole it is the text. It is an invalid QVariant if there is no such item or data.

    \sa setItemData(), currentData()
*/
QVariant QComboBox::itemData(int index, int role) const {
    const QModelIndex item = itemIndex(index);
    return item.isValid() ? m_model->data(item, role) : QVariant();
}

/*!
    Sets the data of the item at \a index for \a role to \a value; for Qt::DisplayRole and Qt::EditRole that is its
    text. Nothing happens if there is no such item.

    \sa itemData()
*/
void QComboBox::setItemData(int index, const QVariant &value, int role) {
    const QModelIndex item = itemIndex(index);
    if (item.isValid())
        m_model->setData(item, value, role);
}

/*!
    \fn QVariant QComboBox::currentData(int role) const

    Returns the data of the current item for \a role, Qt::UserRole by default, or an invalid QVariant if there is no
    current item.

    \sa itemData(), currentIndex()
*/

/*!
    Returns the index of the first item whose data for \a role is \a data, or -1 if there is none. With the default
    \a flags, Qt::MatchExactly, the values must be equal; Qt::MatchFixedString compares them as strings without
    case, and Qt::MatchContains and Qt::MatchStartsWith look for the text in them.

    \sa itemData(), findText()
*/
int QComboBox::findData(const QVariant &data, int role, Qt::MatchFlags flags) const {
    const Qt::CaseSensitivity cs = flags & Qt::MatchCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    const QString wanted = data.toString();
    for (int i = 0; i < count(); ++i) {
        const QVariant value = itemData(i, role);
        if (flags & Qt::MatchContains) {
            if (value.toString().contains(wanted, cs))
                return i;
        } else if (flags & Qt::MatchStartsWith) {
            if (value.toString().startsWith(wanted, cs))
                return i;
        } else if (flags & Qt::MatchFixedString) {
            if (value.toString().compare(wanted, cs) == 0)
                return i;
        } else if (value == data) {
            return i;
        }
    }
    return -1;
}

/*!
    Adds an item for each of \a texts at the end.

    \sa addItem()
*/
void QComboBox::addItems(const QStringList &texts) {
    for (const QString &text : texts)
        addItem(text);
}

/*!
    \overload
    \since 6.0

    Adds an item with \a icon and \a text to the end, with \a userData as its data for Qt::UserRole.
*/
void QComboBox::addItem(const QIcon &icon, const QString &text, const QVariant &userData) {
    insertItem(count(), icon, text, userData);
}

/*!
    \overload
    \since 6.0

    Inserts an item with \a icon and \a text at \a index, with \a userData as its data for Qt::UserRole.
*/
void QComboBox::insertItem(int index, const QIcon &icon, const QString &text, const QVariant &userData) {
    const int at = std::clamp(index, 0, count());
    insertItem(at, text, userData);
    setItemIcon(at, icon);
}

/*!
    \since 6.0

    Returns the icon of the item at \a index; a null icon if it has none.
*/
QIcon QComboBox::itemIcon(int index) const { return itemData(index, Qt::DecorationRole).value<QIcon>(); }

/*!
    \since 6.0

    Sets the icon of the item at \a index to \a icon, shown before its text in iconSize().
*/
void QComboBox::setItemIcon(int index, const QIcon &icon) {
    setItemData(index, icon.isNull() ? QVariant() : QVariant::fromValue(icon), Qt::DecorationRole);
    updateGeometry();
    update();
}

/*!
    \since 6.0

    Returns the size the icons of the items are shown in: what setIconSize() set, or the small icon size of the
    style.
*/
QSize QComboBox::iconSize() const {
    if (m_iconSize.isValid())
        return m_iconSize;
    const int extent = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
    return QSize(extent > 0 ? extent : 16, extent > 0 ? extent : 16);
}

/*!
    \since 6.0

    Sets the size the icons of the items are shown in to \a size.
*/
void QComboBox::setIconSize(const QSize &size) {
    m_iconSize = size;
    updateGeometry();
    update();
}

/*!
    Inserts an item with \a text and, for Qt::UserRole, \a userData at \a index, which is brought into the range of
    the items. The current item stays the same, except that the first item that is added becomes the current one.

    \sa addItem(), removeItem()
*/
void QComboBox::insertItem(int index, const QString &text, const QVariant &userData) {
    if (!m_model || count() >= m_maxCount)
        return;
    const int at = std::clamp(index, 0, count());
    auto *standard = qobject_cast<QStandardItemModel *>(m_model);
    QStandardItem *parentItem = standard && m_root.isValid() ? standard->itemFromIndex(m_root) : nullptr;
    if (standard && (parentItem || !m_root.isValid())) {
        // The item has its data before the row appears, so that currentTextChanged() has the text.
        auto *item = new QStandardItem(text);
        if (userData.isValid())
            item->setData(userData, Qt::UserRole);
        QList<QStandardItem *> row(m_modelColumn + 1, nullptr);
        row.back() = item;
        if (parentItem)
            parentItem->insertRow(at, row);
        else
            standard->insertRow(at, row);
        return;
    }
    if (!m_model->insertRows(at, 1, m_root))
        return;
    m_model->setData(itemIndex(at), text, Qt::DisplayRole);
    if (userData.isValid())
        m_model->setData(itemIndex(at), userData, Qt::UserRole);
}

/*!
    Removes the item at \a index. If it was the current one, the item that takes its place becomes current.

    \sa clear(), insertItem()
*/
void QComboBox::removeItem(int index) {
    if (index >= 0 && index < count())
        m_model->removeRows(index, 1, m_root);
}

/*!
    Removes all items.

    \sa removeItem()
*/
void QComboBox::clear() {
    hidePopup();
    if (count() > 0)
        m_model->removeRows(0, count(), m_root);
}

/*!
    Makes the item at \a index the current one. An \a index outside the items makes it -1, which shows nothing.
    Emits currentIndexChanged() and currentTextChanged() if the item changes.

    \sa currentIndex()
*/
void QComboBox::setCurrentIndex(int index) {
    const int target = index >= 0 && index < count() ? index : -1;
    if (target == m_current)
        return;
    m_current = target;
    announceCurrent();
}

/*!
    Makes the first item with \a text the current one. Nothing happens if there is none.

    \sa currentText(), findText()
*/
void QComboBox::setCurrentText(const QString &text) {
    if (m_edit) {
        setEditText(text);
        return;
    }
    const int index = findText(text);
    if (index >= 0)
        setCurrentIndex(index);
}

/*!
    Returns the text of the current item, or, in an editable combo box, the text of its line edit.

    \sa setCurrentText(), currentIndex()
*/
QString QComboBox::currentText() const { return m_edit ? m_edit->text() : itemText(m_current); }

/*!
    \since 6.0

    Makes the combo box editable if \a editable is \c true: a line edit takes the place of the text, where the user
    can type text that is not one of the items. Return adds the text as an item, as insertPolicy() says, and makes it
    the current one. A combo box that is not editable, the default, only offers its items.

    \sa isEditable(), lineEdit(), editTextChanged()
*/
void QComboBox::setEditable(bool editable) {
    if (editable == isEditable())
        return;
    if (!editable) {
        delete m_edit;
        m_edit = nullptr;
        update();
        return;
    }
    adoptLineEdit(new QLineEdit(this));
}

// Makes `edit` the line edit of the combo box, in place of the one it had.
void QComboBox::adoptLineEdit(QLineEdit *edit) {
    if (m_edit && m_edit != edit)
        delete m_edit;
    m_edit = edit;
    m_edit->setParent(this);
    m_edit->setFrame(false);
    m_edit->setText(itemText(m_current));
    m_edit->setGeometry(editRect());
    QObject::connect(m_edit, &QLineEdit::textChanged, this, [this](const QString &text) {
        emit editTextChanged(text);
        emit currentTextChanged(text);
    });
    QObject::connect(m_edit, &QLineEdit::returnPressed, this, [this] { commitEdit(); });
    if (isVisible())
        m_edit->show();
    update();
}

/*!
    \fn bool QComboBox::isEditable() const
    \since 6.0

    Returns \c true if the user can type in the combo box.

    \sa setEditable()
*/

/*!
    \fn QLineEdit *QComboBox::lineEdit() const
    \since 6.0

    Returns the line edit of an editable combo box, or \c nullptr if it is not editable.
*/

/*!
    \enum QComboBox::InsertPolicy
    \since 6.0

    This enum tells where the text that the user types in an editable combo box goes when Return is pressed.

    \value NoInsert It is not added.
    \value InsertAtTop It is added as the first item.
    \value InsertAtCurrent It replaces the text of the current item.
    \value InsertAtBottom It is added as the last item. This is the default.
    \value InsertAfterCurrent It is added after the current item.
    \value InsertBeforeCurrent It is added before the current item.
    \value InsertAlphabetically It is added where it sorts among the items.
*/

/*!
    \fn QComboBox::InsertPolicy QComboBox::insertPolicy() const
    \since 6.0

    Returns where typed text goes when Return is pressed. The default is InsertAtBottom.
*/

/*!
    \fn void QComboBox::setInsertPolicy(InsertPolicy policy)
    \since 6.0

    Sets where typed text goes when Return is pressed to \a policy.
*/

/*!
    \since 6.0

    Sets the text of the line edit of an editable combo box to \a text, without choosing an item. Does nothing for a
    combo box that is not editable.

    \sa clearEditText(), currentText()
*/
void QComboBox::setEditText(const QString &text) {
    if (m_edit)
        m_edit->setText(text);
}

/*!
    \fn void QComboBox::clearEditText()
    \since 6.0

    Clears the line edit of an editable combo box.
*/

QRect QComboBox::editRect() const {
    QStyleOption option;
    option.initFrom(this);
    const QRect arrow = style()->subElementRect(QStyle::SE_ComboBoxArrow, &option, this);
    return rect().adjusted(3, 3, -std::max(arrow.width(), 16) - 1, -3);
}

// Return in the line edit: the text becomes an item, as the insert policy says, and the current one.
void QComboBox::commitEdit() {
    const QString text = m_edit->text();
    if (text.isEmpty())
        return;
    int index = m_duplicates ? -1 : findText(text);
    if (index < 0) {
        switch (m_insertPolicy) {
        case NoInsert:
            break;
        case InsertAtTop:
            index = 0;
            break;
        case InsertAtCurrent:
            if (m_current >= 0) {
                setItemText(m_current, text);
                index = m_current;
            }
            break;
        case InsertAtBottom:
            index = count();
            break;
        case InsertAfterCurrent:
            index = m_current + 1;
            break;
        case InsertBeforeCurrent:
            index = std::max(0, m_current);
            break;
        case InsertAlphabetically:
            index = 0;
            while (index < count() && itemText(index) < text)
                ++index;
            break;
        }
        if (index >= 0 && m_insertPolicy != InsertAtCurrent)
            insertItem(index, text);
    }
    if (index >= 0)
        choose(index);
}

/*!
    \reimp
    \since 6.0
*/
void QComboBox::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_edit)
        m_edit->setGeometry(editRect());
}

/*!
    Opens the list of items.

    \sa hidePopup()
*/
void QComboBox::showPopup() {
    if (count() == 0 || !isEnabled() || isPopupVisible() || !window())
        return;
    if (!m_popup)
        m_popup = new QComboPopup(this, window());
    static_cast<QComboPopup *>(m_popup.data())->open();
}

/*!
    Closes the list of items, if it is open.

    \sa showPopup()
*/
void QComboBox::hidePopup() {
    if (isPopupVisible())
        static_cast<QComboPopup *>(m_popup.data())->close();
}

// The user chose the item at `index`.
void QComboBox::choose(int index) {
    setCurrentIndex(index);
    emit activated(index);
    emit textActivated(itemText(index));
}

/*!
    \reimp
*/
QSize QComboBox::sizeHint() const {
    const QFontMetrics fm = fontMetrics();
    int width = count() == 0 ? fm.horizontalAdvance(QString(7, QChar('x'))) : 0;
    width = std::max(width, fm.horizontalAdvance(m_placeholder));
    for (int i = 0; i < count(); ++i)
        width = std::max(width, fm.horizontalAdvance(itemText(i)));
    int height = fm.height();
    bool icons = false;
    for (int i = 0; i < count() && !icons; ++i)
        icons = !itemIcon(i).isNull();
    const int minimum = m_minimumContents * fm.horizontalAdvance(QChar('x'));
    if (m_sizePolicy == AdjustToMinimumContentsLengthWithIcon)
        width = minimum;
    else
        width = std::max(width, minimum);
    if (icons || (m_sizePolicy == AdjustToMinimumContentsLengthWithIcon && m_minimumContents > 0)) {
        width += iconSize().width() + 4;
        height = std::max(height, iconSize().height());
    }
    QStyleOptionComboBox option;
    initStyleOption(&option);
    return style()->sizeFromContents(QStyle::CT_ComboBox, &option, QSize(width, height), this);
}

/*!
    \reimp
*/
QSize QComboBox::minimumSizeHint() const { return sizeHint(); }

/*!
    \reimp
*/
void QComboBox::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    painter.setFont(font());
    QStyleOptionComboBox option;
    initStyleOption(&option);
    style()->drawControl(QStyle::CE_ComboBox, &option, &painter, this);
}

/*!
    \reimp
*/
void QComboBox::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    m_down = true;
    update();
    event->accept();
}

/*!
    \reimp
*/
void QComboBox::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || !m_down) {
        event->ignore();
        return;
    }
    m_down = false;
    update();
    if (rect().contains(event->pos()))
        showPopup();
    event->accept();
}

/*!
    \reimp
*/
void QComboBox::wheelEvent(QWheelEvent *event) {
    const int notches = event->angleDelta().y() / 120;
    if (notches == 0 || count() == 0) {
        event->ignore();
        return;
    }
    int index = m_current;
    for (int step = notches > 0 ? -1 : 1, left = std::abs(notches); left > 0; --left) {
        const int next = nextEnabled(index + step, step);
        if (next < 0)
            break;
        index = next;
    }
    if (index != m_current && index >= 0)
        choose(index);
    event->accept();
}

/*!
    \reimp
*/
void QComboBox::keyPressEvent(QKeyEvent *event) {
    const int key = event->key();
    int target = m_current;
    if (key == Qt::Key_Down && (event->modifiers() & Qt::AltModifier)) {
        showPopup();
        event->accept();
        return;
    }
    switch (key) {
    case Qt::Key_Up:
        target = nextEnabled(m_current - 1, -1);
        break;
    case Qt::Key_Down:
        target = nextEnabled(m_current + 1, 1);
        break;
    case Qt::Key_Home:
        target = nextEnabled(0, 1);
        break;
    case Qt::Key_End:
        target = nextEnabled(count() - 1, -1);
        break;
    case Qt::Key_F4:
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        showPopup();
        event->accept();
        return;
    default:
        event->ignore();
        return;
    }
    if (target >= 0 && target < count() && target != m_current)
        choose(target);
    event->accept();
}

/*!
    \reimp
*/
void QComboBox::changeEvent(QEvent *event) {
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
        updateGeometry();
    update();
    QWidget::changeEvent(event);
}

/*!
    \enum QComboBox::SizeAdjustPolicy
    \since 6.0

    How the size hint of the combo box follows its items.

    \value AdjustToContents It is as wide as the widest item.
    \value AdjustToContentsOnFirstShow As AdjustToContents; Qt measures only the items there are when it is first
           shown.
    \value AdjustToMinimumContentsLengthWithIcon It is as wide as minimumContentsLength() characters and an icon.
*/

/*!
    \enum QComboBox::LabelDrawingMode
    \since 6.0

    What draws the label of the current item.

    \value UseStyle The style.
    \value UseDelegate The delegate of the list; Nt keeps the value and uses the style.
*/

/*!
    \since 6.0

    Fills \a option with the state of the combo box, for the style to draw it.
*/
void QComboBox::initStyleOption(QStyleOptionComboBox *option) const {
    if (!option)
        return;
    option->initFrom(this);
    option->currentText = m_edit ? QString() : currentText();
    if (!m_edit && m_current < 0 && !m_placeholder.isEmpty())
        option->currentText = m_placeholder;
    option->currentIcon = m_edit ? QIcon() : itemIcon(m_current);
    option->iconSize = iconSize();
    option->editable = m_edit != nullptr;
    option->frame = m_frame;
    option->state |= m_down ? QStyle::State_Sunken : QStyle::State_Raised;
    if (isPopupVisible())
        option->state |= QStyle::State_On;
}

bool QComboBox::isSeparator(int row) const {
    return itemData(row, Qt::AccessibleDescriptionRole).toString() == QLatin1String("separator");
}

/*!
    \since 6.0

    Inserts an item for each text of \a list, from \a index on.
*/
void QComboBox::insertItems(int index, const QStringList &list) {
    int at = std::clamp(index, 0, count());
    for (const QString &text : list)
        insertItem(at++, text);
}

/*!
    \since 6.0

    Inserts a separator at \a index: a line in the list that cannot be chosen.
*/
void QComboBox::insertSeparator(int index) {
    const int at = std::clamp(index, 0, count());
    insertItem(at, QString());
    setItemData(at, QStringLiteral("separator"), Qt::AccessibleDescriptionRole);
    if (auto *standard = qobject_cast<QStandardItemModel *>(m_model))
        if (QStandardItem *item = standard->itemFromIndex(itemIndex(at)))
            item->setFlags(Qt::NoItemFlags);
}

/*!
    \fn int QComboBox::maxCount() const
    \since 6.0

    Returns the largest number of items the combo box takes. The default is the largest \c int.
*/

/*!
    \since 6.0

    Sets the largest number of items to \a max; the items past it are removed.
*/
void QComboBox::setMaxCount(int max) {
    m_maxCount = std::max(0, max);
    while (count() > m_maxCount)
        removeItem(count() - 1);
}

/*!
    \fn bool QComboBox::duplicatesEnabled() const
    \since 6.0

    Returns whether a text the user enters is added as a new item when an item has it already. The default is
    \c false.
*/

/*!
    \fn void QComboBox::setDuplicatesEnabled(bool enable)
    \since 6.0

    Sets whether text the user enters is added again when an item has it to \a enable.
*/

/*!
    \fn bool QComboBox::hasFrame() const
    \since 6.0

    Returns whether the combo box draws a frame. The default is \c true.
*/

/*!
    \since 6.0

    Sets whether the combo box draws a frame to \a frame.
*/
void QComboBox::setFrame(bool frame) {
    m_frame = frame;
    update();
}

/*!
    \fn int QComboBox::minimumContentsLength() const
    \since 6.0

    Returns how many characters wide the combo box is at least. The default is 0.
*/

/*!
    \since 6.0

    Makes the combo box at least \a characters characters wide.
*/
void QComboBox::setMinimumContentsLength(int characters) {
    m_minimumContents = std::max(0, characters);
    updateGeometry();
}

/*!
    \fn QComboBox::SizeAdjustPolicy QComboBox::sizeAdjustPolicy() const
    \since 6.0

    Returns how the size hint follows the items.
*/

/*!
    \since 6.0

    Sets how the size hint follows the items to \a policy.
*/
void QComboBox::setSizeAdjustPolicy(SizeAdjustPolicy policy) {
    m_sizePolicy = policy;
    updateGeometry();
}

/*!
    \fn QComboBox::LabelDrawingMode QComboBox::labelDrawingMode() const
    \since 6.0

    Returns what draws the label of the current item.
*/

/*!
    \since 6.0

    Sets what draws the label of the current item to \a labelDrawing.
*/
void QComboBox::setLabelDrawingMode(LabelDrawingMode labelDrawing) {
    m_labelMode = labelDrawing;
    update();
}

/*!
    \fn QModelIndex QComboBox::rootModelIndex() const
    \since 6.0

    Returns the parent in the model of the items, an invalid index for the top level by default.
*/

/*!
    \since 6.0

    Makes the children of \a index in the model the items of the combo box; the first one becomes current.
*/
void QComboBox::setRootModelIndex(const QModelIndex &index) {
    if (QModelIndex(m_root) == index)
        return;
    m_root = QPersistentModelIndex(index);
    if (m_view)
        m_view->setRootIndex(index);
    m_current = count() > 0 ? 0 : -1;
    hidePopup();
    announceCurrent();
}

/*!
    \since 6.0

    Replaces the view of the list with \a itemView, which the combo box takes ownership of; the old one is
    deleted. The view gets the model and keeps settings such as its delegate; the combo box draws the list.
*/
void QComboBox::setView(QAbstractItemView *itemView) {
    if (!itemView || itemView == m_view)
        return;
    delete m_view;
    m_view = itemView;
    itemView->setParent(this);
    itemView->hide();
    if (m_model) {
        itemView->setModel(m_model);
        itemView->setRootIndex(m_root);
    }
}

/*!
    \since 6.0

    Makes \a edit the line edit of the combo box, which becomes editable, and takes ownership of it. The line edit
    that it had is deleted.
*/
void QComboBox::setLineEdit(QLineEdit *edit) {
    if (!edit || edit == m_edit)
        return;
    adoptLineEdit(edit);
}

/*!
    \since 6.0

    Returns the completer of the line edit, or \c nullptr if the combo box is not editable or has none.
*/
QCompleter *QComboBox::completer() const { return m_edit ? m_edit->completer() : nullptr; }

/*!
    \since 6.0

    Makes \a c complete what the user types in the line edit. Nothing happens if the combo box is not editable.
*/
void QComboBox::setCompleter(QCompleter *c) {
    if (m_edit)
        m_edit->setCompleter(c);
}

/*!
    \since 6.0

    Returns the validator of the line edit, or \c nullptr.
*/
const QValidator *QComboBox::validator() const { return m_edit ? m_edit->validator() : nullptr; }

/*!
    \since 6.0

    Makes \a validator check what the user types in the line edit. Nothing happens if the combo box is not
    editable.
*/
void QComboBox::setValidator(const QValidator *validator) {
    if (m_edit)
        m_edit->setValidator(validator);
}

/*!
    \reimp

    Hands \a event to the line edit of an editable combo box; a combo box that is not editable ignores it.
*/
void QComboBox::inputMethodEvent(QInputMethodEvent *event) {
    if (m_edit)
        QCoreApplication::sendEvent(m_edit, event);
    else
        event->ignore();
}

#include "moc_qcombobox.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
