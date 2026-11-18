// MIT License - see LICENSE
#pragma once

#include <vector>

#include <QtCore/qpointer.h>

#include <QtCore/qmargins.h>
#include <QtWidgets/qframe.h>

class QScrollBar;

namespace QtPrivate {
class ScrollAreaViewport;
}

class QAbstractScrollArea : public QFrame {
    Q_OBJECT
    Q_PROPERTY(Qt::ScrollBarPolicy verticalScrollBarPolicy READ verticalScrollBarPolicy WRITE setVerticalScrollBarPolicy)
    Q_PROPERTY(
        Qt::ScrollBarPolicy horizontalScrollBarPolicy READ horizontalScrollBarPolicy WRITE setHorizontalScrollBarPolicy)
    Q_PROPERTY(SizeAdjustPolicy sizeAdjustPolicy READ sizeAdjustPolicy WRITE setSizeAdjustPolicy)
public:
    enum SizeAdjustPolicy { AdjustIgnored, AdjustToContentsOnFirstShow, AdjustToContents };
    Q_ENUM(SizeAdjustPolicy)

    explicit QAbstractScrollArea(QWidget *parent = nullptr);
    ~QAbstractScrollArea() override;

    QWidget *viewport() const { return m_viewport; }
    void setViewport(QWidget *widget);
    void setHorizontalScrollBar(QScrollBar *scrollBar);
    void setVerticalScrollBar(QScrollBar *scrollBar);
    QWidget *cornerWidget() const { return m_corner.data(); }
    void setCornerWidget(QWidget *widget);
    void addScrollBarWidget(QWidget *widget, Qt::Alignment alignment);
    QWidgetList scrollBarWidgets(Qt::Alignment alignment);
    QSize maximumViewportSize() const;
    SizeAdjustPolicy sizeAdjustPolicy() const { return m_sizeAdjust; }
    void setSizeAdjustPolicy(SizeAdjustPolicy policy);
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

protected slots:
    virtual void setupViewport(QWidget *viewport);

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
    void connectBar(QScrollBar *bar);
    struct BarWidget {
        QPointer<QWidget> widget;
        Qt::Alignment alignment;
    };
    std::vector<BarWidget> m_barWidgets;
    QPointer<QWidget> m_corner;
    SizeAdjustPolicy m_sizeAdjust = AdjustIgnored;
    QObject *m_viewportFilter = nullptr; // forwards the events of a viewport set with setViewport()

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
