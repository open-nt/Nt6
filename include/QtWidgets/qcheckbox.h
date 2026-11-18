// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qabstractbutton.h>

class QStyleOptionButton;

class QCheckBox : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(bool tristate READ isTristate WRITE setTristate)
public:
    explicit QCheckBox(QWidget *parent = nullptr);
    explicit QCheckBox(const QString &text, QWidget *parent = nullptr);
    ~QCheckBox() override;

    QSize sizeHint() const override;
    bool isTristate() const { return m_tristate; }
    void setTristate(bool y = true) { m_tristate = y; }
    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState state);

signals:
    /*!
        \since 6.0
        This signal is emitted when the check state changes to \a state.
    */
    void checkStateChanged(Qt::CheckState state) { emitSignal(&QCheckBox::checkStateChanged, state); }
    /*!
        \since 6.0
        This signal is emitted when the check state changes to \a state, a Qt::CheckState.
    */
    void stateChanged(int state) { emitSignal(&QCheckBox::stateChanged, state); }

protected:
    void paintEvent(QPaintEvent *event) override;
    bool hitButton(const QPoint &pos) const override;
    void checkStateSet() override;
    void nextCheckState() override;
    void initStyleOption(QStyleOptionButton *option) const;

private:
    void publish();
    bool m_tristate = false;
    bool m_partial = false;
    bool m_blockRefresh = false;
    Qt::CheckState m_published = Qt::Unchecked;
};
