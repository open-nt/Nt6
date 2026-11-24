// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtCore/qsize.h>
#include <QtCore/qrect.h>
#include <QtGui/qtextdocument.h>

class QPlainTextDocumentLayout : public QObject {
    Q_OBJECT
    Q_PROPERTY(int cursorWidth READ cursorWidth WRITE setCursorWidth)
public:
    explicit QPlainTextDocumentLayout(QTextDocument *document);
    ~QPlainTextDocumentLayout() override;

    QTextDocument *document() const { return m_document.data(); }
    int pageCount() const;
    QSizeF documentSize() const;
    QRectF blockBoundingRect(const QTextBlock &block) const;
    QRectF frameBoundingRect() const;
    void ensureBlockLayout(const QTextBlock &block) const;

    void setCursorWidth(int width) { m_cursorWidth = width; }
    int cursorWidth() const { return m_cursorWidth; }
    void requestUpdate();

private:
    QPointer<QTextDocument> m_document;
    int m_cursorWidth = 1;
};
