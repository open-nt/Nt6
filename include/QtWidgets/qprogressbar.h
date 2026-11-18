// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qwidget.h>

class QStyleOptionProgressBar;

class QProgressBar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(bool textVisible READ isTextVisible WRITE setTextVisible)
    Q_PROPERTY(QString format READ format WRITE setFormat RESET resetFormat)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment)
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation)
    Q_PROPERTY(bool invertedAppearance READ invertedAppearance WRITE setInvertedAppearance)
    Q_PROPERTY(Direction textDirection READ textDirection WRITE setTextDirection)
public:
    enum Direction { TopToBottom, BottomToTop };
    Q_ENUM(Direction)

    explicit QProgressBar(QWidget *parent = nullptr);
    ~QProgressBar() override;

    int minimum() const { return m_minimum; }
    void setMinimum(int minimum);
    int maximum() const { return m_maximum; }
    void setMaximum(int maximum);
    void setRange(int minimum, int maximum);
    int value() const { return m_value; }
    bool isTextVisible() const { return m_textVisible; }
    void setTextVisible(bool visible);
    QString format() const { return m_format; }
    void setFormat(const QString &format);
    void resetFormat();
    virtual QString text() const;
    Qt::Alignment alignment() const { return m_alignment; }
    void setAlignment(Qt::Alignment alignment);
    Qt::Orientation orientation() const { return m_orientation; }
    bool invertedAppearance() const { return m_inverted; }
    void setInvertedAppearance(bool invert);
    Direction textDirection() const { return m_direction; }
    void setTextDirection(Direction textDirection);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setValue(int value);
    void reset();
    void setOrientation(Qt::Orientation orientation);

signals:
    /*!
        This signal is emitted when the value changes to \a value.
    */
    void valueChanged(int value) { emitSignal(&QProgressBar::valueChanged, value); }

protected:
    void initStyleOption(QStyleOptionProgressBar *option) const;
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    int m_minimum = 0;
    int m_maximum = 100;
    int m_value = -1;
    bool m_textVisible = true;
    QString m_format = QString::fromLatin1("%p%");
    Qt::Alignment m_alignment = Qt::AlignLeft;
    Qt::Orientation m_orientation = Qt::Horizontal;
    bool m_inverted = false;
    Direction m_direction = TopToBottom;
};
