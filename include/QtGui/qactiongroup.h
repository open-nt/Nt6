// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtGui/qaction.h>

class QActionGroup : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool exclusive READ isExclusive WRITE setExclusive)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible)
public:
    enum class ExclusionPolicy { None, Exclusive, ExclusiveOptional };

    explicit QActionGroup(QObject *parent = nullptr);
    ~QActionGroup() override;

    QAction *addAction(QAction *action);
    QAction *addAction(const QString &text);
    void removeAction(QAction *action);
    QList<QAction *> actions() const;
    QAction *checkedAction() const;
    bool isExclusive() const { return m_policy != ExclusionPolicy::None; }
    bool isEnabled() const { return m_enabled; }
    bool isVisible() const { return m_visible; }
    ExclusionPolicy exclusionPolicy() const { return m_policy; }

public slots:
    void setEnabled(bool enabled);
    void setDisabled(bool disabled) { setEnabled(!disabled); }
    void setVisible(bool visible);
    void setExclusive(bool exclusive);
    void setExclusionPolicy(ExclusionPolicy policy);

signals:
    /*!
        This signal is emitted when the user triggers an action of the group, with that \a action.
    */
    void triggered(QAction *action) { emitSignal(&QActionGroup::triggered, action); }
    /*!
        This signal is emitted when the user hovers over an action of the group in a menu, with that \a action.
    */
    void hovered(QAction *action) { emitSignal(&QActionGroup::hovered, action); }

private:
    friend class QAction;
    void actionToggled(QAction *action, bool checked);

    QList<QPointer<QAction>> m_actions;
    ExclusionPolicy m_policy = ExclusionPolicy::Exclusive;
    bool m_enabled = true;
    bool m_visible = true;
};
