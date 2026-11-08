// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qabstractbutton.h>

class QPushButton : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(bool flat READ isFlat WRITE setFlat)
    Q_PROPERTY(bool autoDefault READ autoDefault WRITE setAutoDefault)
    Q_PROPERTY(bool default READ isDefault WRITE setDefault)
public:
    explicit QPushButton(QWidget *parent = nullptr);
    explicit QPushButton(const QString &text, QWidget *parent = nullptr);
    ~QPushButton() override;

    bool isFlat() const { return m_flat; }
    void setFlat(bool flat);
    bool autoDefault() const;
    void setAutoDefault(bool enable);
    bool isDefault() const { return m_default; }
    void setDefault(bool enable);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    enum class AutoDefault { Auto, Off, On };

    bool m_flat = false;
    bool m_default = false;
    AutoDefault m_autoDefault = AutoDefault::Auto;
};
