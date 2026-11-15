// MIT License - see LICENSE
#include <QtWidgets/qundoview.h>

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qitemselectionmodel.h>
#include <QtGui/qundogroup.h>
#include <QtGui/qundostack.h>

#include <vector>

/*!
    \class QUndoView
    \brief The QUndoView class displays the contents of a QUndoStack.

    A QUndoView lists the commands of an undo stack, under a first entry for the state before any of them, and
    selects the entry of the current index. Clicking an entry undoes or redoes the commands up to it, so the
    user can go back several steps at once:

    \code
    auto *history = new QUndoView(stack);
    history->setEmptyLabel("Opened");
    history->setWindowTitle("History");
    history->show();
    \endcode

    Given a QUndoGroup, the view shows its active stack and follows it as another becomes active.

    \differences
    The clean icon is a QPixmap, as Nt has no \c QIcon.
    \enddifferences

    \sa QUndoStack, QUndoGroup, QListView
*/

// The model of a QUndoView: row 0 is the state before all commands, and row n the state after command n - 1.
class QUndoModel : public QAbstractListModel {
public:
    explicit QUndoModel(QObject *parent) : QAbstractListModel(parent), m_empty(QStringLiteral("<empty>")) {}
    ~QUndoModel() override { disconnectStack(); }

    QUndoStack *stack() const { return m_stack; }
    void setStack(QUndoStack *stack) {
        if (stack == m_stack)
            return;
        beginResetModel();
        disconnectStack();
        m_stack = stack;
        if (stack) {
            const auto changed = [this] { refresh(); };
            m_connections = {
                connect(stack, &QUndoStack::indexChanged, this, [changed](int) { changed(); }),
                connect(stack, &QUndoStack::cleanChanged, this, [changed](bool) { changed(); }),
                connect(stack, &QUndoStack::undoTextChanged, this, [changed](const QString &) { changed(); }),
                connect(stack, &QObject::destroyed, this, [this] { setStack(nullptr); }),
            };
        }
        endResetModel();
        emit selectionWanted();
    }
    QString emptyLabel() const { return m_empty; }
    void setEmptyLabel(const QString &label) {
        m_empty = label;
        if (m_stack)
            emit dataChanged(index(0), index(0));
    }
    QPixmap cleanIcon() const { return m_cleanIcon; }
    void setCleanIcon(const QPixmap &icon) {
        m_cleanIcon = icon;
        if (m_stack && m_stack->cleanIndex() >= 0)
            emit dataChanged(index(m_stack->cleanIndex()), index(m_stack->cleanIndex()));
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        return parent.isValid() || !m_stack ? 0 : m_stack->count() + 1;
    }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!m_stack || !index.isValid() || index.row() > m_stack->count())
            return QVariant();
        if (role == Qt::DisplayRole)
            return index.row() == 0 ? m_empty : m_stack->text(index.row() - 1);
        if (role == Qt::DecorationRole && index.row() == m_stack->cleanIndex() && !m_cleanIcon.isNull())
            return QVariant::fromValue(m_cleanIcon);
        return QVariant();
    }

    // Emitted when the selection should be brought in line with the index of the stack.
    void selectionWanted() { emitSignal(&QUndoModel::selectionWanted); }

private:
    void refresh() {
        beginResetModel();
        endResetModel();
        emit selectionWanted();
    }
    void disconnectStack() {
        for (const QMetaObject::Connection &c : m_connections)
            QObject::disconnect(c);
        m_connections.clear();
    }

    QPointer<QUndoStack> m_stack;
    std::vector<QMetaObject::Connection> m_connections;
    QString m_empty;
    QPixmap m_cleanIcon;
};

/*!
    Constructs an undo view with the given \a parent. It shows nothing until setStack() or setGroup().
*/
QUndoView::QUndoView(QWidget *parent) : QListView(parent), m_model(new QUndoModel(this)) {
    setModel(m_model);
    setSelectionMode(SingleSelection);
    connect(m_model, &QUndoModel::selectionWanted, this, [this] {
        QUndoStack *s = m_model->stack();
        if (!s || !selectionModel())
            return;
        const QModelIndex row = m_model->index(s->index());
        selectionModel()->setCurrentIndex(row, QItemSelectionModel::ClearAndSelect);
    });
    connect(selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current, const QModelIndex &) {
                QUndoStack *s = m_model->stack();
                if (s && current.isValid() && current.row() != s->index())
                    s->setIndex(current.row());
            });
}

/*!
    Constructs an undo view with the given \a parent that shows \a stack.
*/
QUndoView::QUndoView(QUndoStack *stack, QWidget *parent) : QUndoView(parent) { setStack(stack); }

/*!
    Constructs an undo view with the given \a parent that shows the active stack of \a group.
*/
QUndoView::QUndoView(QUndoGroup *group, QWidget *parent) : QUndoView(parent) { setGroup(group); }

/*!
    Destroys the view. The stack and the group are left alone.
*/
QUndoView::~QUndoView() { QObject::disconnect(m_groupConnection); }

/*!
    Returns the stack that the view shows, or \c nullptr.

    \sa setStack()
*/
QUndoStack *QUndoView::stack() const { return m_model->stack(); }

/*!
    Returns the group whose active stack the view shows, or \c nullptr.

    \sa setGroup()
*/
QUndoGroup *QUndoView::group() const { return m_group; }

/*!
    Sets the text of the first entry, the state before all commands, to \a label. The default is
    \c{"<empty>"}.

    \sa emptyLabel()
*/
void QUndoView::setEmptyLabel(const QString &label) { m_model->setEmptyLabel(label); }

/*!
    Returns the text of the first entry.

    \sa setEmptyLabel()
*/
QString QUndoView::emptyLabel() const { return m_model->emptyLabel(); }

/*!
    Shows \a icon next to the entry of the clean index, where the document was saved. A null pixmap, the default,
    shows nothing there.

    \sa cleanIcon(), QUndoStack::setClean()
*/
void QUndoView::setCleanIcon(const QPixmap &icon) { m_model->setCleanIcon(icon); }

/*!
    Returns the icon of the clean entry.

    \sa setCleanIcon()
*/
QPixmap QUndoView::cleanIcon() const { return m_model->cleanIcon(); }

/*!
    Shows \a stack, or nothing if \a stack is \c nullptr. The view no longer follows a group.

    \sa stack(), setGroup()
*/
void QUndoView::setStack(QUndoStack *stack) {
    if (m_group && (!stack || m_group->activeStack() != stack)) {
        QObject::disconnect(m_groupConnection);
        m_group = nullptr;
    }
    m_model->setStack(stack);
}

/*!
    Shows the active stack of \a group, and follows the group as another stack becomes active. A \c nullptr
    shows nothing.

    \sa group(), setStack()
*/
void QUndoView::setGroup(QUndoGroup *group) {
    QObject::disconnect(m_groupConnection);
    m_group = group;
    if (group) {
        m_groupConnection = connect(group, &QUndoGroup::activeStackChanged, this,
                                    [this](QUndoStack *active) { m_model->setStack(active); });
    }
    m_model->setStack(group ? group->activeStack() : nullptr);
}

#include "moc_qundoview.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
