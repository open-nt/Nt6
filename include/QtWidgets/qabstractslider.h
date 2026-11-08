// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qwidget.h>

class QAbstractSlider : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(int singleStep READ singleStep WRITE setSingleStep)
    Q_PROPERTY(int pageStep READ pageStep WRITE setPageStep)
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(int sliderPosition READ sliderPosition WRITE setSliderPosition NOTIFY sliderMoved)
    Q_PROPERTY(bool tracking READ hasTracking WRITE setTracking)
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation)
    Q_PROPERTY(bool invertedControls READ invertedControls WRITE setInvertedControls)
    Q_PROPERTY(bool sliderDown READ isSliderDown WRITE setSliderDown)
public:
    enum SliderAction {
        SliderNoAction,
        SliderSingleStepAdd,
        SliderSingleStepSub,
        SliderPageStepAdd,
        SliderPageStepSub,
        SliderToMinimum,
        SliderToMaximum,
        SliderMove
    };
    Q_ENUM(SliderAction)

    explicit QAbstractSlider(QWidget *parent = nullptr);
    ~QAbstractSlider() override;

    Qt::Orientation orientation() const { return m_orientation; }
    int minimum() const { return m_minimum; }
    void setMinimum(int minimum);
    int maximum() const { return m_maximum; }
    void setMaximum(int maximum);
    void setRange(int minimum, int maximum);
    int singleStep() const { return m_singleStep; }
    void setSingleStep(int step);
    int pageStep() const { return m_pageStep; }
    void setPageStep(int step);
    bool hasTracking() const { return m_tracking; }
    void setTracking(bool enable) { m_tracking = enable; }
    bool isSliderDown() const { return m_down; }
    void setSliderDown(bool down);
    int sliderPosition() const { return m_position; }
    void setSliderPosition(int position);
    bool invertedControls() const { return m_invertedControls; }
    void setInvertedControls(bool invert) { m_invertedControls = invert; }
    int value() const { return m_value; }
    void triggerAction(SliderAction action);

public slots:
    void setValue(int value);
    void setOrientation(Qt::Orientation orientation);

signals:
    /*!
        This signal is emitted when the value changes to \a value, whether the user moved the slider or setValue()
        did. While the slider is dragged it is emitted at every move if tracking is on, otherwise only when the slider
        is released.
    */
    void valueChanged(int value) { emitSignal(&QAbstractSlider::valueChanged, value); }
    /*!
        This signal is emitted when the user presses the slider, and when setSliderDown() sets it down.
    */
    void sliderPressed() { emitSignal(&QAbstractSlider::sliderPressed); }
    /*!
        This signal is emitted when the slider is down and its position changes to \a value: the user drags it.
    */
    void sliderMoved(int value) { emitSignal(&QAbstractSlider::sliderMoved, value); }
    /*!
        This signal is emitted when the user releases the slider, and when setSliderDown() lets it up.
    */
    void sliderReleased() { emitSignal(&QAbstractSlider::sliderReleased); }
    /*!
        This signal is emitted when the range changes to \a min and \a max.
    */
    void rangeChanged(int min, int max) { emitSignal(&QAbstractSlider::rangeChanged, min, max); }
    /*!
        This signal is emitted when triggerAction() runs \a action, a SliderAction. The slider position has already
        moved; the value has not changed yet.
    */
    void actionTriggered(int action) { emitSignal(&QAbstractSlider::actionTriggered, action); }

protected:
    enum SliderChange { SliderRangeChange, SliderOrientationChange, SliderStepsChange, SliderValueChange };

    virtual void sliderChange(SliderChange change);
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    friend class QDial;
    int bound(int value) const;
    int stepped(qint64 delta) const;

    Qt::Orientation m_orientation = Qt::Horizontal;
    int m_minimum = 0;
    int m_maximum = 99;
    int m_value = 0;
    int m_position = 0;
    int m_singleStep = 1;
    int m_pageStep = 10;
    bool m_tracking = true;
    bool m_blockTracking = false;
    bool m_down = false;
    bool m_invertedControls = false;
    bool m_wrapSteps = false; // a dial that wraps goes round the ends when it steps
};
