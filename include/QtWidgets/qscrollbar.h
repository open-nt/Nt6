// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qwidget.h>

class QScrollBar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(int singleStep READ singleStep WRITE setSingleStep)
    Q_PROPERTY(int pageStep READ pageStep WRITE setPageStep)
    Q_PROPERTY(bool sliderDown READ isSliderDown)
public:
    explicit QScrollBar(QWidget *parent = nullptr);
    explicit QScrollBar(Qt::Orientation orientation, QWidget *parent = nullptr);
    ~QScrollBar() override;

    Qt::Orientation orientation() const { return m_orientation; }
    void setOrientation(Qt::Orientation orientation);
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
    bool isSliderDown() const { return m_down; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setValue(int value);

signals:
    /*!
        This signal is emitted when the value changes to \a value, whether the user moved the handle or setValue() did.
    */
    void valueChanged(int value) { emitSignal(&QScrollBar::valueChanged, value); }
    /*!
        This signal is emitted when the user presses the handle.
    */
    void sliderPressed() { emitSignal(&QScrollBar::sliderPressed); }
    /*!
        This signal is emitted when the user releases the handle.
    */
    void sliderReleased() { emitSignal(&QScrollBar::sliderReleased); }
    /*!
        This signal is emitted when the user drags the handle to \a position.
    */
    void sliderMoved(int position) { emitSignal(&QScrollBar::sliderMoved, position); }
    /*!
        This signal is emitted when the range changes to \a min and \a max.
    */
    void rangeChanged(int min, int max) { emitSignal(&QScrollBar::rangeChanged, min, max); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    QRect handleRect() const;
    int valueAt(const QPoint &pos) const;

    Qt::Orientation m_orientation = Qt::Vertical;
    int m_minimum = 0;
    int m_maximum = 99;
    int m_value = 0;
    int m_position = 0;
    int m_singleStep = 1;
    int m_pageStep = 10;
    int m_grabOffset = 0;
    bool m_down = false;
};
