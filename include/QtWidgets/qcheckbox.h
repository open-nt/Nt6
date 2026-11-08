// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qabstractbutton.h>

class QCheckBox : public QAbstractButton {
    Q_OBJECT
public:
    explicit QCheckBox(QWidget *parent = nullptr);
    explicit QCheckBox(const QString &text, QWidget *parent = nullptr);
    ~QCheckBox() override;

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool hitButton(const QPoint &pos) const override;
};
