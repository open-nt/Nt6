// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qpushbutton.h>

class QCommandLinkButton : public QPushButton {
    Q_OBJECT
    Q_PROPERTY(QString description READ description WRITE setDescription)
public:
    explicit QCommandLinkButton(QWidget *parent = nullptr);
    explicit QCommandLinkButton(const QString &text, QWidget *parent = nullptr);
    QCommandLinkButton(const QString &text, const QString &description, QWidget *parent = nullptr);
    ~QCommandLinkButton() override;

    QString description() const { return m_description; }
    void setDescription(const QString &description);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QFont titleFont() const;
    int textWidth() const;

    QString m_description;
};
