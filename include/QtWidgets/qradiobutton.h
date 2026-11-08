// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qabstractbutton.h>

class QRadioButton : public QAbstractButton {
    Q_OBJECT
public:
    explicit QRadioButton(QWidget *parent = nullptr);
    explicit QRadioButton(const QString &text, QWidget *parent = nullptr);
    ~QRadioButton() override;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool hitButton(const QPoint &pos) const override;
    void keyPressEvent(QKeyEvent *event) override;
};
