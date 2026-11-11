// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qplaintextedit.h>

#include <memory>

class QTextDocument;

class QTextEdit : public QPlainTextEdit {
    Q_OBJECT
    Q_PROPERTY(bool acceptRichText READ acceptRichText WRITE setAcceptRichText)
    Q_PROPERTY(QString html READ toHtml WRITE setHtml)
public:
    explicit QTextEdit(QWidget *parent = nullptr);
    explicit QTextEdit(const QString &text, QWidget *parent = nullptr);
    ~QTextEdit() override;

    bool acceptRichText() const { return m_acceptRichText; }
    void setAcceptRichText(bool accept) { m_acceptRichText = accept; }
    QString toHtml() const;
    QTextDocument *document() const;
    QString documentTitle() const;
    void setDocumentTitle(const QString &title);
    QString anchorAt(const QPoint &pos) const;

public slots:
    void setText(const QString &text);
    void setHtml(const QString &text);
    void append(const QString &text);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    friend class QTextBrowser;
    bool showsRichText() const { return m_rich; }
    void adoptDocument();
    void layoutRichText();

    bool m_acceptRichText = true;
    std::unique_ptr<QTextDocument> m_document;
    bool m_rich = false;          // the document is shown, with its formats, and not the plain text
    mutable bool m_stale = false; // the document does not have the plain text that is edited
    bool m_adopting = false;
    bool m_layingRich = false;
};
