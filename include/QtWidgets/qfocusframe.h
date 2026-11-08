// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtWidgets/qwidget.h>

class QFocusFrame : public QWidget {
    Q_OBJECT
public:
    explicit QFocusFrame(QWidget *parent = nullptr);
    ~QFocusFrame() override;

    void setWidget(QWidget *widget);
    QWidget *widget() const { return m_widget.data(); }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void follow();

    QPointer<QWidget> m_widget;
};
