// MIT License - see LICENSE
#pragma once

#include <QtCore/qstringlist.h>
#include <QtWidgets/qdialog.h>
#include <QtWidgets/qlineedit.h>

class QComboBox;
class QDialogButtonBox;
class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QSpinBox;
class QVBoxLayout;

class QInputDialog : public QDialog {
    Q_OBJECT
    Q_PROPERTY(InputMode inputMode READ inputMode WRITE setInputMode)
    Q_PROPERTY(QString labelText READ labelText WRITE setLabelText)
    Q_PROPERTY(QString textValue READ textValue WRITE setTextValue NOTIFY textValueChanged)
    Q_PROPERTY(int intValue READ intValue WRITE setIntValue NOTIFY intValueChanged)
    Q_PROPERTY(double doubleValue READ doubleValue WRITE setDoubleValue NOTIFY doubleValueChanged)
    Q_PROPERTY(QString okButtonText READ okButtonText WRITE setOkButtonText)
    Q_PROPERTY(QString cancelButtonText READ cancelButtonText WRITE setCancelButtonText)
public:
    enum InputDialogOption {
        NoButtons = 0x00000001,
        UseListViewForComboBoxItems = 0x00000002,
        UsePlainTextEditForTextInput = 0x00000004
    };
    Q_DECLARE_FLAGS(InputDialogOptions, InputDialogOption)

    enum InputMode { TextInput, IntInput, DoubleInput };
    Q_ENUM(InputMode)

    explicit QInputDialog(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    ~QInputDialog() override;

    void setInputMode(InputMode mode);
    InputMode inputMode() const { return m_mode; }
    void setLabelText(const QString &text);
    QString labelText() const;
    void setOption(InputDialogOption option, bool on = true);
    bool testOption(InputDialogOption option) const { return m_options.testFlag(option); }
    void setOptions(InputDialogOptions options);
    InputDialogOptions options() const { return m_options; }

    void setTextValue(const QString &text);
    QString textValue() const;
    void setTextEchoMode(QLineEdit::EchoMode mode);
    QLineEdit::EchoMode textEchoMode() const;
    void setComboBoxItems(const QStringList &items);
    QStringList comboBoxItems() const { return m_items; }
    void setComboBoxEditable(bool editable) { m_comboEditable = editable; }
    bool isComboBoxEditable() const { return m_comboEditable; }

    void setIntValue(int value);
    int intValue() const;
    void setIntMinimum(int min);
    int intMinimum() const;
    void setIntMaximum(int max);
    int intMaximum() const;
    void setIntRange(int min, int max);
    void setIntStep(int step);
    int intStep() const;

    void setDoubleValue(double value);
    double doubleValue() const;
    void setDoubleMinimum(double min);
    double doubleMinimum() const;
    void setDoubleMaximum(double max);
    double doubleMaximum() const;
    void setDoubleRange(double min, double max);
    void setDoubleDecimals(int decimals);
    int doubleDecimals() const;
    void setDoubleStep(double step);
    double doubleStep() const;

    void setOkButtonText(const QString &text);
    QString okButtonText() const;
    void setCancelButtonText(const QString &text);
    QString cancelButtonText() const;

    void done(int result) override;

    static QString getText(QWidget *parent, const QString &title, const QString &label,
                           QLineEdit::EchoMode echo = QLineEdit::Normal, const QString &text = QString(),
                           bool *ok = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    static QString getMultiLineText(QWidget *parent, const QString &title, const QString &label,
                                    const QString &text = QString(), bool *ok = nullptr,
                                    Qt::WindowFlags flags = Qt::WindowFlags());
    static QString getItem(QWidget *parent, const QString &title, const QString &label, const QStringList &items,
                           int current = 0, bool editable = true, bool *ok = nullptr,
                           Qt::WindowFlags flags = Qt::WindowFlags());
    static int getInt(QWidget *parent, const QString &title, const QString &label, int value = 0,
                      int minValue = -2147483647, int maxValue = 2147483647, int step = 1, bool *ok = nullptr,
                      Qt::WindowFlags flags = Qt::WindowFlags());
    static double getDouble(QWidget *parent, const QString &title, const QString &label, double value = 0,
                            double minValue = -2147483647, double maxValue = 2147483647, int decimals = 1,
                            bool *ok = nullptr, Qt::WindowFlags flags = Qt::WindowFlags(), double step = 1);

signals:
    /*!
        This signal is emitted when the text changes to \a text in TextInput mode.
    */
    void textValueChanged(const QString &text) { emitSignal(&QInputDialog::textValueChanged, text); }
    /*!
        This signal is emitted when the dialog is accepted in TextInput mode, with the \a text.
    */
    void textValueSelected(const QString &text) { emitSignal(&QInputDialog::textValueSelected, text); }
    /*!
        This signal is emitted when the value changes to \a value in IntInput mode.
    */
    void intValueChanged(int value) { emitSignal(&QInputDialog::intValueChanged, value); }
    /*!
        This signal is emitted when the dialog is accepted in IntInput mode, with the \a value.
    */
    void intValueSelected(int value) { emitSignal(&QInputDialog::intValueSelected, value); }
    /*!
        This signal is emitted when the value changes to \a value in DoubleInput mode.
    */
    void doubleValueChanged(double value) { emitSignal(&QInputDialog::doubleValueChanged, value); }
    /*!
        This signal is emitted when the dialog is accepted in DoubleInput mode, with the \a value.
    */
    void doubleValueSelected(double value) { emitSignal(&QInputDialog::doubleValueSelected, value); }

private:
    void updateInput();

    QVBoxLayout *m_layout = nullptr;
    QLabel *m_label = nullptr;
    QLineEdit *m_line = nullptr;
    QPlainTextEdit *m_plain = nullptr;
    QComboBox *m_combo = nullptr;
    QSpinBox *m_int = nullptr;
    QDoubleSpinBox *m_double = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
    QStringList m_items;
    InputMode m_mode = TextInput;
    InputDialogOptions m_options;
    bool m_comboEditable = true;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QInputDialog::InputDialogOptions)
