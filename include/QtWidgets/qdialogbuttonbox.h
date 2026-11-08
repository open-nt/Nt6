// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtWidgets/qabstractbutton.h>
#include <QtWidgets/qwidget.h>

#include <vector>

class QBoxLayout;
class QPushButton;

class QDialogButtonBox : public QWidget {
    Q_OBJECT
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation)
    Q_PROPERTY(bool centerButtons READ centerButtons WRITE setCenterButtons)
public:
    enum ButtonRole {
        InvalidRole = -1,
        AcceptRole,
        RejectRole,
        DestructiveRole,
        ActionRole,
        HelpRole,
        YesRole,
        NoRole,
        ResetRole,
        ApplyRole,
        NRoles
    };
    Q_ENUM(ButtonRole)

    enum StandardButton {
        NoButton = 0x00000000,
        Ok = 0x00000400,
        Save = 0x00000800,
        SaveAll = 0x00001000,
        Open = 0x00002000,
        Yes = 0x00004000,
        YesToAll = 0x00008000,
        No = 0x00010000,
        NoToAll = 0x00020000,
        Abort = 0x00040000,
        Retry = 0x00080000,
        Ignore = 0x00100000,
        Close = 0x00200000,
        Cancel = 0x00400000,
        Discard = 0x00800000,
        Help = 0x01000000,
        Apply = 0x02000000,
        Reset = 0x04000000,
        RestoreDefaults = 0x08000000,
        FirstButton = Ok,
        LastButton = RestoreDefaults
    };
    Q_DECLARE_FLAGS(StandardButtons, StandardButton)

    enum ButtonLayout { WinLayout, MacLayout, KdeLayout, GnomeLayout, AndroidLayout };
    Q_ENUM(ButtonLayout)

    explicit QDialogButtonBox(QWidget *parent = nullptr);
    explicit QDialogButtonBox(Qt::Orientation orientation, QWidget *parent = nullptr);
    explicit QDialogButtonBox(StandardButtons buttons, QWidget *parent = nullptr);
    QDialogButtonBox(StandardButtons buttons, Qt::Orientation orientation, QWidget *parent = nullptr);
    ~QDialogButtonBox() override;

    void setOrientation(Qt::Orientation orientation);
    Qt::Orientation orientation() const { return m_orientation; }

    void addButton(QAbstractButton *button, ButtonRole role);
    QPushButton *addButton(const QString &text, ButtonRole role);
    QPushButton *addButton(StandardButton button);
    void removeButton(QAbstractButton *button);
    void clear();

    QList<QAbstractButton *> buttons() const;
    ButtonRole buttonRole(QAbstractButton *button) const;

    void setStandardButtons(StandardButtons buttons);
    StandardButtons standardButtons() const;
    StandardButton standardButton(QAbstractButton *button) const;
    QPushButton *button(StandardButton which) const;

    void setCenterButtons(bool center);
    bool centerButtons() const { return m_center; }

signals:
    /*!
        This signal is emitted when \a button of the box is clicked, before accepted(), rejected() or helpRequested().
    */
    void clicked(QAbstractButton *button) { emitSignal(&QDialogButtonBox::clicked, button); }
    /*!
        This signal is emitted when a button with the AcceptRole or the YesRole is clicked.
    */
    void accepted() { emitSignal(&QDialogButtonBox::accepted); }
    /*!
        This signal is emitted when a button with the HelpRole is clicked.
    */
    void helpRequested() { emitSignal(&QDialogButtonBox::helpRequested); }
    /*!
        This signal is emitted when a button with the RejectRole or the NoRole is clicked.
    */
    void rejected() { emitSignal(&QDialogButtonBox::rejected); }

protected:
    void showEvent(QShowEvent *event) override;

private:
    struct Entry {
        QAbstractButton *button;
        ButtonRole role;
        StandardButton standard;
    };

    void add(QAbstractButton *button, ButtonRole role, StandardButton standard);
    QPushButton *createStandardButton(StandardButton which);
    void relayout();
    void buttonClicked(QAbstractButton *button);
    void forget(QObject *button);

    std::vector<Entry> m_buttons;
    QBoxLayout *m_layout = nullptr;
    Qt::Orientation m_orientation = Qt::Horizontal;
    bool m_center = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QDialogButtonBox::StandardButtons)
