// MIT License - see LICENSE
#include <QtWidgets/qtextedit.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtGui/qtextdocument.h>
#include <QtWidgets/qscrollbar.h>

#include <algorithm>

/*!
    \class QTextEdit
    \brief The QTextEdit class shows rich text, and edits plain text.

    A QTextEdit is a QPlainTextEdit that also shows rich text. setHtml() gives it HTML, which it shows with
    its fonts, colors, headings, lists and links, through a QTextDocument; QTextDocument says what of HTML is
    understood. setText() takes either kind of text, and append() adds a line:

    \code
    QTextEdit *view = new QTextEdit(window);
    view->setReadOnly(true);
    view->setHtml("<h3>Result</h3><p>All <b>12</b> tests passed.</p>");
    \endcode

    toHtml() returns the text as HTML, and document() the document.

    \differences
    Rich text is shown, not edited: there is no QTextCursor over the document, and text with formats has no
    selection. When the text is changed in another way than by setHtml(), by typing for one, the editor goes on
    with the text alone, as a QPlainTextEdit: the formats are gone. \c setMarkdown(), \c insertHtml(),
    \c setCurrentFont(), \c setTextColor() and the other functions that set the format of text are not there,
    and \c acceptRichText() does nothing. The document that document() returns is that of the editor only for
    reading and for giving it other contents as a whole. A QTextEdit is a QPlainTextEdit here; in Qt the two
    classes are not related.
    \enddifferences

    \sa QPlainTextEdit, QTextBrowser, QTextDocument
*/

/*!
    Constructs an empty editor with the given \a parent.
*/
QTextEdit::QTextEdit(QWidget *parent) : QPlainTextEdit(parent), m_document(std::make_unique<QTextDocument>()) {
    // A change of the text that is not one of the document leaves the plain text to show.
    connect(this, &QPlainTextEdit::textChanged, this, [this] {
        if (m_adopting)
            return;
        m_stale = true;
        if (m_rich) {
            m_rich = false;
            viewport()->unsetCursor();
            viewport()->update();
        }
    });
    // New contents of the document, set by the program, are shown.
    connect(m_document.get(), &QTextDocument::contentsChanged, this, [this] {
        if (!m_adopting)
            adoptDocument();
    });
}

/*!
    Constructs an editor with the given \a text and \a parent. The text is plain text or HTML, as for
    setText().
*/
QTextEdit::QTextEdit(const QString &text, QWidget *parent) : QTextEdit(parent) { setText(text); }

/*!
    Destroys the editor.
*/
QTextEdit::~QTextEdit() = default;

/*!
    \fn bool QTextEdit::acceptRichText() const

    Returns whether the editor takes rich text from the clipboard. It has no effect.

    \sa setAcceptRichText()
*/

/*!
    \fn void QTextEdit::setAcceptRichText(bool accept)

    Sets \a accept, which has no effect.

    \sa acceptRichText()
*/

/*!
    Sets the text to \a text: as rich text if Qt::mightBeRichText() takes it for HTML, as setHtml() does, and
    as plain text otherwise, as setPlainText() does.
*/
void QTextEdit::setText(const QString &text) {
    if (Qt::mightBeRichText(text))
        setHtml(text);
    else
        setPlainText(text);
}

/*!
    Shows the HTML in \a text, with its formats. toPlainText() is its text without them.

    \sa toHtml(), QTextDocument::setHtml()
*/
void QTextEdit::setHtml(const QString &text) {
    m_adopting = true;
    m_document->setDefaultFont(font());
    m_document->setHtml(text);
    m_adopting = false;
    adoptDocument();
}

// Shows the document as it is now.
void QTextEdit::adoptDocument() {
    m_adopting = true;
    setPlainText(m_document->toPlainText());
    m_adopting = false;
    m_rich = true;
    m_stale = false;
    layoutRichText();
    viewport()->update();
}

/*!
    Returns the text as HTML: with its formats if it was set as rich text, and as paragraphs of plain text
    otherwise.

    \sa setHtml(), toPlainText()
*/
QString QTextEdit::toHtml() const { return document()->toHtml(); }

/*!
    Returns the document of the editor, which has the text and its formats. The editor owns it. Giving the
    document other contents, with QTextDocument::setHtml() for example, shows them.
*/
QTextDocument *QTextEdit::document() const {
    if (m_stale) {
        // The text was edited: the document gets it, without telling the editor to show it.
        auto *self = const_cast<QTextEdit *>(this); // NOLINT(cppcoreguidelines-pro-type-const-cast): a cache
        self->m_adopting = true;
        m_document->setPlainText(toPlainText());
        self->m_adopting = false;
        m_stale = false;
    }
    return m_document.get();
}

/*!
    Returns the title of the document, which setHtml() reads from the \c title element.

    \sa setDocumentTitle()
*/
QString QTextEdit::documentTitle() const { return m_document->metaInformation(QTextDocument::DocumentTitle); }

/*!
    Sets the title of the document to \a title.

    \sa documentTitle()
*/
void QTextEdit::setDocumentTitle(const QString &title) {
    m_document->setMetaInformation(QTextDocument::DocumentTitle, title);
}

/*!
    Returns the target of the link at \a pos, in the coordinates of the viewport, or an empty string if
    there is no link there.
*/
QString QTextEdit::anchorAt(const QPoint &pos) const {
    if (!m_rich)
        return QString();
    return m_document->documentLayout()->anchorAt(
        QPointF(pos.x() + horizontalScrollBar()->value(), pos.y() + verticalScrollBar()->value()));
}

/*!
    Appends a new line with \a text, as appendPlainText() does. The formats of rich text that was shown are
    gone then.
*/
void QTextEdit::append(const QString &text) { appendPlainText(text); }

// Lays the document out for the viewport, and sets the scroll bars from its size. A scroll bar that comes or goes
// changes the width, so this goes on until the sizes agree.
void QTextEdit::layoutRichText() {
    if (!m_rich || m_layingRich)
        return;
    m_layingRich = true;
    for (int pass = 0; pass < 4; ++pass) {
        const int width = viewport()->width();
        m_document->setTextWidth(lineWrapMode() == NoWrap ? -1 : std::max(1, width));
        const QSizeF size = m_document->size();
        QScrollBar *vbar = verticalScrollBar(), *hbar = horizontalScrollBar();
        vbar->setPageStep(std::max(1, viewport()->height()));
        vbar->setRange(0, std::max(0, static_cast<int>(size.height()) - viewport()->height()));
        hbar->setPageStep(std::max(1, viewport()->width()));
        hbar->setRange(0, std::max(0, static_cast<int>(size.width()) - viewport()->width()));
        if (viewport()->width() == width)
            break;
    }
    m_layingRich = false;
}

/*!
    \reimp
*/
void QTextEdit::paintEvent(QPaintEvent *event) {
    if (!m_rich) {
        QPlainTextEdit::paintEvent(event);
        return;
    }
    layoutRichText(); // the editor below may have set the scroll bars for its lines
    QPainter painter(viewport());
    const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    const QRect all = viewport()->rect();
    painter.fillRect(all, palette().color(group, QPalette::Base));
    painter.setPen(palette().color(group, QPalette::Text));
    const int dx = horizontalScrollBar()->value(), dy = verticalScrollBar()->value();
    painter.translate(-dx, -dy);
    m_document->drawContents(&painter, QRectF(dx, dy, all.width(), all.height()));
}

/*!
    \reimp
*/
void QTextEdit::resizeEvent(QResizeEvent *event) {
    QPlainTextEdit::resizeEvent(event);
    layoutRichText();
}

/*!
    \reimp
*/
void QTextEdit::changeEvent(QEvent *event) {
    QPlainTextEdit::changeEvent(event);
    if (m_rich && event->type() == QEvent::FontChange) {
        m_document->setDefaultFont(font());
        layoutRichText();
    }
}

/*!
    \reimp

    Rich text has no cursor to put somewhere: the press is taken, and does nothing.
*/
void QTextEdit::mousePressEvent(QMouseEvent *event) {
    if (!m_rich) {
        QPlainTextEdit::mousePressEvent(event);
        return;
    }
    setFocus();
    event->accept();
}

/*!
    \reimp

    Over a link of rich text the cursor is the pointing hand.
*/
void QTextEdit::mouseMoveEvent(QMouseEvent *event) {
    if (!m_rich) {
        QPlainTextEdit::mouseMoveEvent(event);
        return;
    }
    if (anchorAt(event->pos()).isEmpty())
        viewport()->unsetCursor();
    else
        viewport()->setCursor(Qt::PointingHandCursor);
    event->accept();
}

/*!
    \reimp
*/
void QTextEdit::mouseReleaseEvent(QMouseEvent *event) {
    if (!m_rich) {
        QPlainTextEdit::mouseReleaseEvent(event);
        return;
    }
    event->accept();
}

/*!
    \reimp
*/
void QTextEdit::mouseDoubleClickEvent(QMouseEvent *event) {
    if (!m_rich) {
        QPlainTextEdit::mouseDoubleClickEvent(event);
        return;
    }
    event->accept();
}

#include "moc_qtextedit.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
