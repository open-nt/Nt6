// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtGui/qpixmap.h>
#include <QtWidgets/qwidget.h>

#include <memory>

class QTextDocument;

class QLabel : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(bool wordWrap READ wordWrap WRITE setWordWrap)
    Q_PROPERTY(int margin READ margin WRITE setMargin)
    Q_PROPERTY(bool scaledContents READ hasScaledContents WRITE setScaledContents)
    Q_PROPERTY(Qt::TextFormat textFormat READ textFormat WRITE setTextFormat)
    Q_PROPERTY(bool openExternalLinks READ openExternalLinks WRITE setOpenExternalLinks)
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
    QPixmap pixmap() const { return m_pixmap; }
    bool hasScaledContents() const { return m_scaled; }
    void setScaledContents(bool scaled);
    Qt::TextFormat textFormat() const { return m_format; }
    void setTextFormat(Qt::TextFormat format);
    bool openExternalLinks() const { return m_openExternal; }
    void setOpenExternalLinks(bool open) { m_openExternal = open; }
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setText(const QString &text);
    void setNum(int number);
    void setPixmap(const QPixmap &pixmap);
    void clear();

signals:
    /*!
        This signal is emitted when the user clicks a link of the rich text of the label. \a link is the
        target of the link, the \c href of its \c a element.

        \sa linkHovered()
    */
    void linkActivated(const QString &link) { emitSignal(&QLabel::linkActivated, link); }
    /*!
        This signal is emitted when the mouse comes over a link of the rich text of the label, with the
        target of the link as \a link, and when it leaves the link, with an empty string.

        \sa linkActivated()
    */
    void linkHovered(const QString &link) { emitSignal(&QLabel::linkHovered, link); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QSize textSize(int width) const;
    void updateDocument();
    QRect documentRect() const;
    QString linkAt(const QPoint &pos) const;
    void setHovered(const QString &link);

    QString m_text;
    Qt::Alignment m_alignment = Qt::AlignLeft | Qt::AlignVCenter;
    bool m_wordWrap = false;
    int m_margin = 0;
    QPixmap m_pixmap;
    bool m_scaled = false;
    Qt::TextFormat m_format = Qt::AutoText;
    bool m_openExternal = false;
    std::unique_ptr<QTextDocument> m_document; // of a text that is rich text
    QString m_hovered;
    QString m_pressed;
};
