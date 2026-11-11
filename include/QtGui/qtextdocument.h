// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtGui/qfont.h>
#include <QtGui/qtextformat.h>
#include <QtGui/qtextlayout.h>
#include <QtGui/qtextoption.h>

#include <memory>

class QPainter;
class QTextDocument;
class QTextDocumentPrivate;

namespace Qt {
bool mightBeRichText(const QString &text);
QString convertFromPlainText(const QString &plain, WhiteSpaceMode mode = WhiteSpacePre);
} // namespace Qt

class QTextBlock {
public:
    /*!
        Constructs an invalid block. Valid blocks come from a document.
    */
    QTextBlock() = default;

    bool isValid() const;
    QString text() const;
    int position() const;
    int length() const;
    bool contains(int position) const;
    int blockNumber() const { return m_index; }
    QTextBlockFormat blockFormat() const;
    QTextCharFormat charFormat() const;
    QList<QTextLayout::FormatRange> textFormats() const;
    QTextLayout *layout() const;
    const QTextDocument *document() const { return m_document; }
    QTextBlock next() const;
    QTextBlock previous() const;

    /*!
        Returns \c true if \a a and \a b are the same block of the same
        document; otherwise returns \c false.
    */
    friend bool operator==(const QTextBlock &a, const QTextBlock &b) {
        return a.m_document == b.m_document && a.m_index == b.m_index;
    }
    /*!
        Returns \c true if \a a and \a b are different blocks; otherwise
        returns \c false.
    */
    friend bool operator!=(const QTextBlock &a, const QTextBlock &b) { return !(a == b); }

private:
    friend class QTextDocument;
    friend class QAbstractTextDocumentLayout;
    QTextBlock(const QTextDocument *document, int index) : m_document(document), m_index(index) {}
    const QTextDocument *m_document = nullptr;
    int m_index = -1;
};

class QAbstractTextDocumentLayout {
public:
    QString anchorAt(const QPointF &pos) const;
    QSizeF documentSize() const;
    QRectF blockBoundingRect(const QTextBlock &block) const;
    QTextDocument *document() const { return m_document; }

private:
    friend class QTextDocument;
    friend class QTextDocumentPrivate;
    explicit QAbstractTextDocumentLayout(QTextDocument *document) : m_document(document) {}
    QTextDocument *m_document;
};

class QTextDocument : public QObject {
    Q_OBJECT
    Q_PROPERTY(qreal textWidth READ textWidth WRITE setTextWidth)
    Q_PROPERTY(int blockCount READ blockCount)
    Q_PROPERTY(qreal documentMargin READ documentMargin WRITE setDocumentMargin)
public:
    /*!
        \enum QTextDocument::MetaInformation

        What a document knows about itself.

        \value DocumentTitle The title of the document, from the \c title element of its HTML.
        \value DocumentUrl The URL that the document came from, for the application to set.
    */
    enum MetaInformation { DocumentTitle, DocumentUrl };

    explicit QTextDocument(QObject *parent = nullptr);
    explicit QTextDocument(const QString &text, QObject *parent = nullptr);
    ~QTextDocument() override;

    bool isEmpty() const;
    virtual void clear();

    void setPlainText(const QString &text);
    QString toPlainText() const;
    void setHtml(const QString &html);
    QString toHtml() const;

    void setMetaInformation(MetaInformation info, const QString &string);
    QString metaInformation(MetaInformation info) const;

    void setDefaultFont(const QFont &font);
    QFont defaultFont() const;
    void setDefaultTextOption(const QTextOption &option);
    QTextOption defaultTextOption() const;

    void setTextWidth(qreal width);
    qreal textWidth() const;
    qreal idealWidth() const;
    QSizeF size() const;
    qreal documentMargin() const;
    void setDocumentMargin(qreal margin);

    int blockCount() const;
    int lineCount() const;
    int characterCount() const;
    QChar characterAt(int pos) const;

    QTextBlock begin() const;
    QTextBlock end() const;
    QTextBlock firstBlock() const;
    QTextBlock lastBlock() const;
    QTextBlock findBlock(int pos) const;
    QTextBlock findBlockByNumber(int blockNumber) const;

    void drawContents(QPainter *painter, const QRectF &rect = QRectF());
    QAbstractTextDocumentLayout *documentLayout() const;

signals:
    /*!
        This signal is emitted when the contents of the document changed.
    */
    void contentsChanged() { emitSignal(&QTextDocument::contentsChanged); }

private:
    friend class QTextBlock;
    friend class QAbstractTextDocumentLayout;
    friend class QTextDocumentPrivate;
    std::unique_ptr<QTextDocumentPrivate> d;
};
