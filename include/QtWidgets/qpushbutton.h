// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtWidgets/qabstractbutton.h>

class QMenu;
class QStyleOptionButton;

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
    QMenu *menu() const;
    void setMenu(QMenu *menu);
    QSize sizeHint() const override;

public slots:
    void showMenu();

protected:
    void initStyleOption(QStyleOptionButton *option) const;
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    enum class AutoDefault { Auto, Off, On };

    bool m_flat = false;
    bool m_default = false;
    AutoDefault m_autoDefault = AutoDefault::Auto;
    QPointer<QWidget> m_menu; // a QMenu
    QMetaObject::Connection m_menuConnection;
};
