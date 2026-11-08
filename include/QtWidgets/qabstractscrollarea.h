// MIT License - see LICENSE
#pragma once

#include <QtCore/qmargins.h>
#include <QtWidgets/qframe.h>

class QScrollBar;

namespace QtPrivate {
class ScrollAreaViewport;
}

class QAbstractScrollArea : public QFrame {
    Q_OBJECT
public:
    explicit QAbstractScrollArea(QWidget *parent = nullptr);
    ~QAbstractScrollArea() override;

    QWidget *viewport() const { return m_viewport; }
    QScrollBar *horizontalScrollBar() const { return m_hbar; }
    QScrollBar *verticalScrollBar() const { return m_vbar; }
    Qt::ScrollBarPolicy horizontalScrollBarPolicy() const { return m_hPolicy; }
    void setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy policy);
    Qt::ScrollBarPolicy verticalScrollBarPolicy() const { return m_vPolicy; }
    void setVerticalScrollBarPolicy(Qt::ScrollBarPolicy policy);
    QMargins viewportMargins() const { return m_margins; }
    void setViewportMargins(int left, int top, int right, int bottom);
    void setViewportMargins(const QMargins &margins) {
        setViewportMargins(margins.left(), margins.top(), margins.right(), margins.bottom());
    }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    bool event(QEvent *event) override;
    virtual bool viewportEvent(QEvent *event);
    virtual QSize viewportSizeHint() const;
    virtual void scrollContentsBy(int dx, int dy);
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    friend class QtPrivate::ScrollAreaViewport;
    void layoutChildren();

    QWidget *m_viewport = nullptr;
    QScrollBar *m_hbar = nullptr;
    QScrollBar *m_vbar = nullptr;
    Qt::ScrollBarPolicy m_hPolicy = Qt::ScrollBarAsNeeded;
    Qt::ScrollBarPolicy m_vPolicy = Qt::ScrollBarAsNeeded;
    QMargins m_margins;
    int m_hLast = 0;
    int m_vLast = 0;
    bool m_layouting = false;
    bool m_layoutAgain = false;
};
