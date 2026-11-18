// MIT License - see LICENSE
#pragma once

#include <QtGui/qtextdocument.h>
#include <QtWidgets/qplaintextedit.h>

#include <memory>

class QTextEdit : public QPlainTextEdit {
    Q_OBJECT
    Q_PROPERTY(bool acceptRichText READ acceptRichText WRITE setAcceptRichText)
    Q_PROPERTY(QString html READ toHtml WRITE setHtml USER true)
    Q_PROPERTY(QString markdown READ toMarkdown WRITE setMarkdown)
    Q_PROPERTY(AutoFormatting autoFormatting READ autoFormatting WRITE setAutoFormatting)
public:
    enum AutoFormattingFlag : unsigned { AutoNone = 0, AutoBulletList = 0x00000001, AutoAll = 0xffffffff };
    Q_DECLARE_FLAGS(AutoFormatting, AutoFormattingFlag)
    Q_FLAG(AutoFormatting)

    explicit QTextEdit(QWidget *parent = nullptr);
    explicit QTextEdit(const QString &text, QWidget *parent = nullptr);
    ~QTextEdit() override;

    bool acceptRichText() const { return m_acceptRichText; }
    void setAcceptRichText(bool accept) { m_acceptRichText = accept; }
    QString toHtml() const;
    QString toMarkdown(QTextDocument::MarkdownFeatures features = QTextDocument::MarkdownDialectGitHub) const;
    QTextDocument *document() const;
    QString documentTitle() const;
    void setDocumentTitle(const QString &title);
    QString anchorAt(const QPoint &pos) const;
    AutoFormatting autoFormatting() const { return m_autoFormatting; }
    void setAutoFormatting(AutoFormatting features) { m_autoFormatting = features; }
    Qt::Alignment alignment() const { return m_alignment; }
    QFont currentFont() const;
    QString fontFamily() const;
    qreal fontPointSize() const;
    int fontWeight() const;
    bool fontItalic() const;
    bool fontUnderline() const;
    QColor textColor() const;
    QColor textBackgroundColor() const;

public slots:
    void setText(const QString &text);
    void setHtml(const QString &text);
    void setMarkdown(const QString &markdown);
    void insertHtml(const QString &text);
    void append(const QString &text);
    void setAlignment(Qt::Alignment a);
    void setCurrentFont(const QFont &f);
    void setFontFamily(const QString &fontFamily);
    void setFontPointSize(qreal s);
    void setFontWeight(int weight);
    void setFontItalic(bool italic);
    void setFontUnderline(bool underline);
    void setTextColor(const QColor &c);
    void setTextBackgroundColor(const QColor &c);

signals:
    /*!
        \since 6.0
        This signal is emitted when the current character format changes to \a f.
    */
    void currentCharFormatChanged(const QTextCharFormat &f) { emitSignal(&QTextEdit::currentCharFormatChanged, f); }

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
    friend class QTextCursor;
    bool showsRichText() const { return m_rich; }
    void adoptDocument();
    void layoutRichText();
    void mergeFormat(const QTextCharFormat &modifier);

    bool m_acceptRichText = true;
    std::unique_ptr<QTextDocument> m_document;
    bool m_rich = false;          // the document is shown, with its formats, and not the plain text
    mutable bool m_stale = false; // the document does not have the plain text that is edited
    bool m_adopting = false;
    bool m_layingRich = false;
    AutoFormatting m_autoFormatting = AutoNone;
    Qt::Alignment m_alignment = Qt::AlignLeft;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QTextEdit::AutoFormatting)
