// MIT License - see LICENSE
#include <QtWidgets/qformlayout.h>

#include "qlayoutcalc_p.h"

#include <QtCore/qdebug.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qwidget.h>

#include <algorithm>

using QtPrivate::calculate;
using QtPrivate::Cell;

namespace {

QSize hintOf(const QLayoutItem *item) { return item && !item->isEmpty() ? item->sizeHint() : QSize(0, 0); }
QSize minimumOf(const QLayoutItem *item) { return item && !item->isEmpty() ? item->minimumSize() : QSize(0, 0); }

bool absent(const QLayoutItem *item) { return !item || item->isEmpty(); }

// How far a label may grow in a row that is taller than it: a label that is next to a large field keeps close to the
// top of the row, as in Qt, where it gets about 1.76 times its height at most.
int labelHeightLimit(const QLayoutItem *label, int hintHeight) {
    return label->widget() ? std::max(hintHeight, hintHeight * 30 / 17 - 1) : QLAYOUTSIZE_MAX;
}

} // namespace

/*!
    \class QFormLayout
    \brief The QFormLayout class lays out a form of labels and fields.

    A form has rows, and a row has a label and the field that it names: a line edit, a spin box, a combo box or
    any other widget or layout. The labels are in a column on the left, and the fields in a column to the right of
    them, so the fields line up. A row with only one widget, such as a button or a group box, takes the width of
    the form.

    \code
    QFormLayout *form = new QFormLayout(window);
    form->addRow("Name:", new QLineEdit);
    form->addRow("Age:", new QSpinBox);
    form->addRow(new QLabel("Notes:"), notes);
    form->addRow(okButton);
    \endcode

    In a narrow window a form can put each label above its field: see setRowWrapPolicy(). The fields grow to fill the
    form by default, as in the Fusion style of Qt, and setFieldGrowthPolicy() changes that.

    A form with labels in the left column, fields that fill the right column, a field that is taller than its label,
    and a button on a row of its own.

    \image widgets/qformlayout.png QFormLayout in the light color scheme

    \differences
    A new label cannot be given a buddy. There is no \c takeRow(), no \c removeRow() for a widget or a layout, no \c setSpacing()
    that is different from QLayout::setSpacing(), and no height for width (\c hasHeightForWidth()), so a word-wrapped label
    needs a fixed height. The form alignment only places the form when it is smaller than the space it has. The defaults
    are those of the Fusion style: the labels are on the left, and the fields grow.
    \enddifferences

    \sa QGridLayout, QBoxLayout, QLayout
*/

/*!
    \enum QFormLayout::FieldGrowthPolicy

    This enum says how the fields of a form take the room that is left.

    \value FieldsStayAtSizeHint The fields keep their size hint.
    \value ExpandingFieldsGrow The fields whose size policy is expanding grow, and the others keep their size hint.
    \value AllNonFixedFieldsGrow Every field that is not fixed in width grows to fill the column.
*/

/*!
    \enum QFormLayout::RowWrapPolicy

    This enum says when a label is put above its field instead of before it.

    \value DontWrapRows Labels are always before their field.
    \value WrapLongRows A label goes above its field when the form is too narrow for both side by side.
    \value WrapAllRows Labels are always above their field.
*/

/*!
    \enum QFormLayout::ItemRole

    This enum says what an item is in its row.

    \value LabelRole The label of the row.
    \value FieldRole The field of the row.
    \value SpanningRole The only item of a row, which takes the width of the form.
*/

/*!
    Constructs a form layout for \a parent, with no rows.
*/
QFormLayout::QFormLayout(QWidget *parent) : QLayout(parent) {}

/*!
    Destroys the layout. The widgets that are in it stay.
*/
QFormLayout::~QFormLayout() {
    // A layout that is deleted leaves the layout it is in.
    if (auto *parentLayout = dynamic_cast<QLayout *>(parent()))
        parentLayout->removeItem(this);
    for (Row &row : m_rows) {
        if (row.labelOwned)
            delete row.label;
        if (row.fieldOwned)
            delete row.field;
    }
}

QWidget *QFormLayout::newLabel(const QString &text) {
    auto *label = new QLabel(text);
    addChildWidget(label);
    return label;
}

int QFormLayout::insertEmptyRow(int row) {
    row = std::clamp(row, 0, rowCount());
    m_rows.insert(m_rows.begin() + row, Row());
    return row;
}

// Puts `item` in the slot of the role; the slot must be free.
void QFormLayout::setItem(int row, ItemRole role, QLayoutItem *item, bool owned) {
    Row &r = m_rows[static_cast<std::size_t>(row)];
    if (role == LabelRole) {
        r.label = item;
        r.labelOwned = owned;
    } else {
        r.field = item;
        r.fieldOwned = owned;
        r.spanning = role == SpanningRole;
    }
    invalidate();
}

/*!
    Adds a row with \a label before \a field. The form takes over both widgets.

    \sa insertRow()
*/
void QFormLayout::addRow(QWidget *label, QWidget *field) { insertRow(rowCount(), label, field); }

/*!
    \overload

    Adds a row with \a label before the layout \a field.
*/
void QFormLayout::addRow(QWidget *label, QLayout *field) { insertRow(rowCount(), label, field); }

/*!
    \overload

    Adds a row with a new label that has \a labelText before \a field.
*/
void QFormLayout::addRow(const QString &labelText, QWidget *field) { insertRow(rowCount(), labelText, field); }

/*!
    \overload

    Adds a row with a new label that has \a labelText before the layout \a field.
*/
void QFormLayout::addRow(const QString &labelText, QLayout *field) { insertRow(rowCount(), labelText, field); }

/*!
    \overload

    Adds a row with only \a widget, as wide as the form.
*/
void QFormLayout::addRow(QWidget *widget) { insertRow(rowCount(), widget); }

/*!
    \overload

    Adds a row with only \a layout, as wide as the form.
*/
void QFormLayout::addRow(QLayout *layout) { insertRow(rowCount(), layout); }

/*!
    Inserts a row at \a row, with \a label before \a field; rows from \a row on move down. \a row is brought into the
    range 0 to rowCount().

    \sa addRow(), removeRow()
*/
void QFormLayout::insertRow(int row, QWidget *label, QWidget *field) {
    row = insertEmptyRow(row);
    setWidget(row, LabelRole, label);
    setWidget(row, FieldRole, field);
}

/*!
    \overload

    Inserts a row at \a row, with \a label before the layout \a field.
*/
void QFormLayout::insertRow(int row, QWidget *label, QLayout *field) {
    row = insertEmptyRow(row);
    setWidget(row, LabelRole, label);
    setLayout(row, FieldRole, field);
}

/*!
    \overload

    Inserts a row at \a row, with a new label that has \a labelText before \a field.
*/
void QFormLayout::insertRow(int row, const QString &labelText, QWidget *field) {
    insertRow(row, labelText.isEmpty() ? nullptr : newLabel(labelText), field);
}

/*!
    \overload

    Inserts a row at \a row, with a new label that has \a labelText before the layout \a field.
*/
void QFormLayout::insertRow(int row, const QString &labelText, QLayout *field) {
    insertRow(row, labelText.isEmpty() ? nullptr : newLabel(labelText), field);
}

/*!
    \overload

    Inserts a row at \a row with only \a widget, as wide as the form.
*/
void QFormLayout::insertRow(int row, QWidget *widget) {
    row = insertEmptyRow(row);
    setWidget(row, SpanningRole, widget);
}

/*!
    \overload

    Inserts a row at \a row with only \a layout, as wide as the form.
*/
void QFormLayout::insertRow(int row, QLayout *layout) {
    row = insertEmptyRow(row);
    setLayout(row, SpanningRole, layout);
}

/*!
    Removes the row \a row, and deletes the widgets and layouts in it. Does nothing if there is no such row.

    \sa insertRow()
*/
void QFormLayout::removeRow(int row) {
    if (row < 0 || row >= rowCount())
        return;
    const Row r = m_rows[static_cast<std::size_t>(row)];
    m_rows.erase(m_rows.begin() + row);
    invalidate();
    for (QLayoutItem *item : {r.label, r.field}) {
        if (!item)
            continue;
        QWidget *widget = item->widget();
        QLayout *inner = item->layout();
        if (item == r.label ? r.labelOwned : r.fieldOwned)
            delete item;
        delete widget;
        delete inner;
    }
}

/*!
    Puts \a widget in the row \a row with the given \a role. The row may be rowCount(), which adds a row. Nothing
    happens if the row is out of range or the role is taken in it.

    \sa setLayout(), itemAt()
*/
void QFormLayout::setWidget(int row, ItemRole role, QWidget *widget) {
    if (!widget)
        return;
    if (row < 0 || row > rowCount()) {
        qWarning("QFormLayout::setWidget: row %d is out of range", row);
        return;
    }
    if (row == rowCount())
        insertEmptyRow(row);
    const Row &r = m_rows[static_cast<std::size_t>(row)];
    if ((role == LabelRole && r.label) || (role != LabelRole && r.field) || (role == SpanningRole && r.label)) {
        qWarning("QFormLayout::setWidget: the row already has an item for that role");
        return;
    }
    addChildWidget(widget);
    setItem(row, role, new QWidgetItem(widget), true);
}

/*!
    Puts \a layout in the row \a row with the given \a role; see setWidget(). The layout becomes a child of this one.

    \sa setWidget()
*/
void QFormLayout::setLayout(int row, ItemRole role, QLayout *layout) {
    if (!layout)
        return;
    if (row < 0 || row > rowCount()) {
        qWarning("QFormLayout::setLayout: row %d is out of range", row);
        return;
    }
    if (layout->parent() && layout->parent() != this) {
        qWarning("QFormLayout::setLayout: the layout already has a parent");
        return;
    }
    if (row == rowCount())
        insertEmptyRow(row);
    const Row &r = m_rows[static_cast<std::size_t>(row)];
    if ((role == LabelRole && r.label) || (role != LabelRole && r.field) || (role == SpanningRole && r.label)) {
        qWarning("QFormLayout::setLayout: the row already has an item for that role");
        return;
    }
    layout->QObject::setParent(this);
    layout->reparentChildWidgets();
    setItem(row, role, layout, false);
}

/*!
    \fn int QFormLayout::rowCount() const

    Returns the number of rows.
*/

/*!
    Returns the item in the row \a row with the given \a role, or \c nullptr if there is none.
*/
QLayoutItem *QFormLayout::itemAt(int row, ItemRole role) const {
    if (row < 0 || row >= rowCount())
        return nullptr;
    const Row &r = m_rows[static_cast<std::size_t>(row)];
    if (role == LabelRole)
        return r.label;
    if (role == FieldRole)
        return r.spanning ? nullptr : r.field;
    return r.spanning ? r.field : nullptr;
}

/*!
    Finds the row and the role of \a widget and stores them in \a row and \a role. They are -1 if the widget is not
    in the form.

    \sa getLayoutPosition()
*/
void QFormLayout::getWidgetPosition(QWidget *widget, int *row, ItemRole *role) const {
    for (int i = 0; i < rowCount(); ++i)
        for (ItemRole r : {LabelRole, FieldRole, SpanningRole}) {
            const QLayoutItem *item = itemAt(i, r);
            if (item && item->widget() == widget) {
                if (row)
                    *row = i;
                if (role)
                    *role = r;
                return;
            }
        }
    if (row)
        *row = -1;
    if (role)
        *role = LabelRole;
}

/*!
    Finds the row and the role of \a layout and stores them in \a row and \a role. They are -1 if the layout is not
    in the form.

    \sa getWidgetPosition()
*/
void QFormLayout::getLayoutPosition(QLayout *layout, int *row, ItemRole *role) const {
    for (int i = 0; i < rowCount(); ++i)
        for (ItemRole r : {LabelRole, FieldRole, SpanningRole}) {
            QLayoutItem *item = itemAt(i, r);
            if (item && item->layout() == layout) {
                if (row)
                    *row = i;
                if (role)
                    *role = r;
                return;
            }
        }
    if (row)
        *row = -1;
    if (role)
        *role = LabelRole;
}

/*!
    Returns the label of the row that has \a field, or \c nullptr if there is none.
*/
QWidget *QFormLayout::labelForField(QWidget *field) const {
    for (const Row &r : m_rows)
        if (!r.spanning && r.field && r.field->widget() == field && r.label)
            return r.label->widget();
    return nullptr;
}

/*!
    \overload

    Returns the label of the row that has the layout \a field, or \c nullptr if there is none.
*/
QWidget *QFormLayout::labelForField(QLayout *field) const {
    for (const Row &r : m_rows)
        if (!r.spanning && r.field && r.field->layout() == field && r.label)
            return r.label->widget();
    return nullptr;
}

/*!
    \fn QFormLayout::FieldGrowthPolicy QFormLayout::fieldGrowthPolicy() const

    Returns how the fields take the room that is left. The default is \c AllNonFixedFieldsGrow, as in the Fusion style.

    \sa setFieldGrowthPolicy()
*/

/*!
    Sets how the fields take the room that is left to \a policy.

    \sa fieldGrowthPolicy()
*/
void QFormLayout::setFieldGrowthPolicy(FieldGrowthPolicy policy) {
    m_growth = policy;
    invalidate();
}

/*!
    \fn QFormLayout::RowWrapPolicy QFormLayout::rowWrapPolicy() const

    Returns when a label is put above its field. The default is \c DontWrapRows.

    \sa setRowWrapPolicy()
*/

/*!
    Sets when a label is put above its field to \a policy.

    \sa rowWrapPolicy()
*/
void QFormLayout::setRowWrapPolicy(RowWrapPolicy policy) {
    m_wrap = policy;
    invalidate();
}

/*!
    \fn Qt::Alignment QFormLayout::labelAlignment() const

    Returns how the labels are placed in their column. The default is \c Qt::AlignLeft.

    \sa setLabelAlignment()
*/

/*!
    Sets how the labels are placed in their column to \a alignment, one of \c Qt::AlignLeft, \c Qt::AlignRight and
    \c Qt::AlignHCenter. A label that is above its field is always at the left.

    \sa labelAlignment()
*/
void QFormLayout::setLabelAlignment(Qt::Alignment alignment) {
    m_labelAlignment = alignment;
    invalidate();
}

/*!
    \fn Qt::Alignment QFormLayout::formAlignment() const

    Returns where the form is put when it is smaller than the space it has. The default is
    \c{Qt::AlignLeft | Qt::AlignTop}.

    \sa setFormAlignment()
*/

/*!
    Sets where the form is put when it does not fill the space it has to \a alignment.

    \sa formAlignment()
*/
void QFormLayout::setFormAlignment(Qt::Alignment alignment) {
    m_formAlignment = alignment;
    invalidate();
}

/*!
    Returns the space between the label column and the field column: the one that setHorizontalSpacing() set, or
    else spacing().

    \sa setHorizontalSpacing()
*/
int QFormLayout::horizontalSpacing() const { return m_horizontalSpacing >= 0 ? m_horizontalSpacing : spacing(); }

/*!
    Sets the space between the label column and the field column to \a spacing.

    \sa horizontalSpacing()
*/
void QFormLayout::setHorizontalSpacing(int spacing) {
    m_horizontalSpacing = spacing;
    invalidate();
}

/*!
    Returns the space between the rows, and between a label and its field above it: the one that
    setVerticalSpacing() set, or else spacing().

    \sa setVerticalSpacing()
*/
int QFormLayout::verticalSpacing() const { return m_verticalSpacing >= 0 ? m_verticalSpacing : spacing(); }

/*!
    Sets the space between the rows to \a spacing.

    \sa verticalSpacing()
*/
void QFormLayout::setVerticalSpacing(int spacing) {
    m_verticalSpacing = spacing;
    invalidate();
}

/*!
    \reimp

    Adds \a item as a row of its own, as wide as the form.
*/
void QFormLayout::addItem(QLayoutItem *item) {
    if (!item)
        return;
    const int row = insertEmptyRow(rowCount());
    setItem(row, SpanningRole, item, item->layout() == nullptr);
}

/*!
    \reimp
*/
int QFormLayout::count() const {
    int n = 0;
    for (const Row &r : m_rows)
        n += (r.label ? 1 : 0) + (r.field ? 1 : 0);
    return n;
}

/*!
    \reimp
*/
QLayoutItem *QFormLayout::itemAt(int index) const {
    if (index < 0)
        return nullptr;
    for (const Row &r : m_rows)
        for (QLayoutItem *item : {r.label, r.field})
            if (item && index-- == 0)
                return item;
    return nullptr;
}

/*!
    \reimp
*/
QLayoutItem *QFormLayout::takeAt(int index) {
    if (index < 0)
        return nullptr;
    for (std::size_t i = 0; i < m_rows.size(); ++i) {
        Row &r = m_rows[i];
        if (r.label && index-- == 0) {
            QLayoutItem *item = r.label;
            r.label = nullptr;
            if (!r.field)
                m_rows.erase(m_rows.begin() + static_cast<std::ptrdiff_t>(i));
            return item;
        }
        if (r.field && index-- == 0) {
            QLayoutItem *item = r.field;
            r.field = nullptr;
            r.spanning = false;
            if (!r.label)
                m_rows.erase(m_rows.begin() + static_cast<std::ptrdiff_t>(i));
            return item;
        }
    }
    return nullptr;
}

// What the form needs of a row.
struct QFormLayout::Metrics {
    struct RowSizes {
        const Row *row;
        bool wrapped;
        QSize labelHint, labelMin, fieldHint, fieldMin;
        int hintHeight, minHeight;
        bool expands;
    };
    std::vector<RowSizes> rows;
    int labelHint = 0, labelMin = 0;     // the label column, over all the labels
    int fieldHint = 0, fieldMin = 0;     // the field column, likewise
    int wrappedHint = 0, wrappedMin = 0; // the widest of the wrapped and the spanning rows
};

// Whether the row has its label above its field in a form of the given width, whose label column is labelWidth wide.
bool QFormLayout::wraps(const Row &row, int width, int labelWidth) const {
    if (row.spanning || !row.label || absent(row.label) || absent(row.field))
        return false;
    if (m_wrap == WrapAllRows)
        return true;
    return m_wrap == WrapLongRows && labelWidth + horizontalSpacing() + minimumOf(row.field).width() > width;
}

QFormLayout::Metrics QFormLayout::measure(int width) const {
    Metrics m;
    int widest = 0;
    for (const Row &r : m_rows)
        if (!r.spanning)
            widest = std::max(widest, hintOf(r.label).width());
    const int vs = verticalSpacing();
    for (const Row &r : m_rows) {
        if (absent(r.label) && absent(r.field))
            continue;
        Metrics::RowSizes s{};
        s.row = &r;
        s.wrapped = wraps(r, width, widest);
        s.labelHint = hintOf(r.label);
        s.labelMin = minimumOf(r.label);
        s.fieldHint = hintOf(r.field);
        s.fieldMin = minimumOf(r.field);
        s.expands = r.field && !r.field->isEmpty() && (r.field->expandingDirections() & Qt::Vertical);
        if (s.wrapped) {
            s.hintHeight = s.labelHint.height() + vs + s.fieldHint.height();
            s.minHeight = s.labelMin.height() + vs + s.fieldMin.height();
            m.wrappedHint = std::max({m.wrappedHint, s.labelHint.width(), s.fieldHint.width()});
            m.wrappedMin = std::max({m.wrappedMin, s.labelMin.width(), s.fieldMin.width()});
        } else if (r.spanning) {
            s.hintHeight = s.fieldHint.height();
            s.minHeight = s.fieldMin.height();
            m.wrappedHint = std::max(m.wrappedHint, s.fieldHint.width());
            m.wrappedMin = std::max(m.wrappedMin, s.fieldMin.width());
        } else {
            s.hintHeight = std::max(s.labelHint.height(), s.fieldHint.height());
            s.minHeight = std::max(s.labelMin.height(), s.fieldMin.height());
            m.fieldHint = std::max(m.fieldHint, s.fieldHint.width());
            m.fieldMin = std::max(m.fieldMin, s.fieldMin.width());
        }
        // The label column is as wide as the widest label, also when its row is wrapped.
        if (!r.spanning) {
            m.labelHint = std::max(m.labelHint, s.labelHint.width());
            m.labelMin = std::max(m.labelMin, s.labelMin.width());
        }
        m.rows.push_back(s);
    }
    return m;
}

// The size the form needs: for the hint, as the rows are laid out when there is room; for the minimum, as they are
// when there is none, which wraps the rows that the policy lets wrap.
QSize QFormLayout::total(bool minimum) const {
    const int hs = horizontalSpacing(), vs = verticalSpacing();
    // A width of 0 is too narrow for any row, so it makes WrapLongRows wrap them all.
    const Metrics m = measure(minimum ? 0 : QLAYOUTSIZE_MAX);
    int width = m.wrappedHint, height = 0;
    if (minimum)
        width = m.wrappedMin;
    bool twoColumns = false;
    for (const auto &s : m.rows)
        twoColumns = twoColumns || (!s.wrapped && !s.row->spanning);
    if (twoColumns) {
        const int columns = minimum ? m.labelMin + hs + m.fieldMin : m.labelHint + hs + m.fieldHint;
        width = std::max(width, columns);
    }
    for (const auto &s : m.rows)
        height += minimum ? s.minHeight : s.hintHeight;
    height += std::max(0, static_cast<int>(m.rows.size()) - 1) * vs;
    const QMargins margins = contentsMargins();
    return QSize(width + margins.left() + margins.right(), height + margins.top() + margins.bottom());
}

/*!
    \reimp
*/
QSize QFormLayout::sizeHint() const { return total(false); }

/*!
    \reimp
*/
QSize QFormLayout::minimumSize() const { return total(true); }

/*!
    \reimp
*/
Qt::Orientations QFormLayout::expandingDirections() const {
    Qt::Orientations directions;
    for (const Row &r : m_rows)
        for (const QLayoutItem *item : {r.label, r.field})
            if (item && !item->isEmpty())
                directions |= item->expandingDirections();
    return directions;
}

/*!
    \reimp
*/
void QFormLayout::setGeometry(const QRect &rect) {
    QLayout::setGeometry(rect);
    const QRect r = rect.marginsRemoved(contentsMargins());
    const int hs = horizontalSpacing(), vs = verticalSpacing();
    const Metrics m = measure(r.width());
    if (m.rows.empty())
        return;

    // The columns: the labels get what they ask for as long as the fields keep their minimum.
    int labelWidth = m.labelHint;
    if (r.width() < labelWidth + hs + m.fieldMin)
        labelWidth = std::max(m.labelMin, r.width() - hs - m.fieldMin);
    const int fieldAvailable = std::max(0, r.width() - labelWidth - hs);

    std::vector<Cell> cells;
    for (const auto &s : m.rows) {
        Cell c{};
        c.item = nullptr;
        c.stretch = 0;
        c.min = s.minHeight;
        c.hint = std::max(s.hintHeight, s.minHeight);
        c.max = s.expands ? QLAYOUTSIZE_MAX : c.hint;
        c.expansive = s.expands;
        c.empty = false;
        cells.push_back(c);
    }
    calculate(cells, r.height(), vs);

    // The widest a field goes, to place the form when it does not fill the space.
    int usedWidth = 0;
    int usedHeight = std::max(0, static_cast<int>(cells.size()) - 1) * vs;
    for (const Cell &c : cells)
        usedHeight += c.size;

    struct Placed {
        QLayoutItem *item;
        QRect rect;
    };
    std::vector<Placed> placed;
    auto fieldWidth = [this](const QLayoutItem *item, bool spanning, int available) {
        const QSize hint = item->sizeHint(), mn = item->minimumSize(), mx = item->maximumSize();
        const bool grows = spanning || m_growth == AllNonFixedFieldsGrow ||
                           (m_growth == ExpandingFieldsGrow && (item->expandingDirections() & Qt::Horizontal));
        const int wanted = grows ? available : std::min(hint.width(), available);
        return std::clamp(wanted, mn.width(), std::max(mn.width(), mx.width()));
    };
    int y = 0;
    for (std::size_t i = 0; i < m.rows.size(); ++i) {
        const auto &s = m.rows[i];
        const Row &row = *s.row;
        const int height = cells[i].size;
        if (s.wrapped) {
            const int labelHeight = s.labelHint.height();
            const int labelW = std::min(r.width(), s.labelHint.width());
            placed.push_back({row.label, QRect(0, y, labelW, labelHeight)});
            const int fieldHeight = std::max(0, height - labelHeight - vs);
            const int w = fieldWidth(row.field, false, r.width());
            const int h = std::min(fieldHeight, std::max(s.fieldMin.height(), row.field->maximumSize().height()));
            placed.push_back({row.field, QRect(0, y + labelHeight + vs, w, h)});
            usedWidth = std::max({usedWidth, labelW, w});
        } else if (row.spanning) {
            const int w = fieldWidth(row.field, true, r.width());
            const int h = std::min(height, std::max(s.fieldMin.height(), row.field->maximumSize().height()));
            placed.push_back({row.field, QRect(0, y, w, h)});
            usedWidth = std::max(usedWidth, w);
        } else {
            if (!absent(row.label)) {
                const QSize mx = row.label->maximumSize();
                const int w = std::min(labelWidth, std::max(s.labelMin.width(), s.labelHint.width()));
                int x = 0;
                if (m_labelAlignment & Qt::AlignRight)
                    x = labelWidth - w;
                else if (m_labelAlignment & Qt::AlignHCenter)
                    x = (labelWidth - w) / 2;
                const int h = std::min({height, mx.height(), labelHeightLimit(row.label, s.labelHint.height())});
                placed.push_back({row.label, QRect(x, y, w, std::max(h, std::min(height, s.labelMin.height())))});
            }
            if (!absent(row.field)) {
                const int w = fieldWidth(row.field, false, fieldAvailable);
                const int h = std::min(height, std::max(s.fieldMin.height(), row.field->maximumSize().height()));
                placed.push_back({row.field, QRect(labelWidth + hs, y, w, h)});
                usedWidth = std::max(usedWidth, labelWidth + hs + w);
            } else {
                usedWidth = std::max(usedWidth, labelWidth);
            }
        }
        y += height + vs;
    }

    int offsetX = 0, offsetY = 0;
    if (m_formAlignment & Qt::AlignRight)
        offsetX = std::max(0, r.width() - usedWidth);
    else if (m_formAlignment & Qt::AlignHCenter)
        offsetX = std::max(0, (r.width() - usedWidth) / 2);
    if (m_formAlignment & Qt::AlignBottom)
        offsetY = std::max(0, r.height() - usedHeight);
    else if (m_formAlignment & Qt::AlignVCenter)
        offsetY = std::max(0, (r.height() - usedHeight) / 2);
    for (const Placed &p : placed)
        p.item->setGeometry(p.rect.translated(r.left() + offsetX, r.top() + offsetY));
}

#include "moc_qformlayout.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
