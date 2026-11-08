// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>
#include <QtGui/qfont.h>
#include <QtGui/qkeysequence.h>

class QActionGroup;
class QWidget;

class QAction : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool checkable READ isCheckable WRITE setCheckable NOTIFY checkableChanged)
    Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY toggled)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY changed)
    Q_PROPERTY(QString toolTip READ toolTip WRITE setToolTip NOTIFY changed)
    Q_PROPERTY(QString statusTip READ statusTip WRITE setStatusTip NOTIFY changed)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
public:
    enum ActionEvent { Trigger, Hover };

    explicit QAction(QObject *parent = nullptr);
    explicit QAction(const QString &text, QObject *parent = nullptr);
    ~QAction() override;

    QString text() const { return m_text; }
    void setText(const QString &text);
    QString iconText() const;
    QString toolTip() const;
    void setToolTip(const QString &tip);
    QString statusTip() const { return m_statusTip; }
    void setStatusTip(const QString &tip);
    QFont font() const { return m_font; }
    void setFont(const QFont &font);
    QKeySequence shortcut() const { return m_shortcut; }
    void setShortcut(const QKeySequence &shortcut);
    QVariant data() const { return m_data; }
    void setData(const QVariant &data) { m_data = data; }

    bool isCheckable() const { return m_checkable; }
    void setCheckable(bool checkable);
    bool isChecked() const { return m_checkable && m_checked; }
    bool isEnabled() const { return m_enabled && m_visible; }
    bool isVisible() const { return m_visible; }
    bool isSeparator() const { return m_separator; }
    void setSeparator(bool separator);

    QActionGroup *actionGroup() const { return m_group; }
    void setActionGroup(QActionGroup *group);
    template <typename T> T menu() const { return dynamic_cast<T>(m_menu.data()); }
    void setMenu(QObject *menu);
    QList<QObject *> associatedObjects() const;
    void activate(ActionEvent event);

public slots:
    void trigger() { activate(Trigger); }
    void hover() { activate(Hover); }
    void toggle();
    void setChecked(bool checked);
    void setEnabled(bool enabled);
    void setDisabled(bool disabled) { setEnabled(!disabled); }
    void setVisible(bool visible);

signals:
    /*!
        This signal is emitted when anything about the action changes: its text, tool tip, shortcut, font, whether it is
        checkable, checked, enabled or visible, or whether it is a separator.
    */
    void changed() { emitSignal(&QAction::changed); }
    /*!
        This signal is emitted when the action is enabled or disabled, with the new state \a enabled. An action that is
        not visible is not enabled.
    */
    void enabledChanged(bool enabled) { emitSignal(&QAction::enabledChanged, enabled); }
    /*!
        This signal is emitted when the action becomes checkable or stops being it, with the new state \a checkable.
    */
    void checkableChanged(bool checkable) { emitSignal(&QAction::checkableChanged, checkable); }
    /*!
        This signal is emitted when the action is shown or hidden.
    */
    void visibleChanged() { emitSignal(&QAction::visibleChanged); }
    /*!
        This signal is emitted when the action is triggered, by the user or by trigger(). \a checked is the state of a
        checkable action after it has toggled, and \c false for any other.
    */
    void triggered(bool checked = false) { emitSignal(&QAction::triggered, checked); }
    /*!
        This signal is emitted when the user hovers over the action in a menu, and by hover().
    */
    void hovered() { emitSignal(&QAction::hovered); }
    /*!
        This signal is emitted when a checkable action is checked or unchecked, with the new state \a checked.
    */
    void toggled(bool checked) { emitSignal(&QAction::toggled, checked); }

private:
    friend class QActionGroup;
    friend class QWidget;
    void addAssociated(QObject *object);
    void removeAssociated(QObject *object);

    QString m_text;
    QString m_toolTip;
    QString m_statusTip;
    QFont m_font;
    QKeySequence m_shortcut;
    QVariant m_data;
    QPointer<QObject> m_menu;
    QActionGroup *m_group = nullptr;
    QList<QPointer<QObject>> m_associated;
    bool m_checkable = false;
    bool m_checked = false;
    bool m_enabled = true;
    bool m_visible = true;
    bool m_separator = false;
};
