// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qwidget.h>

class QGroupBox : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(bool checkable READ isCheckable WRITE setCheckable)
    Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY toggled)
public:
    explicit QGroupBox(QWidget *parent = nullptr);
    explicit QGroupBox(const QString &title, QWidget *parent = nullptr);
    ~QGroupBox() override;

    QString title() const { return m_title; }
    void setTitle(const QString &title);
    bool isCheckable() const { return m_checkable; }
    void setCheckable(bool checkable);
    bool isChecked() const { return m_checked; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setChecked(bool checked);

signals:
    /*!
        This signal is emitted when the state of a checkable group box changes to \a on.
    */
    void toggled(bool on) { emitSignal(&QGroupBox::toggled, on); }
    /*!
        This signal is emitted when the user clicks the check box in the title of the group box, and \a checked is its
        state afterwards.
    */
    void clicked(bool checked = false) { emitSignal(&QGroupBox::clicked, checked); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    QRect titleRect() const;
    void applyChecked();

    QString m_title;
    bool m_checkable = false;
    bool m_checked = true;
    bool m_down = false;
};
