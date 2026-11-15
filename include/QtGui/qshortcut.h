// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtGui/qkeysequence.h>

#include <type_traits>
#include <utility>

class QShortcut : public QObject {
    Q_OBJECT
    Q_PROPERTY(QKeySequence key READ key WRITE setKey)
    Q_PROPERTY(QString whatsThis READ whatsThis WRITE setWhatsThis)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled)
    Q_PROPERTY(bool autoRepeat READ autoRepeat WRITE setAutoRepeat)
    Q_PROPERTY(Qt::ShortcutContext context READ context WRITE setContext)
public:
    explicit QShortcut(QObject *parent);
    explicit QShortcut(const QKeySequence &key, QObject *parent, const char *member = nullptr,
                       const char *ambiguousMember = nullptr, Qt::ShortcutContext context = Qt::WindowShortcut);
    explicit QShortcut(QKeySequence::StandardKey key, QObject *parent, const char *member = nullptr,
                       const char *ambiguousMember = nullptr, Qt::ShortcutContext context = Qt::WindowShortcut);
    /*!
        Constructs a shortcut for \a key, active in \a shortcutContext relative to \a context (its parent), that calls
        \a functor when it is activated.
    */
    template <typename Functor, typename = std::enable_if_t<std::is_invocable_v<Functor>>>
    QShortcut(const QKeySequence &key, QObject *context, Functor functor,
              Qt::ShortcutContext shortcutContext = Qt::WindowShortcut)
        : QShortcut(key, context, nullptr, nullptr, shortcutContext) {
        QObject::connect(this, &QShortcut::activated, context, std::move(functor));
    }
    ~QShortcut() override;

    void setKey(const QKeySequence &key);
    QKeySequence key() const;
    void setKeys(QKeySequence::StandardKey key);
    void setKeys(const QList<QKeySequence> &keys);
    QList<QKeySequence> keys() const;

    void setEnabled(bool enable);
    bool isEnabled() const;

    void setContext(Qt::ShortcutContext context);
    Qt::ShortcutContext context() const;

    void setAutoRepeat(bool on);
    bool autoRepeat() const;

    void setWhatsThis(const QString &text);
    QString whatsThis() const;

signals:
    /*!
        This signal is emitted when the key sequence of the shortcut is pressed where the shortcut is active.
    */
    void activated() { emitSignal(&QShortcut::activated); }
    /*!
        This signal is emitted instead of activated() when another active shortcut has the same key sequence.
    */
    void activatedAmbiguously() { emitSignal(&QShortcut::activatedAmbiguously); }

private:
    QList<QKeySequence> m_keys;
    QString m_whatsThis;
    Qt::ShortcutContext m_context = Qt::WindowShortcut;
    bool m_enabled = true;
    bool m_autoRepeat = true;
};
