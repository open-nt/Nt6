// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtWidgets/qabstractscrollarea.h>

class QScrollArea : public QAbstractScrollArea {
    Q_OBJECT
    Q_PROPERTY(bool widgetResizable READ widgetResizable WRITE setWidgetResizable)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment)
public:
    explicit QScrollArea(QWidget *parent = nullptr);
    ~QScrollArea() override;

    QWidget *widget() const { return m_widget.data(); }
    void setWidget(QWidget *widget);
    QWidget *takeWidget();
    bool widgetResizable() const { return m_resizable; }
    void setWidgetResizable(bool resizable);
    Qt::Alignment alignment() const { return m_alignment; }
    void setAlignment(Qt::Alignment alignment);

public slots:
    void ensureVisible(int x, int y, int xmargin = 50, int ymargin = 50);
    void ensureWidgetVisible(QWidget *child, int xmargin = 50, int ymargin = 50);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    QSize viewportSizeHint() const override;

private:
    void updateScrollBars();
    void placeWidget();

    QPointer<QWidget> m_widget;
    bool m_resizable = false;
    Qt::Alignment m_alignment = Qt::AlignLeft | Qt::AlignTop;
    bool m_updating = false;
    bool m_updateAgain = false;
};
