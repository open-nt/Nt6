// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qabstractslider.h>

class QDial : public QAbstractSlider {
    Q_OBJECT
    Q_PROPERTY(bool wrapping READ wrapping WRITE setWrapping)
    Q_PROPERTY(bool notchesVisible READ notchesVisible WRITE setNotchesVisible)
public:
    explicit QDial(QWidget *parent = nullptr);
    ~QDial() override;

    bool wrapping() const { return m_wrapSteps; }
    bool notchesVisible() const { return m_notches; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setWrapping(bool on);
    void setNotchesVisible(bool visible);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    int valueAt(const QPoint &pos) const;

    bool m_notches = false;
};
