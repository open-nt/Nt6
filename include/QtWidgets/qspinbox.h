// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qabstractspinbox.h>

class QSpinBox : public QAbstractSpinBox {
    Q_OBJECT
    Q_PROPERTY(int minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(int singleStep READ singleStep WRITE setSingleStep)
    Q_PROPERTY(QString prefix READ prefix WRITE setPrefix)
    Q_PROPERTY(QString suffix READ suffix WRITE setSuffix)
    Q_PROPERTY(QString cleanText READ cleanText)
public:
    explicit QSpinBox(QWidget *parent = nullptr);
    ~QSpinBox() override;

    int minimum() const { return m_minimum; }
    void setMinimum(int minimum);
    int maximum() const { return m_maximum; }
    void setMaximum(int maximum);
    void setRange(int minimum, int maximum);
    int value() const { return m_value; }
    int singleStep() const { return m_step; }
    void setSingleStep(int step);
    QString prefix() const { return m_prefix; }
    void setPrefix(const QString &prefix);
    QString suffix() const { return m_suffix; }
    void setSuffix(const QString &suffix);
    QString cleanText() const;
    void stepBy(int steps) override;

public slots:
    void setValue(int value);

signals:
    /*!
        This signal is emitted when the value changes to \a value, whether the user changed it by typing or with the
        buttons, or setValue() did.
    */
    void valueChanged(int value) { emitSignal(&QSpinBox::valueChanged, value); }
    /*!
        This signal is emitted when the value changes, with the \a text that shows it, prefix and suffix included.
    */
    void textChanged(const QString &text) { emitSignal(&QSpinBox::textChanged, text); }

protected:
    virtual QString textFromValue(int value) const;
    virtual int valueFromText(const QString &text) const;
    StepEnabled stepEnabled() const override;

private:
    QString valueText() const override;
    QStringList sizeTexts() const override;
    void takeText(const QString &number, bool atMinimum, bool finished) override;
    bool acceptsCharacter(QChar c) const override;
    void changeValue(int value, bool rewrite);

    int m_minimum = 0;
    int m_maximum = 99;
    int m_value = 0;
    int m_step = 1;
};

class QDoubleSpinBox : public QAbstractSpinBox {
    Q_OBJECT
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(double singleStep READ singleStep WRITE setSingleStep)
    Q_PROPERTY(int decimals READ decimals WRITE setDecimals)
    Q_PROPERTY(QString prefix READ prefix WRITE setPrefix)
    Q_PROPERTY(QString suffix READ suffix WRITE setSuffix)
    Q_PROPERTY(QString cleanText READ cleanText)
public:
    explicit QDoubleSpinBox(QWidget *parent = nullptr);
    ~QDoubleSpinBox() override;

    double minimum() const { return m_minimum; }
    void setMinimum(double minimum);
    double maximum() const { return m_maximum; }
    void setMaximum(double maximum);
    void setRange(double minimum, double maximum);
    double value() const { return m_value; }
    double singleStep() const { return m_step; }
    void setSingleStep(double step);
    int decimals() const { return m_decimals; }
    void setDecimals(int decimals);
    QString prefix() const { return m_prefix; }
    void setPrefix(const QString &prefix);
    QString suffix() const { return m_suffix; }
    void setSuffix(const QString &suffix);
    QString cleanText() const;
    void stepBy(int steps) override;

    virtual QString textFromValue(double value) const;
    virtual double valueFromText(const QString &text) const;

public slots:
    void setValue(double value);

signals:
    /*!
        This signal is emitted when the value changes to \a value, whether the user changed it by typing or with the
        buttons, or setValue() did.
    */
    void valueChanged(double value) { emitSignal(&QDoubleSpinBox::valueChanged, value); }
    /*!
        This signal is emitted when the value changes, with the \a text that shows it, prefix and suffix included.
    */
    void textChanged(const QString &text) { emitSignal(&QDoubleSpinBox::textChanged, text); }

protected:
    StepEnabled stepEnabled() const override;

private:
    QString valueText() const override;
    QStringList sizeTexts() const override;
    void takeText(const QString &number, bool atMinimum, bool finished) override;
    bool acceptsCharacter(QChar c) const override;
    double round(double value) const;
    void changeValue(double value, bool rewrite);

    double m_minimum = 0.0;
    double m_maximum = 99.99;
    double m_value = 0.0;
    double m_step = 1.0;
    int m_decimals = 2;
};
