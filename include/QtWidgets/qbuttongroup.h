// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtWidgets/qabstractbutton.h>

#include <vector>

class QButtonGroup : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool exclusive READ exclusive WRITE setExclusive)
public:
    explicit QButtonGroup(QObject *parent = nullptr);
    ~QButtonGroup() override;

    void setExclusive(bool exclusive) { m_exclusive = exclusive; }
    bool exclusive() const { return m_exclusive; }

    void addButton(QAbstractButton *button, int id = -1);
    void removeButton(QAbstractButton *button);
    QList<QAbstractButton *> buttons() const;
    QAbstractButton *checkedButton() const;
    QAbstractButton *button(int id) const;
    void setId(QAbstractButton *button, int id);
    int id(QAbstractButton *button) const;
    int checkedId() const;

signals:
    /*!
        This signal is emitted when \a button of the group is clicked, after its own clicked() signal.

        \sa idClicked()
    */
    void buttonClicked(QAbstractButton *button) { emitSignal(&QButtonGroup::buttonClicked, button); }
    /*!
        This signal is emitted when \a button of the group is pressed down.

        \sa idPressed()
    */
    void buttonPressed(QAbstractButton *button) { emitSignal(&QButtonGroup::buttonPressed, button); }
    /*!
        This signal is emitted when \a button of the group is released.

        \sa idReleased()
    */
    void buttonReleased(QAbstractButton *button) { emitSignal(&QButtonGroup::buttonReleased, button); }
    /*!
        This signal is emitted when \a button of the group is toggled; \a checked is its new state.

        \sa idToggled()
    */
    void buttonToggled(QAbstractButton *button, bool checked) {
        emitSignal(&QButtonGroup::buttonToggled, button, checked);
    }
    /*!
        This signal is emitted when the button with the given \a id is clicked, just before buttonClicked().
    */
    void idClicked(int id) { emitSignal(&QButtonGroup::idClicked, id); }
    /*!
        This signal is emitted when the button with the given \a id is pressed down.
    */
    void idPressed(int id) { emitSignal(&QButtonGroup::idPressed, id); }
    /*!
        This signal is emitted when the button with the given \a id is released.
    */
    void idReleased(int id) { emitSignal(&QButtonGroup::idReleased, id); }
    /*!
        This signal is emitted when the button with the given \a id is toggled; \a checked is its new state.
    */
    void idToggled(int id, bool checked) { emitSignal(&QButtonGroup::idToggled, id, checked); }

private:
    friend class QAbstractButton;
    struct Entry {
        QAbstractButton *button;
        int id;
    };

    std::vector<Entry> m_buttons;
    bool m_exclusive = true;
    int m_nextAutoId = -2;
};
