// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtWidgets/qabstractscrollarea.h>

class QScrollArea : public QAbstractScrollArea {
    Q_OBJECT
    Q_PROPERTY(bool widgetResizable READ widgetResizable WRITE setWidgetResizable)
public:
    explicit QScrollArea(QWidget *parent = nullptr);
    ~QScrollArea() override;

    QWidget *widget() const { return m_widget.data(); }
    void setWidget(QWidget *widget);
    QWidget *takeWidget();
    bool widgetResizable() const { return m_resizable; }
    void setWidgetResizable(bool resizable);

public slots:
    void ensureVisible(int x, int y, int xmargin = 50, int ymargin = 50);
    void ensureWidgetVisible(QWidget *child, int xmargin = 50, int ymargin = 50);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    QSize viewportSizeHint() const override;

private:
    void updateScrollBars();

    QPointer<QWidget> m_widget;
    bool m_resizable = false;
    bool m_updating = false;
    bool m_updateAgain = false;
};
