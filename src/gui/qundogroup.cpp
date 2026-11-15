// MIT License - see LICENSE
#include <QtGui/qundogroup.h>

#include <QtGui/qaction.h>
#include <QtGui/qkeysequence.h>
#include <QtGui/qundostack.h>

/*!
    \class QUndoGroup
    \brief The QUndoGroup class is a group of QUndoStack objects.

    An application with several documents has an undo stack for each, and one pair of undo and redo actions
    that work on the document in front. A QUndoGroup holds the stacks; one of them is active, and the group's
    actions, signals and functions follow it:

    \code
    auto *group = new QUndoGroup(this);
    editMenu->addAction(group->createUndoAction(this));
    editMenu->addAction(group->createRedoAction(this));
    group->addStack(document->undoStack());
    document->undoStack()->setActive(); // when the document comes to the front
    \endcode

    \differences
    None.
    \enddifferences

    \sa QUndoStack, QUndoView
*/

/*!
    Constructs an empty group with the given \a parent.
*/
QUndoGroup::QUndoGroup(QObject *parent) : QObject(parent) {}

/*!
    Destroys the group. Its stacks are left alone, in no group.
*/
QUndoGroup::~QUndoGroup() {
    for (const QMetaObject::Connection &c : m_activeConnections)
        QObject::disconnect(c);
    for (QUndoStack *stack : m_stacks)
        stack->m_group = nullptr;
}

/*!
    Adds \a stack to the group, taking it out of the group it was in. The group does not own it, and it is not
    active until setActiveStack() or QUndoStack::setActive() makes it so.

    \sa removeStack(), stacks()
*/
void QUndoGroup::addStack(QUndoStack *stack) {
    if (!stack || m_stacks.contains(stack))
        return;
    if (QUndoGroup *old = stack->m_group)
        old->removeStack(stack);
    m_stacks.append(stack);
    stack->m_group = this;
}

/*!
    Takes \a stack out of the group. If it was the active stack, the group has none afterwards.

    \sa addStack()
*/
void QUndoGroup::removeStack(QUndoStack *stack) {
    // A copy, so that the list cannot be thought to change the pointer it is given by reference.
    if (!stack || m_stacks.removeAll(static_cast<QUndoStack *>(stack)) == 0)
        return;
    if (stack == m_active)
        setActiveStack(nullptr);
    stack->m_group = nullptr;
}

/*!
    \fn QList<QUndoStack *> QUndoGroup::stacks() const

    Returns the stacks of the group, in the order they were added.

    \sa addStack()
*/

/*!
    Returns the active stack, or \c nullptr if there is none.

    \sa setActiveStack()
*/
QUndoStack *QUndoGroup::activeStack() const { return m_active; }

/*!
    Makes \a stack, one of the stacks of the group, the active stack, or leaves the group without one if \a stack
    is \c nullptr. The group emits activeStackChanged(), and its other signals for what the new stack says.

    \sa activeStack(), QUndoStack::setActive()
*/
void QUndoGroup::setActiveStack(QUndoStack *stack) {
    if (stack == m_active || (stack && !m_stacks.contains(stack)))
        return;
    for (const QMetaObject::Connection &c : m_activeConnections)
        QObject::disconnect(c);
    m_activeConnections.clear();
    m_active = stack;
    if (stack) {
        m_activeConnections = {
            connect(stack, &QUndoStack::indexChanged, this, &QUndoGroup::indexChanged),
            connect(stack, &QUndoStack::cleanChanged, this, &QUndoGroup::cleanChanged),
            connect(stack, &QUndoStack::canUndoChanged, this, &QUndoGroup::canUndoChanged),
            connect(stack, &QUndoStack::canRedoChanged, this, &QUndoGroup::canRedoChanged),
            connect(stack, &QUndoStack::undoTextChanged, this, &QUndoGroup::undoTextChanged),
            connect(stack, &QUndoStack::redoTextChanged, this, &QUndoGroup::redoTextChanged),
        };
    }
    emit indexChanged(stack ? stack->index() : 0);
    emit cleanChanged(isClean());
    emit canUndoChanged(canUndo());
    emit undoTextChanged(undoText());
    emit canRedoChanged(canRedo());
    emit redoTextChanged(redoText());
    emit activeStackChanged(stack);
}

namespace {
QString actionLabel(const QString &prefix, const QString &word, const QString &text) {
    const QString start = prefix.isEmpty() ? word : prefix;
    return text.isEmpty() ? start : start + QLatin1Char(' ') + text;
}
} // namespace

/*!
    Returns a new action, a child of \a parent, that undoes the last command of the active stack. Its text is
    \a prefix (by default "Undo") followed by undoText().

    \sa createRedoAction(), QUndoStack::createUndoAction()
*/
QAction *QUndoGroup::createUndoAction(QObject *parent, const QString &prefix) const {
    auto *self = const_cast<QUndoGroup *>(this); // NOLINT(cppcoreguidelines-pro-type-const-cast): Qt's signature
    auto *action = new QAction(actionLabel(prefix, QStringLiteral("Undo"), undoText()), parent);
    action->setEnabled(canUndo());
    action->setShortcut(QKeySequence(QKeySequence::Undo));
    connect(self, &QUndoGroup::canUndoChanged, action, &QAction::setEnabled);
    connect(self, &QUndoGroup::undoTextChanged, action, [action, prefix](const QString &text) {
        action->setText(actionLabel(prefix, QStringLiteral("Undo"), text));
    });
    connect(action, &QAction::triggered, self, &QUndoGroup::undo);
    return action;
}

/*!
    Returns a new action, a child of \a parent, that redoes the next command of the active stack. Its text is
    \a prefix (by default "Redo") followed by redoText().

    \sa createUndoAction()
*/
QAction *QUndoGroup::createRedoAction(QObject *parent, const QString &prefix) const {
    auto *self = const_cast<QUndoGroup *>(this); // NOLINT(cppcoreguidelines-pro-type-const-cast): Qt's signature
    auto *action = new QAction(actionLabel(prefix, QStringLiteral("Redo"), redoText()), parent);
    action->setEnabled(canRedo());
    action->setShortcut(QKeySequence(QKeySequence::Redo));
    connect(self, &QUndoGroup::canRedoChanged, action, &QAction::setEnabled);
    connect(self, &QUndoGroup::redoTextChanged, action, [action, prefix](const QString &text) {
        action->setText(actionLabel(prefix, QStringLiteral("Redo"), text));
    });
    connect(action, &QAction::triggered, self, &QUndoGroup::redo);
    return action;
}

/*!
    Returns QUndoStack::canUndo() of the active stack, or \c false without one.
*/
bool QUndoGroup::canUndo() const { return m_active && m_active->canUndo(); }

/*!
    Returns QUndoStack::canRedo() of the active stack, or \c false without one.
*/
bool QUndoGroup::canRedo() const { return m_active && m_active->canRedo(); }

/*!
    Returns QUndoStack::undoText() of the active stack, or an empty string without one.
*/
QString QUndoGroup::undoText() const { return m_active ? m_active->undoText() : QString(); }

/*!
    Returns QUndoStack::redoText() of the active stack, or an empty string without one.
*/
QString QUndoGroup::redoText() const { return m_active ? m_active->redoText() : QString(); }

/*!
    Returns QUndoStack::isClean() of the active stack, or \c true without one.
*/
bool QUndoGroup::isClean() const { return !m_active || m_active->isClean(); }

/*!
    Undoes the last command of the active stack.

    \sa redo()
*/
void QUndoGroup::undo() {
    if (m_active)
        m_active->undo();
}

/*!
    Redoes the next command of the active stack.

    \sa undo()
*/
void QUndoGroup::redo() {
    if (m_active)
        m_active->redo();
}

#include "moc_qundogroup.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
