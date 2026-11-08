// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qwidget.h>

class QDial : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(int singleStep READ singleStep WRITE setSingleStep)
    Q_PROPERTY(int pageStep READ pageStep WRITE setPageStep)
    Q_PROPERTY(bool tracking READ hasTracking WRITE setTracking)
    Q_PROPERTY(bool sliderDown READ isSliderDown)
    Q_PROPERTY(bool wrapping READ wrapping WRITE setWrapping)
    Q_PROPERTY(bool notchesVisible READ notchesVisible WRITE setNotchesVisible)
public:
    explicit QDial(QWidget *parent = nullptr);
    ~QDial() override;

    int minimum() const { return m_minimum; }
    void setMinimum(int minimum);
    int maximum() const { return m_maximum; }
    void setMaximum(int maximum);
    void setRange(int minimum, int maximum);
    int value() const { return m_value; }
    int sliderPosition() const { return m_position; }
    int singleStep() const { return m_singleStep; }
    void setSingleStep(int step);
    int pageStep() const { return m_pageStep; }
    void setPageStep(int step);
    bool hasTracking() const { return m_tracking; }
    void setTracking(bool enable) { m_tracking = enable; }
    bool isSliderDown() const { return m_down; }
    bool wrapping() const { return m_wrapping; }
    void setWrapping(bool on);
    bool notchesVisible() const { return m_notches; }
    void setNotchesVisible(bool visible);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setValue(int value);

signals:
    /*!
        This signal is emitted when the value changes to \a value, whether the user turned the dial or setValue() did.
        While the dial is turned it is emitted at every move if tracking is on, otherwise only when it is released.
    */
    void valueChanged(int value) { emitSignal(&QDial::valueChanged, value); }
    /*!
        This signal is emitted when the user presses the dial.
    */
    void sliderPressed() { emitSignal(&QDial::sliderPressed); }
    /*!
        This signal is emitted when the user releases the dial.
    */
    void sliderReleased() { emitSignal(&QDial::sliderReleased); }
    /*!
        This signal is emitted when the user turns the dial to \a position.
    */
    void sliderMoved(int position) { emitSignal(&QDial::sliderMoved, position); }
    /*!
        This signal is emitted when the range changes to \a min and \a max.
    */
    void rangeChanged(int min, int max) { emitSignal(&QDial::rangeChanged, min, max); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    int valueAt(const QPoint &pos) const;
    void setPosition(int position);
    void step(int delta);

    int m_minimum = 0;
    int m_maximum = 99;
    int m_value = 0;
    int m_position = 0;
    int m_singleStep = 1;
    int m_pageStep = 10;
    bool m_tracking = true;
    bool m_down = false;
    bool m_wrapping = false;
    bool m_notches = false;
};
