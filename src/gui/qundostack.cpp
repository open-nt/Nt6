// MIT License - see LICENSE
#include <QtGui/qundostack.h>

#include <QtCore/qlogging.h>
#include <QtGui/qaction.h>
#include <QtGui/qkeysequence.h>
#include <QtGui/qundogroup.h>

#include <algorithm>

/*!
    \class QUndoCommand
    \brief The QUndoCommand class is the base class of all commands stored on a QUndoStack.

    A command is one change to a document that can be taken back: redo() makes the change and undo() takes it
    back. Subclass it for each kind of change, and push the commands on a QUndoStack, which calls redo() once
    as the command is pushed:

    \code
    class AppendText : public QUndoCommand {
    public:
        AppendText(QString *document, const QString &text)
            : QUndoCommand("Append \"" + text + "\""), m_document(document), m_text(text) {}
        void redo() override { m_document->append(m_text); }
        void undo() override { m_document->chop(m_text.size()); }

    private:
        QString *m_document;
        QString m_text;
    };

    stack->push(new AppendText(&document, "hazel"));
    \endcode

    A command can have children, made with the command as their parent: the command's default redo() redoes
    them in order and its default undo() undoes them the other way round, so that a group of changes is taken
    back as one. Commands of the same kind can merge, so that typing a word is one step: give them the same
    id() and implement mergeWith().

    \differences
    None.
    \enddifferences

    \sa QUndoStack
*/

/*!
    Constructs a command with no text. If \a parent is not \c nullptr, the command is a child of it, which owns
    it.
*/
QUndoCommand::QUndoCommand(QUndoCommand *parent) {
    if (parent)
        parent->m_children.push_back(this);
}

/*!
    Constructs a command with the text \a text, a child of \a parent if \a parent is not \c nullptr.

    \sa setText()
*/
QUndoCommand::QUndoCommand(const QString &text, QUndoCommand *parent) : QUndoCommand(parent) { setText(text); }

/*!
    Destroys the command and its children.
*/
QUndoCommand::~QUndoCommand() {
    for (QUndoCommand *child : m_children)
        delete child;
}

/*!
    Takes the change of the command back. The default undoes the children, from the last to the first.

    \sa redo()
*/
void QUndoCommand::undo() {
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
        (*it)->undo();
}

/*!
    Makes the change of the command. The default redoes the children, in order. QUndoStack::push() calls it.

    \sa undo()
*/
void QUndoCommand::redo() {
    for (QUndoCommand *child : m_children)
        child->redo();
}

/*!
    \fn QString QUndoCommand::text() const

    Returns the text of the command, which a QUndoView shows for it.

    \sa setText(), actionText()
*/

/*!
    \fn QString QUndoCommand::actionText() const

    Returns the text that the undo and redo actions show for the command: "Undo" and "Redo" with this text.

    \sa setText(), QUndoStack::createUndoAction()
*/

/*!
    Sets the text of the command to \a text. A text with a newline gives the command two texts: the part before
    the newline is text(), for a QUndoView, and the part after it actionText(), for the undo and redo actions.

    \sa text(), actionText()
*/
void QUndoCommand::setText(const QString &text) {
    const qsizetype newline = text.indexOf(QLatin1Char('\n'));
    if (newline > 0) {
        m_text = text.left(newline);
        m_actionText = text.mid(newline + 1);
    } else {
        m_text = text;
        m_actionText = text;
    }
}

/*!
    \fn bool QUndoCommand::isObsolete() const

    Returns whether the command does nothing any more, so that the stack can drop it.

    \sa setObsolete()
*/

/*!
    \fn void QUndoCommand::setObsolete(bool obsolete)

    Marks the command as one that does nothing if \a obsolete is \c true. The stack deletes a command that is
    obsolete after it was pushed, merged, undone or redone, such as a move that ended where it started.

    \sa isObsolete()
*/

/*!
    Returns the kind of the command, for merging: commands with the same id other than -1 may merge. The
    default is -1, for a command that never merges.

    \sa mergeWith()
*/
int QUndoCommand::id() const { return -1; }

/*!
    Takes the change of \a other, a command of the same id() pushed after this one, into this command and
    returns \c true, or returns \c false if they do not merge. The stack deletes \a other when they do. The
    default returns \c false.

    \sa id()
*/
bool QUndoCommand::mergeWith(const QUndoCommand *other) {
    static_cast<void>(other);
    return false;
}

/*!
    \fn int QUndoCommand::childCount() const

    Returns the number of children of the command.

    \sa child()
*/

/*!
    Returns the child \a index of the command, or \c nullptr if there is none.

    \sa childCount()
*/
const QUndoCommand *QUndoCommand::child(int index) const {
    if (index < 0 || index >= childCount())
        return nullptr;
    return m_children[static_cast<std::size_t>(index)];
}

/*!
    \class QUndoStack
    \brief The QUndoStack class is a stack of QUndoCommand objects.

    An undo stack holds the commands that changed a document, in order, with an index: the commands below the
    index are done, those above it were undone and can be redone. push() does a command and puts it on top,
    which throws away what could be redone; undo() and redo() move the index.

    \code
    auto *stack = new QUndoStack(this);
    editMenu->addAction(stack->createUndoAction(this));
    editMenu->addAction(stack->createRedoAction(this));
    connect(stack, &QUndoStack::cleanChanged, this, [this](bool clean) { setWindowModified(!clean); });
    stack->push(new AppendText(&document, "hazel"));
    \endcode

    The stack is clean at the index where the document was saved: call setClean() then, and isClean() and
    cleanChanged() tell whether the document has changed since. beginMacro() and endMacro() make the commands
    pushed between them one step. setUndoLimit() keeps only so many steps. Several stacks, one for each
    document, can share undo and redo actions through a QUndoGroup.

    \differences
    None.
    \enddifferences

    \sa QUndoCommand, QUndoGroup, QUndoView
*/

/*!
    Constructs an empty stack with the given \a parent. It is clean, and active unless it joins a QUndoGroup.
*/
QUndoStack::QUndoStack(QObject *parent) : QObject(parent) {}

/*!
    Destroys the stack and the commands on it, and takes it out of its group.
*/
QUndoStack::~QUndoStack() {
    if (m_group)
        m_group->removeStack(this);
    for (QUndoCommand *command : m_commands)
        delete command;
}

// Emits the signals that tell where the stack is now.
void QUndoStack::announce() {
    emit indexChanged(m_index);
    emit canUndoChanged(canUndo());
    emit undoTextChanged(undoText());
    emit canRedoChanged(canRedo());
    emit redoTextChanged(redoText());
}

// Moves the index to `idx` without doing or undoing anything, makes it the clean index if `clean`, and emits
// what changed.
void QUndoStack::moveIndex(int idx, bool clean) {
    const bool wasClean = m_index == m_clean;
    if (idx != m_index) {
        m_index = idx;
        announce();
    }
    if (clean)
        m_clean = m_index;
    const bool isNowClean = m_index == m_clean;
    if (isNowClean != wasClean)
        emit cleanChanged(isNowClean);
}

// Deletes the commands that could be redone. Returns nothing; the clean index goes if it was among them.
void QUndoStack::dropRedo() {
    while (m_index < m_commands.size())
        delete m_commands.takeLast();
    if (m_clean > m_index)
        m_clean = -1;
}

// Deletes the oldest commands beyond the undo limit; returns true if there were any.
bool QUndoStack::checkUndoLimit() {
    if (m_limit <= 0 || !m_macros.isEmpty() || m_limit >= m_commands.size())
        return false;
    const auto drop = static_cast<int>(m_commands.size() - m_limit);
    for (int i = 0; i < drop; ++i)
        delete m_commands.takeFirst();
    m_index -= drop;
    if (m_clean != -1)
        m_clean = m_clean < drop ? -1 : m_clean - drop;
    return true;
}

/*!
    Deletes all commands. The stack is empty and clean afterwards, and a macro that was open is given up.

    \sa count()
*/
void QUndoStack::clear() {
    if (m_commands.isEmpty())
        return;
    const bool wasClean = isClean();
    m_macros.clear();
    for (QUndoCommand *command : m_commands)
        delete command;
    m_commands.clear();
    m_index = 0;
    m_clean = 0;
    announce();
    if (!wasClean)
        emit cleanChanged(true);
}

/*!
    Does the command \a cmd, by calling its redo(), and puts it on top of the stack, which takes it over. What
    could be redone is deleted. If the command on top has the same id(), the two may merge (see
    QUndoCommand::mergeWith()), unless the stack is clean there. Inside a macro the command becomes part of the
    macro. A command that is obsolete after redo() is deleted at once.

    \sa undo(), beginMacro()
*/
void QUndoStack::push(QUndoCommand *cmd) {
    if (!cmd)
        return;
    cmd->redo();
    const bool macro = !m_macros.isEmpty();
    QUndoCommand *top = nullptr;
    bool dropped = false;
    if (macro) {
        QUndoCommand *open = m_macros.last();
        if (!open->m_children.empty())
            top = open->m_children.back();
    } else {
        if (m_index > 0)
            top = m_commands.at(m_index - 1);
        dropped = m_index < m_commands.size();
        dropRedo();
    }
    const bool tryMerge = top && top->id() != -1 && top->id() == cmd->id() && (macro || m_index != m_clean);
    if (tryMerge && top->mergeWith(cmd)) {
        delete cmd;
        if (macro) {
            if (top->isObsolete()) {
                m_macros.last()->m_children.pop_back();
                delete top;
            }
        } else if (top->isObsolete()) {
            delete m_commands.takeAt(m_index - 1);
            moveIndex(m_index - 1, false);
        } else {
            announce(); // the texts changed
        }
        return;
    }
    if (cmd->isObsolete()) {
        delete cmd;
        if (dropped)
            announce();
        return;
    }
    if (macro) {
        m_macros.last()->m_children.push_back(cmd);
        return;
    }
    m_commands.append(cmd);
    checkUndoLimit();
    moveIndex(m_index + 1, false);
}

/*!
    Returns \c true if there is a command to undo: the index is above 0 and no macro is open.

    \sa undo(), canRedo()
*/
bool QUndoStack::canUndo() const { return m_macros.isEmpty() && m_index > 0; }

/*!
    Returns \c true if there is a command to redo: the index is below count() and no macro is open.

    \sa redo(), canUndo()
*/
bool QUndoStack::canRedo() const { return m_macros.isEmpty() && m_index < m_commands.size(); }

/*!
    Returns the action text of the command that undo() takes back, or an empty string if there is none.

    \sa QUndoCommand::actionText(), redoText()
*/
QString QUndoStack::undoText() const { return canUndo() ? m_commands.at(m_index - 1)->actionText() : QString(); }

/*!
    Returns the action text of the command that redo() does again, or an empty string if there is none.

    \sa QUndoCommand::actionText(), undoText()
*/
QString QUndoStack::redoText() const { return canRedo() ? m_commands.at(m_index)->actionText() : QString(); }

/*!
    \fn int QUndoStack::count() const

    Returns the number of commands on the stack; a macro counts as one.

    \sa index()
*/

/*!
    \fn int QUndoStack::index() const

    Returns the index of the stack: the number of commands that are done. undo() takes back the command at
    index() - 1 and redo() does the one at index().

    \sa setIndex()
*/

/*!
    Returns the text of the command at \a idx, or an empty string if there is none.

    \sa QUndoCommand::text()
*/
QString QUndoStack::text(int idx) const {
    return idx >= 0 && idx < m_commands.size() ? m_commands.at(idx)->text() : QString();
}

/*!
    Returns the command at \a index, or \c nullptr if there is none. The stack keeps it.
*/
const QUndoCommand *QUndoStack::command(int index) const {
    return index >= 0 && index < m_commands.size() ? m_commands.at(index) : nullptr;
}

namespace {
// The text of an undo or redo action: the prefix, or the default word, followed by the command's text.
QString actionLabel(const QString &prefix, const QString &word, const QString &text) {
    const QString start = prefix.isEmpty() ? word : prefix;
    return text.isEmpty() ? start : start + QLatin1Char(' ') + text;
}
} // namespace

/*!
    Returns a new action, a child of \a parent, that undoes the last command: it is enabled while canUndo() is
    true, and its text is \a prefix (by default "Undo") followed by undoText(). It has the standard undo
    shortcut.

    \sa createRedoAction(), QUndoGroup::createUndoAction()
*/
QAction *QUndoStack::createUndoAction(QObject *parent, const QString &prefix) const {
    auto *self = const_cast<QUndoStack *>(this); // NOLINT(cppcoreguidelines-pro-type-const-cast): Qt's signature
    auto *action = new QAction(actionLabel(prefix, QStringLiteral("Undo"), undoText()), parent);
    action->setEnabled(canUndo());
    action->setShortcut(QKeySequence(QKeySequence::Undo));
    connect(self, &QUndoStack::canUndoChanged, action, &QAction::setEnabled);
    connect(self, &QUndoStack::undoTextChanged, action, [action, prefix](const QString &text) {
        action->setText(actionLabel(prefix, QStringLiteral("Undo"), text));
    });
    connect(action, &QAction::triggered, self, &QUndoStack::undo);
    return action;
}

/*!
    Returns a new action, a child of \a parent, that redoes the next command: it is enabled while canRedo() is
    true, and its text is \a prefix (by default "Redo") followed by redoText(). It has the standard redo
    shortcut.

    \sa createUndoAction()
*/
QAction *QUndoStack::createRedoAction(QObject *parent, const QString &prefix) const {
    auto *self = const_cast<QUndoStack *>(this); // NOLINT(cppcoreguidelines-pro-type-const-cast): Qt's signature
    auto *action = new QAction(actionLabel(prefix, QStringLiteral("Redo"), redoText()), parent);
    action->setEnabled(canRedo());
    action->setShortcut(QKeySequence(QKeySequence::Redo));
    connect(self, &QUndoStack::canRedoChanged, action, &QAction::setEnabled);
    connect(self, &QUndoStack::redoTextChanged, action, [action, prefix](const QString &text) {
        action->setText(actionLabel(prefix, QStringLiteral("Redo"), text));
    });
    connect(action, &QAction::triggered, self, &QUndoStack::redo);
    return action;
}

/*!
    Returns \c true if the stack is the active stack of its QUndoGroup, or is in no group.

    \sa setActive()
*/
bool QUndoStack::isActive() const { return !m_group || m_group->activeStack() == this; }

/*!
    Returns \c true if the index is the clean index, where setClean() was last called, and no macro is open.

    \sa setClean(), cleanChanged()
*/
bool QUndoStack::isClean() const { return m_macros.isEmpty() && m_clean == m_index; }

/*!
    \fn int QUndoStack::cleanIndex() const

    Returns the clean index, or -1 if there is none any more because the command there was deleted.

    \sa setClean(), resetClean()
*/

/*!
    Starts a macro with the text \a text: the commands pushed until endMacro() become its children, and the
    macro is one step on the stack. Macros can be nested. While a macro is open nothing can be undone or
    redone.

    \sa endMacro()
*/
void QUndoStack::beginMacro(const QString &text) {
    auto *cmd = new QUndoCommand(text);
    if (m_macros.isEmpty()) {
        dropRedo();
        m_commands.append(cmd);
    } else {
        m_macros.last()->m_children.push_back(cmd);
    }
    m_macros.append(cmd);
    if (m_macros.size() == 1) {
        emit canUndoChanged(false);
        emit undoTextChanged(QString());
        emit canRedoChanged(false);
        emit redoTextChanged(QString());
    }
}

/*!
    Ends the macro that beginMacro() started last. When the outermost macro ends, it is a step on the stack.

    \sa beginMacro()
*/
void QUndoStack::endMacro() {
    if (m_macros.isEmpty()) {
        qWarning("QUndoStack::endMacro(): no matching beginMacro()");
        return;
    }
    m_macros.removeLast();
    if (m_macros.isEmpty()) {
        checkUndoLimit();
        moveIndex(m_index + 1, false);
    }
}

/*!
    Keeps at most \a limit commands on the stack, deleting the oldest; 0, the default, keeps them all. The limit
    can only be set while the stack is empty.

    \sa undoLimit()
*/
void QUndoStack::setUndoLimit(int limit) {
    if (!m_commands.isEmpty()) {
        qWarning("QUndoStack::setUndoLimit(): an undo limit can only be set when the stack is empty");
        return;
    }
    m_limit = limit < 0 ? 0 : limit;
}

/*!
    \fn int QUndoStack::undoLimit() const

    Returns the most commands that the stack keeps, or 0 for no limit.

    \sa setUndoLimit()
*/

/*!
    Marks the index as the clean one, where the document is as it was saved, and emits cleanChanged() if the
    stack was not clean.

    \sa isClean(), resetClean()
*/
void QUndoStack::setClean() {
    if (!m_macros.isEmpty()) {
        qWarning("QUndoStack::setClean(): cannot set clean in the middle of a macro");
        return;
    }
    moveIndex(m_index, true);
}

/*!
    Forgets the clean index, so that the stack is not clean anywhere, for a document that was never saved.

    \sa setClean()
*/
void QUndoStack::resetClean() {
    const bool wasClean = isClean();
    m_clean = -1;
    if (wasClean)
        emit cleanChanged(false);
}

/*!
    Undoes or redoes commands until the index is \a idx, between 0 and count().

    \sa index(), undo(), redo()
*/
void QUndoStack::setIndex(int idx) {
    if (!m_macros.isEmpty()) {
        qWarning("QUndoStack::setIndex(): cannot set the index in the middle of a macro");
        return;
    }
    idx = std::clamp(idx, 0, count());
    int i = m_index;
    while (i < idx)
        m_commands.at(i++)->redo();
    while (i > idx)
        m_commands.at(--i)->undo();
    moveIndex(idx, false);
}

/*!
    Undoes the command below the index and moves the index down, if canUndo() is true. A command that is
    obsolete afterwards is deleted.

    \sa redo(), canUndo()
*/
void QUndoStack::undo() {
    if (!canUndo())
        return;
    const int idx = m_index - 1;
    QUndoCommand *cmd = m_commands.at(idx);
    cmd->undo();
    if (cmd->isObsolete()) {
        delete m_commands.takeAt(idx);
        if (m_clean > idx)
            resetClean();
    }
    moveIndex(idx, false);
}

/*!
    Redoes the command at the index and moves the index up, if canRedo() is true. A command that is obsolete
    afterwards is deleted.

    \sa undo(), canRedo()
*/
void QUndoStack::redo() {
    if (!canRedo())
        return;
    const int idx = m_index;
    QUndoCommand *cmd = m_commands.at(idx);
    cmd->redo();
    if (cmd->isObsolete()) {
        delete m_commands.takeAt(idx);
        if (m_clean > idx)
            resetClean();
        announce();
        return;
    }
    moveIndex(m_index + 1, false);
}

/*!
    Makes the stack the active stack of its QUndoGroup if \a active is \c true, or stops it being the active
    one. A stack in no group is always active.

    \sa isActive(), QUndoGroup::setActiveStack()
*/
void QUndoStack::setActive(bool active) {
    if (!m_group)
        return;
    if (active)
        m_group->setActiveStack(this);
    else if (m_group->activeStack() == this)
        m_group->setActiveStack(nullptr);
}

#include "moc_qundostack.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
