// MIT License - see LICENSE
#include <QtWidgets/qdatawidgetmapper.h>

#include <QtCore/qvariant.h>
#include <QtWidgets/qabstractitemdelegate.h>
#include <QtWidgets/qstyleditemdelegate.h>
#include <QtWidgets/qwidget.h>

#include <algorithm>

/*!
    \class QDataWidgetMapper
    \brief The QDataWidgetMapper class provides mapping between a section of a data model to widgets.

    A QDataWidgetMapper shows one row of a model in a form of widgets, one widget for each column, and writes
    what the user changes back to the model. It is the way to edit a record of a table in a dialog:

    \code
    auto *mapper = new QDataWidgetMapper(this);
    mapper->setModel(model);
    mapper->addMapping(nameEdit, 0);
    mapper->addMapping(ageSpinBox, 1);
    mapper->addMapping(notesEdit, 2, "plainText");
    mapper->toFirst();
    connect(nextButton, &QPushButton::clicked, mapper, &QDataWidgetMapper::toNext);
    \endcode

    A widget shows its section through the delegate, itemDelegate(), which fills it with QAbstractItemDelegate::setEditorData()
    and reads it back with QAbstractItemDelegate::setModelData(); QStyledItemDelegate, the default, uses the user property of the widget,
    such as QLineEdit::text or QSpinBox::value. A mapping with a property name uses that property instead.

    With the \c AutoSubmit policy, the default, a widget writes its value to the model when it loses the focus
    or Return is pressed in it, and Escape brings the value of the model back. With \c ManualSubmit nothing is
    written until submit() is called, and revert() brings the values of the model back.

    With the vertical orientation the mapper goes through the columns of the model instead, and the sections
    are rows.

    \differences
    None.
    \enddifferences

    \sa QStyledItemDelegate, QAbstractItemModel
*/

/*!
    \enum QDataWidgetMapper::SubmitPolicy

    This enum says when the values of the widgets are written to the model.

    \value AutoSubmit When a widget loses the focus, or the user presses Return in it.
    \value ManualSubmit Only when submit() is called.
*/

/*!
    Constructs a mapper with the given \a parent. It has no model and a QStyledItemDelegate of its own.
*/
QDataWidgetMapper::QDataWidgetMapper(QObject *parent) : QObject(parent), m_delegate(new QStyledItemDelegate(this)) {
    connectDelegate();
}

/*!
    Destroys the mapper. The widgets and the model are left alone.
*/
QDataWidgetMapper::~QDataWidgetMapper() {
    for (const QMetaObject::Connection &c : m_modelConnections)
        QObject::disconnect(c);
    for (const QMetaObject::Connection &c : m_delegateConnections)
        QObject::disconnect(c);
    for (const Mapping &m : m_mappings)
        watch(m.widget, false);
}

/*!
    Makes \a model the model whose data the widgets show. The current index is -1 until setCurrentIndex() or
    toFirst() is called. The mapper does not own the model.

    \sa model(), toFirst()
*/
void QDataWidgetMapper::setModel(QAbstractItemModel *model) {
    if (model == m_model)
        return;
    for (const QMetaObject::Connection &c : m_modelConnections)
        QObject::disconnect(c);
    m_modelConnections.clear();
    m_model = model;
    m_root = QPersistentModelIndex();
    m_current = QPersistentModelIndex();
    if (!model)
        return;
    m_modelConnections = {
        connect(model, &QAbstractItemModel::dataChanged, this,
                [this](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &) {
                    for (const Mapping &m : m_mappings) {
                        const QModelIndex index = indexOf(m);
                        if (index.isValid() && index.parent() == topLeft.parent() && index.row() >= topLeft.row() &&
                            index.row() <= bottomRight.row() && index.column() >= topLeft.column() &&
                            index.column() <= bottomRight.column())
                            populate(m);
                    }
                }),
        connect(model, &QAbstractItemModel::modelReset, this, [this] { m_current = QPersistentModelIndex(); }),
        connect(model, &QAbstractItemModel::layoutChanged, this,
                [this](const QList<QPersistentModelIndex> &, QAbstractItemModel::LayoutChangeHint) { populateAll(); }),
    };
}

/*!
    Returns the model, or \c nullptr.

    \sa setModel()
*/
QAbstractItemModel *QDataWidgetMapper::model() const { return m_model; }

void QDataWidgetMapper::watch(QWidget *widget, bool on) {
    if (!widget || !m_delegate)
        return;
    if (on)
        widget->installEventFilter(m_delegate);
    else
        widget->removeEventFilter(m_delegate);
}

void QDataWidgetMapper::connectDelegate() {
    for (const QMetaObject::Connection &c : m_delegateConnections)
        QObject::disconnect(c);
    m_delegateConnections.clear();
    if (!m_delegate)
        return;
    m_delegateConnections = {
        connect(m_delegate.data(), &QAbstractItemDelegate::commitData, this,
                [this](QWidget *editor) {
                    if (m_policy == ManualSubmit)
                        return;
                    if (const Mapping *m = find(editor))
                        commit(*m);
                }),
        connect(m_delegate.data(), &QAbstractItemDelegate::closeEditor, this,
                [this](QWidget *editor, QAbstractItemDelegate::EndEditHint hint) {
                    const Mapping *m = find(editor);
                    if (!m || !m->widget)
                        return;
                    switch (hint) {
                    case QAbstractItemDelegate::RevertModelCache:
                        populate(*m);
                        break;
                    case QAbstractItemDelegate::EditNextItem:
                        m->widget->focusNextChild();
                        break;
                    case QAbstractItemDelegate::EditPreviousItem:
                        m->widget->focusPreviousChild();
                        break;
                    case QAbstractItemDelegate::NoHint:
                    case QAbstractItemDelegate::SubmitModelCache:
                        break;
                    }
                }),
    };
}

/*!
    Makes \a delegate the delegate that fills the widgets and reads them back. The mapper does not own it.

    \sa itemDelegate()
*/
void QDataWidgetMapper::setItemDelegate(QAbstractItemDelegate *delegate) {
    if (!delegate || delegate == m_delegate)
        return;
    for (const Mapping &m : m_mappings)
        watch(m.widget, false);
    if (m_delegate && m_delegate->parent() == this)
        m_delegate->deleteLater();
    m_delegate = delegate;
    connectDelegate();
    for (const Mapping &m : m_mappings)
        watch(m.widget, true);
}

/*!
    Returns the delegate that fills the widgets and reads them back: a QStyledItemDelegate unless
    setItemDelegate() set another one.

    \sa setItemDelegate()
*/
QAbstractItemDelegate *QDataWidgetMapper::itemDelegate() const { return m_delegate; }

/*!
    Makes the children of \a index the rows and columns that the mapper goes through. The current index is -1
    again.

    \sa rootIndex()
*/
void QDataWidgetMapper::setRootIndex(const QModelIndex &index) {
    if (index.isValid() && index.model() != m_model)
        return;
    m_root = QPersistentModelIndex(index);
    m_current = QPersistentModelIndex();
}

/*!
    Returns the item whose children the mapper goes through: an invalid index, for the top of the model, by
    default.

    \sa setRootIndex()
*/
QModelIndex QDataWidgetMapper::rootIndex() const { return m_root; }

/*!
    Sets whether the mapper goes through the rows of the model, with Qt::Horizontal, the default, or through its
    columns, with Qt::Vertical, to \a orientation. The mappings stay, and the current index is -1 again.

    \sa orientation()
*/
void QDataWidgetMapper::setOrientation(Qt::Orientation orientation) {
    if (orientation == m_orientation)
        return;
    m_orientation = orientation;
    m_current = QPersistentModelIndex();
}

/*!
    \fn Qt::Orientation QDataWidgetMapper::orientation() const

    Returns whether the mapper goes through the rows of the model (Qt::Horizontal, the default) or its columns
    (Qt::Vertical).

    \sa setOrientation()
*/

/*!
    Sets when the values of the widgets are written to the model to \a policy.

    \sa submitPolicy(), submit()
*/
void QDataWidgetMapper::setSubmitPolicy(SubmitPolicy policy) { m_policy = policy; }

/*!
    \fn QDataWidgetMapper::SubmitPolicy QDataWidgetMapper::submitPolicy() const

    Returns when the values of the widgets are written to the model. The default is \c AutoSubmit.

    \sa setSubmitPolicy()
*/

/*!
    Makes \a widget show the section \a section of the current row (the column \a section), or of the current
    column with the vertical orientation, through the delegate. A widget that had a mapping loses it.

    \sa removeMapping(), mappedSection()
*/
void QDataWidgetMapper::addMapping(QWidget *widget, int section) { addMapping(widget, section, QByteArray()); }

/*!
    \overload

    Makes \a widget show the section \a section through its property \a propertyName, instead of the delegate.
*/
void QDataWidgetMapper::addMapping(QWidget *widget, int section, const QByteArray &propertyName) {
    if (!widget)
        return;
    removeMapping(widget);
    m_mappings.push_back({widget, section, propertyName});
    watch(widget, true);
    populate(m_mappings.back());
}

/*!
    Takes the mapping away from \a widget. The widget keeps what it shows.

    \sa addMapping(), clearMapping()
*/
void QDataWidgetMapper::removeMapping(QWidget *widget) {
    const auto it =
        std::find_if(m_mappings.begin(), m_mappings.end(), [widget](const Mapping &m) { return m.widget == widget; });
    if (it == m_mappings.end())
        return;
    watch(it->widget, false);
    m_mappings.erase(it);
}

/*!
    Returns the section that \a widget shows, or -1 if it has no mapping.

    \sa addMapping()
*/
int QDataWidgetMapper::mappedSection(QWidget *widget) const {
    const Mapping *m = find(widget);
    return m ? m->section : -1;
}

/*!
    Returns the name of the property through which \a widget shows its section: the one given to addMapping(),
    or the user property of the widget.

    \sa addMapping()
*/
QByteArray QDataWidgetMapper::mappedPropertyName(QWidget *widget) const {
    const Mapping *m = find(widget);
    if (!m)
        return QByteArray();
    if (!m->property.isEmpty())
        return m->property;
    const QMetaProperty user = widget->metaObject()->userProperty();
    return user.isValid() ? QByteArray(user.name()) : QByteArray();
}

/*!
    Returns the widget that shows the section \a section, or \c nullptr if none does.

    \sa addMapping()
*/
QWidget *QDataWidgetMapper::mappedWidgetAt(int section) const {
    for (const Mapping &m : m_mappings) {
        if (m.section == section && m.widget)
            return m.widget;
    }
    return nullptr;
}

/*!
    Takes the mappings away from all widgets.

    \sa removeMapping()
*/
void QDataWidgetMapper::clearMapping() {
    for (const Mapping &m : m_mappings)
        watch(m.widget, false);
    m_mappings.clear();
}

/*!
    Returns the row, or with the vertical orientation the column, that the widgets show; -1 if there is none.

    \sa setCurrentIndex()
*/
int QDataWidgetMapper::currentIndex() const {
    if (!m_current.isValid())
        return -1;
    return m_orientation == Qt::Horizontal ? m_current.row() : m_current.column();
}

int QDataWidgetMapper::count() const {
    if (!m_model)
        return 0;
    return m_orientation == Qt::Horizontal ? m_model->rowCount(m_root) : m_model->columnCount(m_root);
}

QModelIndex QDataWidgetMapper::indexOf(const Mapping &mapping) const {
    if (!m_model || !m_current.isValid())
        return QModelIndex();
    return m_orientation == Qt::Horizontal ? m_model->index(m_current.row(), mapping.section, m_root)
                                           : m_model->index(mapping.section, m_current.column(), m_root);
}

const QDataWidgetMapper::Mapping *QDataWidgetMapper::find(const QWidget *widget) const {
    for (const Mapping &m : m_mappings) {
        if (widget && m.widget == widget)
            return &m;
    }
    return nullptr;
}

void QDataWidgetMapper::populate(const Mapping &mapping) {
    const QModelIndex index = indexOf(mapping);
    if (!mapping.widget || !index.isValid())
        return;
    if (!mapping.property.isEmpty())
        mapping.widget->setProperty(mapping.property.constData(), index.data(Qt::EditRole));
    else if (m_delegate)
        m_delegate->setEditorData(mapping.widget, index);
}

void QDataWidgetMapper::populateAll() {
    for (const Mapping &m : m_mappings)
        populate(m);
}

bool QDataWidgetMapper::commit(const Mapping &mapping) {
    const QModelIndex index = indexOf(mapping);
    if (!mapping.widget || !index.isValid() || !m_model)
        return false;
    if (!mapping.property.isEmpty())
        return m_model->setData(index, mapping.widget->property(mapping.property.constData()), Qt::EditRole);
    if (!m_delegate)
        return false;
    m_delegate->setModelData(mapping.widget, m_model, index);
    return true;
}

/*!
    Fills the widgets again with the values of the model, so that what the user changed and was not submitted
    is gone.

    \sa submit()
*/
void QDataWidgetMapper::revert() { populateAll(); }

/*!
    Writes the values of all widgets to the model, then calls QAbstractItemModel::submit(). Returns \c true if
    the model took them all; with the \c ManualSubmit policy this is how they reach it.

    \sa revert(), setSubmitPolicy()
*/
bool QDataWidgetMapper::submit() {
    if (!m_model || !m_current.isValid())
        return false;
    bool ok = true;
    for (const Mapping &m : m_mappings) {
        if (m.widget && !commit(m))
            ok = false;
    }
    return m_model->submit() && ok;
}

/*!
    Shows the first row, or column.

    \sa toLast(), toNext()
*/
void QDataWidgetMapper::toFirst() { setCurrentIndex(0); }

/*!
    Shows the last row, or column.

    \sa toFirst(), toPrevious()
*/
void QDataWidgetMapper::toLast() { setCurrentIndex(count() - 1); }

/*!
    Shows the next row, or column, if there is one.

    \sa toPrevious()
*/
void QDataWidgetMapper::toNext() { setCurrentIndex(currentIndex() + 1); }

/*!
    Shows the row, or column, before the current one, if there is one.

    \sa toNext()
*/
void QDataWidgetMapper::toPrevious() { setCurrentIndex(currentIndex() - 1); }

/*!
    Shows the row \a index, or with the vertical orientation the column, in the widgets, and emits
    currentIndexChanged(). Nothing happens if there is no such row.

    \sa currentIndex(), setCurrentModelIndex()
*/
void QDataWidgetMapper::setCurrentIndex(int index) {
    if (!m_model || index < 0 || index >= count())
        return;
    m_current = QPersistentModelIndex(m_orientation == Qt::Horizontal ? m_model->index(index, 0, m_root)
                                                                      : m_model->index(0, index, m_root));
    populateAll();
    emit currentIndexChanged(index);
}

/*!
    Shows the row of \a index, or its column with the vertical orientation, in the widgets: connect a view's
    QItemSelectionModel::currentRowChanged() here so that the form follows the view. An index that is not a
    child of rootIndex() is ignored.

    \sa setCurrentIndex()
*/
void QDataWidgetMapper::setCurrentModelIndex(const QModelIndex &index) {
    if (!index.isValid() || index.model() != m_model || index.parent() != rootIndex())
        return;
    setCurrentIndex(m_orientation == Qt::Horizontal ? index.row() : index.column());
}

#include "moc_qdatawidgetmapper.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
