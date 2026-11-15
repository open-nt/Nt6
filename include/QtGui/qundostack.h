// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtCore/qstring.h>

#include <vector>

class QAction;
class QUndoGroup;
class QUndoStack;

class QUndoCommand {
public:
    explicit QUndoCommand(QUndoCommand *parent = nullptr);
    explicit QUndoCommand(const QString &text, QUndoCommand *parent = nullptr);
    virtual ~QUndoCommand();
    QUndoCommand(const QUndoCommand &) = delete;
    QUndoCommand &operator=(const QUndoCommand &) = delete;

    virtual void undo();
    virtual void redo();

    QString text() const { return m_text; }
    QString actionText() const { return m_actionText; }
    void setText(const QString &text);

    bool isObsolete() const { return m_obsolete; }
    void setObsolete(bool obsolete) { m_obsolete = obsolete; }

    virtual int id() const;
    virtual bool mergeWith(const QUndoCommand *other);

    int childCount() const { return static_cast<int>(m_children.size()); }
    const QUndoCommand *child(int index) const;

private:
    friend class QUndoStack;
    std::vector<QUndoCommand *> m_children; // owned, in the order they were made
    QString m_text;
    QString m_actionText;
    bool m_obsolete = false;
};

class QUndoStack : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive WRITE setActive)
    Q_PROPERTY(int undoLimit READ undoLimit WRITE setUndoLimit)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY canUndoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY canRedoChanged)
    Q_PROPERTY(QString undoText READ undoText NOTIFY undoTextChanged)
    Q_PROPERTY(QString redoText READ redoText NOTIFY redoTextChanged)
    Q_PROPERTY(bool clean READ isClean NOTIFY cleanChanged)
public:
    explicit QUndoStack(QObject *parent = nullptr);
    ~QUndoStack() override;

    void clear();
    void push(QUndoCommand *cmd);

    bool canUndo() const;
    bool canRedo() const;
    QString undoText() const;
    QString redoText() const;

    int count() const { return static_cast<int>(m_commands.size()); }
    int index() const { return m_index; }
    QString text(int idx) const;
    const QUndoCommand *command(int index) const;

    QAction *createUndoAction(QObject *parent, const QString &prefix = QString()) const;
    QAction *createRedoAction(QObject *parent, const QString &prefix = QString()) const;

    bool isActive() const;
    bool isClean() const;
    int cleanIndex() const { return m_clean; }

    void beginMacro(const QString &text);
    void endMacro();

    void setUndoLimit(int limit);
    int undoLimit() const { return m_limit; }

public slots:
    void setClean();
    void resetClean();
    void setIndex(int idx);
    void undo();
    void redo();
    void setActive(bool active = true);

signals:
    /*! This signal is emitted when the index of the stack changes to \a idx, after undo(), redo() or push(). */
    void indexChanged(int idx) { emitSignal(&QUndoStack::indexChanged, idx); }
    /*! This signal is emitted when the stack becomes clean or stops being clean; \a clean is the new state. */
    void cleanChanged(bool clean) { emitSignal(&QUndoStack::cleanChanged, clean); }
    /*! This signal is emitted when canUndo() changes to \a canUndo. */
    void canUndoChanged(bool canUndo) { emitSignal(&QUndoStack::canUndoChanged, canUndo); }
    /*! This signal is emitted when canRedo() changes to \a canRedo. */
    void canRedoChanged(bool canRedo) { emitSignal(&QUndoStack::canRedoChanged, canRedo); }
    /*! This signal is emitted when undoText() changes to \a undoText. */
    void undoTextChanged(const QString &undoText) { emitSignal(&QUndoStack::undoTextChanged, undoText); }
    /*! This signal is emitted when redoText() changes to \a redoText. */
    void redoTextChanged(const QString &redoText) { emitSignal(&QUndoStack::redoTextChanged, redoText); }

private:
    friend class QUndoGroup;
    void moveIndex(int idx, bool clean);
    void announce();
    void dropRedo();
    bool checkUndoLimit();

    QList<QUndoCommand *> m_commands; // owned; those below m_index are done
    QList<QUndoCommand *> m_macros;   // the macros that are open, outermost first
    QPointer<QUndoGroup> m_group;
    int m_index = 0;
    int m_clean = 0;
    int m_limit = 0;
};
