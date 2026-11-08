// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtWidgets/qwidget.h>

class QLabel : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(bool wordWrap READ wordWrap WRITE setWordWrap)
    Q_PROPERTY(int margin READ margin WRITE setMargin)
public:
    explicit QLabel(QWidget *parent = nullptr);
    explicit QLabel(const QString &text, QWidget *parent = nullptr);
    ~QLabel() override;

    QString text() const { return m_text; }
    Qt::Alignment alignment() const { return m_alignment; }
    void setAlignment(Qt::Alignment alignment);
    bool wordWrap() const { return m_wordWrap; }
    void setWordWrap(bool on);
    int margin() const { return m_margin; }
    void setMargin(int margin);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setText(const QString &text);
    void setNum(int number);
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    QSize textSize(int width) const;

    QString m_text;
    Qt::Alignment m_alignment = Qt::AlignLeft | Qt::AlignVCenter;
    bool m_wordWrap = false;
    int m_margin = 0;
};
