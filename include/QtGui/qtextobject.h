// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtGui/qtextformat.h>

class QTextDocument;
class QTextFrame;

class QTextObject : public QObject {
    Q_OBJECT
public:
    QTextFormat format() const { return m_format; }
    int formatIndex() const { return -1; }
    QTextDocument *document() const { return m_document; }
    int objectIndex() const { return -1; }

protected:
    explicit QTextObject(QTextDocument *doc);
    ~QTextObject() override;
    void setFormat(const QTextFormat &format);

private:
    friend class QTextDocumentPrivate;
    Q_DISABLE_COPY(QTextObject)
    QTextDocument *m_document;
    QTextFormat m_format;
};

class QTextFrame : public QTextObject {
    Q_OBJECT
public:
    explicit QTextFrame(QTextDocument *doc);
    ~QTextFrame() override;

    void setFrameFormat(const QTextFrameFormat &format) { setFormat(format); }
    QTextFrameFormat frameFormat() const { return format().toFrameFormat(); }

    int firstPosition() const;
    int lastPosition() const;
    QList<QTextFrame *> childFrames() const;
    QTextFrame *parentFrame() const;

private:
    friend class QTextDocumentPrivate;
    int m_firstBlock = 0; // the blocks of the frame; -1 for the last block of the document
    int m_lastBlock = -1;
};
