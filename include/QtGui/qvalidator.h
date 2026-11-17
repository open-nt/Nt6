// MIT License - see LICENSE
#pragma once

#include <QtCore/qlocale.h>
#include <QtCore/qobject.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qstring.h>

class QValidator : public QObject {
    Q_OBJECT
public:
    enum State { Invalid, Intermediate, Acceptable };
    Q_ENUM(State)

    explicit QValidator(QObject *parent = nullptr);
    ~QValidator() override;

    void setLocale(const QLocale &locale);
    QLocale locale() const;

    virtual State validate(QString &input, int &pos) const = 0;
    virtual void fixup(QString &input) const;

signals:
    /*!
        This signal is emitted when the rules of the validator change, so that text may become valid or invalid.
    */
    void changed() { emitSignal(&QValidator::changed); }

private:
    Q_DISABLE_COPY(QValidator)
    QLocale m_locale;
};

class QIntValidator : public QValidator {
    Q_OBJECT
    Q_PROPERTY(int bottom READ bottom WRITE setBottom NOTIFY bottomChanged)
    Q_PROPERTY(int top READ top WRITE setTop NOTIFY topChanged)
public:
    explicit QIntValidator(QObject *parent = nullptr);
    QIntValidator(int minimum, int maximum, QObject *parent = nullptr);
    ~QIntValidator() override;

    QValidator::State validate(QString &input, int &pos) const override;
    void fixup(QString &input) const override;

    void setBottom(int bottom);
    void setTop(int top);
    void setRange(int bottom, int top);
    int bottom() const { return m_bottom; }
    int top() const { return m_top; }

signals:
    /*!
        This signal is emitted when the bottom of the range changes to \a bottom.
    */
    void bottomChanged(int bottom) { emitSignal(&QIntValidator::bottomChanged, bottom); }
    /*!
        This signal is emitted when the top of the range changes to \a top.
    */
    void topChanged(int top) { emitSignal(&QIntValidator::topChanged, top); }

private:
    int m_bottom;
    int m_top;
};

class QDoubleValidator : public QValidator {
    Q_OBJECT
    Q_PROPERTY(double bottom READ bottom WRITE setBottom NOTIFY bottomChanged)
    Q_PROPERTY(double top READ top WRITE setTop NOTIFY topChanged)
    Q_PROPERTY(int decimals READ decimals WRITE setDecimals NOTIFY decimalsChanged)
    Q_PROPERTY(Notation notation READ notation WRITE setNotation NOTIFY notationChanged)
public:
    enum Notation { StandardNotation, ScientificNotation };
    Q_ENUM(Notation)

    explicit QDoubleValidator(QObject *parent = nullptr);
    QDoubleValidator(double bottom, double top, int decimals, QObject *parent = nullptr);
    ~QDoubleValidator() override;

    QValidator::State validate(QString &input, int &pos) const override;
    void fixup(QString &input) const override;

    void setRange(double bottom, double top, int decimals);
    void setRange(double bottom, double top);
    void setBottom(double bottom);
    void setTop(double top);
    void setDecimals(int decimals);
    void setNotation(Notation notation);

    double bottom() const { return m_bottom; }
    double top() const { return m_top; }
    int decimals() const { return m_decimals; }
    Notation notation() const { return m_notation; }

signals:
    /*!
        This signal is emitted when the bottom of the range changes to \a bottom.
    */
    void bottomChanged(double bottom) { emitSignal(&QDoubleValidator::bottomChanged, bottom); }
    /*!
        This signal is emitted when the top of the range changes to \a top.
    */
    void topChanged(double top) { emitSignal(&QDoubleValidator::topChanged, top); }
    /*!
        This signal is emitted when the number of decimals changes to \a decimals.
    */
    void decimalsChanged(int decimals) { emitSignal(&QDoubleValidator::decimalsChanged, decimals); }
    /*!
        This signal is emitted when the notation changes to \a notation.
    */
    void notationChanged(QDoubleValidator::Notation notation) {
        emitSignal(&QDoubleValidator::notationChanged, notation);
    }

private:
    double m_bottom;
    double m_top;
    int m_decimals = -1;
    Notation m_notation = ScientificNotation;
};

class QRegularExpressionValidator : public QValidator {
    Q_OBJECT
    Q_PROPERTY(QRegularExpression regularExpression READ regularExpression WRITE setRegularExpression NOTIFY
                   regularExpressionChanged)
public:
    explicit QRegularExpressionValidator(QObject *parent = nullptr);
    explicit QRegularExpressionValidator(const QRegularExpression &re, QObject *parent = nullptr);
    ~QRegularExpressionValidator() override;

    QValidator::State validate(QString &input, int &pos) const override;

    QRegularExpression regularExpression() const { return m_re; }

public slots:
    void setRegularExpression(const QRegularExpression &re);

signals:
    /*!
        This signal is emitted when the regular expression changes to \a re.
    */
    void regularExpressionChanged(const QRegularExpression &re) {
        emitSignal(&QRegularExpressionValidator::regularExpressionChanged, re);
    }

private:
    QRegularExpression m_re;
};
