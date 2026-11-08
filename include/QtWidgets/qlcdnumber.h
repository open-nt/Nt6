// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qframe.h>

#include <vector>

class QLCDNumber : public QFrame {
    Q_OBJECT
    Q_PROPERTY(bool smallDecimalPoint READ smallDecimalPoint WRITE setSmallDecimalPoint)
    Q_PROPERTY(int digitCount READ digitCount WRITE setDigitCount)
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
    Q_PROPERTY(SegmentStyle segmentStyle READ segmentStyle WRITE setSegmentStyle)
    Q_PROPERTY(double value READ value WRITE display)
    Q_PROPERTY(int intValue READ intValue WRITE display)
public:
    enum Mode { Hex, Dec, Oct, Bin };
    enum SegmentStyle { Outline, Filled, Flat };
    Q_ENUM(Mode)
    Q_ENUM(SegmentStyle)

    explicit QLCDNumber(QWidget *parent = nullptr);
    explicit QLCDNumber(uint numDigits, QWidget *parent = nullptr);
    ~QLCDNumber() override;

    bool smallDecimalPoint() const { return m_smallPoint; }
    void setSmallDecimalPoint(bool on);
    int digitCount() const { return m_digits; }
    void setDigitCount(int numDigits);
    Mode mode() const { return m_mode; }
    void setMode(Mode mode);
    SegmentStyle segmentStyle() const { return m_style; }
    void setSegmentStyle(SegmentStyle style);
    double value() const { return m_value; }
    int intValue() const;
    bool checkOverflow(double num) const;
    bool checkOverflow(int num) const;

    QSize sizeHint() const override;

public slots:
    void display(const QString &str);
    void display(int num);
    void display(double num);
    void setHexMode() { setMode(Hex); }
    void setDecMode() { setMode(Dec); }
    void setOctMode() { setMode(Oct); }
    void setBinMode() { setMode(Bin); }

signals:
    /*!
        This signal is emitted when what is displayed has more digits than the display has.
    */
    void overflow() { emitSignal(&QLCDNumber::overflow); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Digit {
        char character;
        bool point;
    };
    QString format(int num) const;
    QString format(double num) const;
    std::vector<Digit> digitsOf(const QString &text) const;
    void show(const QString &text);

    QString m_text;
    double m_value = 0;
    int m_digits = 5;
    Mode m_mode = Dec;
    SegmentStyle m_style = Filled;
    bool m_smallPoint = false;
    bool m_overflowed = false;
};
