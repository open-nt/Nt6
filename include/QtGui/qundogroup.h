// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtCore/qstring.h>

#include <vector>

class QAction;
class QUndoStack;

class QUndoGroup : public QObject {
    Q_OBJECT
public:
    explicit QUndoGroup(QObject *parent = nullptr);
    ~QUndoGroup() override;

    void addStack(QUndoStack *stack);
    void removeStack(QUndoStack *stack);
    QList<QUndoStack *> stacks() const { return m_stacks; }
    QUndoStack *activeStack() const;

    QAction *createUndoAction(QObject *parent, const QString &prefix = QString()) const;
    QAction *createRedoAction(QObject *parent, const QString &prefix = QString()) const;

    bool canUndo() const;
    bool canRedo() const;
    QString undoText() const;
    QString redoText() const;
    bool isClean() const;

public slots:
    void undo();
    void redo();
    void setActiveStack(QUndoStack *stack);

signals:
    /*! This signal is emitted when the active stack changes to \a stack, which may be \c nullptr. */
    void activeStackChanged(QUndoStack *stack) { emitSignal(&QUndoGroup::activeStackChanged, stack); }
    /*! This signal is emitted when the index of the active stack changes to \a idx. */
    void indexChanged(int idx) { emitSignal(&QUndoGroup::indexChanged, idx); }
    /*! This signal is emitted when the active stack becomes clean or stops being clean; \a clean is the new state. */
    void cleanChanged(bool clean) { emitSignal(&QUndoGroup::cleanChanged, clean); }
    /*! This signal is emitted when canUndo() changes to \a canUndo. */
    void canUndoChanged(bool canUndo) { emitSignal(&QUndoGroup::canUndoChanged, canUndo); }
    /*! This signal is emitted when canRedo() changes to \a canRedo. */
    void canRedoChanged(bool canRedo) { emitSignal(&QUndoGroup::canRedoChanged, canRedo); }
    /*! This signal is emitted when undoText() changes to \a undoText. */
    void undoTextChanged(const QString &undoText) { emitSignal(&QUndoGroup::undoTextChanged, undoText); }
    /*! This signal is emitted when redoText() changes to \a redoText. */
    void redoTextChanged(const QString &redoText) { emitSignal(&QUndoGroup::redoTextChanged, redoText); }

private:
    QList<QUndoStack *> m_stacks;
    QPointer<QUndoStack> m_active;
    std::vector<QMetaObject::Connection> m_activeConnections;
};
