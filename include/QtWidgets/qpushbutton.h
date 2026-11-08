// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qabstractbutton.h>

class QPushButton : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(bool flat READ isFlat WRITE setFlat)
public:
    explicit QPushButton(QWidget *parent = nullptr);
    explicit QPushButton(const QString &text, QWidget *parent = nullptr);
    ~QPushButton() override;

    bool isFlat() const { return m_flat; }
    void setFlat(bool flat);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    bool m_flat = false;
};
