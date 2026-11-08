// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qdialog.h>
#include <QtWidgets/qdialogbuttonbox.h>

#include <QtCore/qpointer.h>
#include <QtWidgets/qabstractbutton.h>
#include <QtWidgets/qpushbutton.h>

class QCheckBox;
class QGridLayout;
class QLabel;
class QPlainTextEdit;

class QMessageBox : public QDialog {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(Icon icon READ icon WRITE setIcon)
    Q_PROPERTY(QString informativeText READ informativeText WRITE setInformativeText)
    Q_PROPERTY(QString detailedText READ detailedText WRITE setDetailedText)
public:
    enum Icon { NoIcon = 0, Information = 1, Warning = 2, Critical = 3, Question = 4 };
    Q_ENUM(Icon)

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

    explicit QMessageBox(QWidget *parent = nullptr);
    QMessageBox(Icon icon, const QString &title, const QString &text, StandardButtons buttons = NoButton,
                QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Dialog);
    ~QMessageBox() override;

    void addButton(QAbstractButton *button, ButtonRole role);
    QPushButton *addButton(const QString &text, ButtonRole role);
    QPushButton *addButton(StandardButton button);
    void removeButton(QAbstractButton *button);
    QList<QAbstractButton *> buttons() const;
    ButtonRole buttonRole(QAbstractButton *button) const;

    void setStandardButtons(StandardButtons buttons);
    StandardButtons standardButtons() const;
    StandardButton standardButton(QAbstractButton *button) const;
    QAbstractButton *button(StandardButton which) const;

    QPushButton *defaultButton() const { return m_default.data(); }
    void setDefaultButton(QPushButton *button);
    void setDefaultButton(StandardButton button);
    QAbstractButton *escapeButton() const { return m_escape.data(); }
    void setEscapeButton(QAbstractButton *button);
    void setEscapeButton(StandardButton button);
    QAbstractButton *clickedButton() const { return m_clicked.data(); }

    QString text() const;
    void setText(const QString &text);
    Icon icon() const { return m_icon; }
    void setIcon(Icon icon);
    QString informativeText() const;
    void setInformativeText(const QString &text);
    QString detailedText() const;
    void setDetailedText(const QString &text);
    void setCheckBox(QCheckBox *cb);
    QCheckBox *checkBox() const { return m_checkBox; }

    static StandardButton information(QWidget *parent, const QString &title, const QString &text,
                                      StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    static StandardButton question(QWidget *parent, const QString &title, const QString &text,
                                   StandardButtons buttons = StandardButtons(Yes) | No,
                                   StandardButton defaultButton = NoButton);
    static StandardButton warning(QWidget *parent, const QString &title, const QString &text,
                                  StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    static StandardButton critical(QWidget *parent, const QString &title, const QString &text,
                                   StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    static void about(QWidget *parent, const QString &title, const QString &text);

public slots:
    int exec() override;

signals:
    /*!
        This signal is emitted when \a button of the message box is clicked, before the box closes.
    */
    void buttonClicked(QAbstractButton *button) { emitSignal(&QMessageBox::buttonClicked, button); }

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void init();
    void clicked(QAbstractButton *button);
    QAbstractButton *detectedEscapeButton() const;
    int returnCode(QAbstractButton *button) const;
    void toggleDetails();

    QWidget *m_iconWidget = nullptr;
    QLabel *m_label = nullptr;
    QLabel *m_informative = nullptr;
    QPlainTextEdit *m_details = nullptr;
    QPushButton *m_detailsButton = nullptr;
    QCheckBox *m_checkBox = nullptr;
    QDialogButtonBox *m_box = nullptr;
    QGridLayout *m_grid = nullptr;
    QPointer<QPushButton> m_default;
    QPointer<QAbstractButton> m_escape;
    QPointer<QAbstractButton> m_clicked;
    Icon m_icon = NoIcon;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QMessageBox::StandardButtons)
