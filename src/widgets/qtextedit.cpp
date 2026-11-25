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

    A text edit showing rich text from HTML: a heading, bold, italic and colored text, and a list.

    \image widgets/qtextedit.png QTextEdit in the light color scheme

    \differences
    Rich text is shown, not edited: there is no QTextCursor over the document, and text with formats has no
    selection. When the text is changed in another way than by setHtml(), by typing for one, the editor goes on
    with the text alone, as a QPlainTextEdit: the formats are gone. setCurrentFont(), setTextColor(),
    setAlignment() and the other functions that set the format of text keep it as the current format, which
    their getters return, but typed text does not take it. autoFormatting() and \c acceptRichText() do
    nothing, and there is no \c scrollToAnchor(). The document that document() returns is that of the editor only for
    reading and for giving it other contents as a whole. A QTextEdit is a QPlainTextEdit here; in Qt the two
    classes are not related.
    \enddifferences

    \sa QPlainTextEdit, QTextBrowser, QTextDocument
*/

/*!
    Constructs an empty editor with the given \a parent.
*/
QTextEdit::QTextEdit(QWidget *parent) : QPlainTextEdit(parent), m_document(std::make_unique<QTextDocument>()) {
    // The document that document() returns is modified when the editor is, and the other way round.
    connect(m_document.get(), &QTextDocument::modificationChanged, this,
            [this](bool changed) { setModifiedFlag(changed); });
    connect(this, &QPlainTextEdit::modificationChanged, this,
            [this](bool changed) { m_document->setModified(changed); });
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

/*!
    \since 6.0

    Shows the Markdown in \a markdown, with its formats, as QTextDocument::setMarkdown() reads it.

    \sa toMarkdown(), setHtml()
*/
void QTextEdit::setMarkdown(const QString &markdown) {
    m_adopting = true;
    m_document->setDefaultFont(font());
    m_document->setMarkdown(markdown);
    m_adopting = false;
    adoptDocument();
}

/*!
    \since 6.0

    Returns the text as Markdown, written with \a features.

    \sa setMarkdown(), QTextDocument::toMarkdown()
*/
QString QTextEdit::toMarkdown(QTextDocument::MarkdownFeatures features) const {
    return document()->toMarkdown(features);
}

/*!
    \since 6.0

    Inserts the HTML \a text at the cursor, in place of the selection, with its formats; the cursor is after it.

    \sa QTextCursor::insertHtml()
*/
void QTextEdit::insertHtml(const QString &text) {
    QTextCursor cursor = textCursor();
    cursor.insertHtml(text);
    setTextCursor(cursor);
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

/*!
    \enum QTextEdit::AutoFormattingFlag
    \since 6.0

    Formats that the editor would apply as the user types. Nt keeps the value and applies none.

    \value AutoNone No automatic formatting.
    \value AutoBulletList A line that starts with an asterisk would become a bullet list.
    \value AutoAll All automatic formatting.
*/

/*!
    \fn QTextEdit::AutoFormatting QTextEdit::autoFormatting() const
    \since 6.0

    Returns the automatic formatting, AutoNone by default.
*/

/*!
    \fn void QTextEdit::setAutoFormatting(AutoFormatting features)
    \since 6.0

    Sets the automatic formatting to \a features, which Nt keeps.
*/

void QTextEdit::mergeFormat(const QTextCharFormat &modifier) {
    mergeCurrentCharFormat(modifier);
    emit currentCharFormatChanged(currentCharFormat());
}

/*!
    \fn Qt::Alignment QTextEdit::alignment() const
    \since 6.0

    Returns the alignment of the current paragraph, Qt::AlignLeft by default.
*/

/*!
    \since 6.0

    Sets the alignment of the current paragraph to \a a. Nt keeps it; the text is shown aligned left.
*/
void QTextEdit::setAlignment(Qt::Alignment a) { m_alignment = a; }

/*!
    \since 6.0

    Returns the font of the current format: the font of the editor with the properties that were set on the format.
*/
QFont QTextEdit::currentFont() const {
    QFont f = font();
    const QTextCharFormat format = currentCharFormat();
    if (format.hasProperty(QTextFormat::FontFamilies) && !format.fontFamilies().isEmpty())
        f.setFamilies(format.fontFamilies());
    if (format.hasProperty(QTextFormat::FontPointSize) && format.fontPointSize() > 0)
        f.setPointSizeF(format.fontPointSize());
    if (format.hasProperty(QTextFormat::FontWeight))
        f.setWeight(static_cast<QFont::Weight>(format.fontWeight()));
    if (format.hasProperty(QTextFormat::FontItalic))
        f.setItalic(format.fontItalic());
    if (format.hasProperty(QTextFormat::FontUnderline))
        f.setUnderline(format.fontUnderline());
    return f;
}

/*!
    \since 6.0

    Sets the font of the current format to \a f.
*/
void QTextEdit::setCurrentFont(const QFont &f) {
    QTextCharFormat format;
    format.setFontFamilies(f.families().isEmpty() ? QStringList{f.family()} : f.families());
    if (f.pointSizeF() > 0)
        format.setFontPointSize(f.pointSizeF());
    format.setFontWeight(static_cast<int>(f.weight()));
    format.setFontItalic(f.italic());
    format.setFontUnderline(f.underline());
    mergeFormat(format);
}

/*!
    \since 6.0

    Returns the font family of the current format.
*/
QString QTextEdit::fontFamily() const {
    const QFont f = currentFont();
    return f.families().value(0, f.family());
}

/*!
    \since 6.0

    Sets the font family of the current format to \a fontFamily.
*/
void QTextEdit::setFontFamily(const QString &fontFamily) {
    QTextCharFormat format;
    format.setFontFamilies({fontFamily});
    mergeFormat(format);
}

/*!
    \since 6.0

    Returns the point size of the font of the current format.
*/
qreal QTextEdit::fontPointSize() const { return currentFont().pointSizeF(); }

/*!
    \since 6.0

    Sets the point size of the current format to \a s; a size of 0 or less is ignored.
*/
void QTextEdit::setFontPointSize(qreal s) {
    if (s <= 0)
        return;
    QTextCharFormat format;
    format.setFontPointSize(s);
    mergeFormat(format);
}

/*!
    \since 6.0

    Returns the weight of the font of the current format, a QFont::Weight.
*/
int QTextEdit::fontWeight() const { return static_cast<int>(currentFont().weight()); }

/*!
    \since 6.0

    Sets the font weight of the current format to \a weight.
*/
void QTextEdit::setFontWeight(int weight) {
    QTextCharFormat format;
    format.setFontWeight(weight);
    mergeFormat(format);
}

/*!
    \since 6.0

    Returns \c true if the current format is italic.
*/
bool QTextEdit::fontItalic() const { return currentFont().italic(); }

/*!
    \since 6.0

    Makes the current format italic if \a italic is \c true.
*/
void QTextEdit::setFontItalic(bool italic) {
    QTextCharFormat format;
    format.setFontItalic(italic);
    mergeFormat(format);
}

/*!
    \since 6.0

    Returns \c true if the current format is underlined.
*/
bool QTextEdit::fontUnderline() const { return currentFont().underline(); }

/*!
    \since 6.0

    Makes the current format underlined if \a underline is \c true.
*/
void QTextEdit::setFontUnderline(bool underline) {
    QTextCharFormat format;
    format.setFontUnderline(underline);
    mergeFormat(format);
}

/*!
    \since 6.0

    Returns the text color of the current format, or the Text color of the palette if it has none.
*/
QColor QTextEdit::textColor() const {
    const QBrush brush = currentCharFormat().foreground();
    return brush.style() != Qt::NoBrush ? brush.color() : palette().color(QPalette::Text);
}

/*!
    \since 6.0

    Sets the text color of the current format to \a c.
*/
void QTextEdit::setTextColor(const QColor &c) {
    QTextCharFormat format;
    format.setForeground(QBrush(c));
    mergeFormat(format);
}

/*!
    \since 6.0

    Returns the background color of the current format, or an invalid color if it has none.
*/
QColor QTextEdit::textBackgroundColor() const {
    const QBrush brush = currentCharFormat().background();
    return brush.style() != Qt::NoBrush ? brush.color() : QColor();
}

/*!
    \since 6.0

    Sets the background color of the current format to \a c.
*/
void QTextEdit::setTextBackgroundColor(const QColor &c) {
    QTextCharFormat format;
    format.setBackground(QBrush(c));
    mergeFormat(format);
}

#include "moc_qtextedit.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
