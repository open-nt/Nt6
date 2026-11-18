// MIT License - see LICENSE
#include <QtWidgets/qlabel.h>

#include <QtGui/qaction.h>

#include <QtGui/qcursor.h>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtGui/qtextdocument.h>
#include <QtWidgets/qstyle.h>

#include <algorithm>

/*!
    \class QLabel
    \brief The QLabel class displays text.

    A label shows a text, aligned in its rectangle. It does nothing else:

    \code
    QLabel *label = new QLabel("Name:", parent);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    \endcode

    The text is drawn in the WindowText color of the palette with the font of the widget. A label with
    word wrap breaks lines at the width it gets.

    A text that looks like HTML is shown as rich text, through a QTextDocument, which says what is
    understood; setTextFormat() decides it for the text if it does not look like what it is. A link in the text
    shows the pointing hand, and a click on it emits linkActivated():

    \code
    QLabel *label = new QLabel("<b>Done.</b> See the <a href=\"report\">report</a>.", parent);
    QObject::connect(label, &QLabel::linkActivated, [](const QString &link) { open(link); });
    \endcode

    A label: plain, bold and larger, word-wrapped, right-aligned, and disabled.

    \image widgets/qlabel.png QLabel in the light color scheme

    \differences
    The label shows plain text, rich text or a pixmap: no movies, pictures, Markdown, selection of the text with the mouse or links
    reached with the keys. It has no \c indent().
    Of the textInteractionFlags(), only Qt::LinksAccessibleByMouse has an effect. Nothing opens a link for openExternalLinks(): a click emits
    linkActivated() with or without it. Rich text is placed as a whole by the alignment of the label; its
    paragraphs are aligned by their own HTML. A word-wrapped label has no height-for-width, so its size
    hint is that of the text with the lines that it has without the wrapping.
    \enddifferences

    \sa QPushButton, QWidget
*/

/*!
    Constructs an empty label with the given \a parent.
*/
QLabel::QLabel(QWidget *parent) : QFrame(parent) {}

/*!
    Constructs a label that shows \a text, with the given \a parent.
*/
QLabel::QLabel(const QString &text, QWidget *parent) : QFrame(parent), m_text(text) { updateDocument(); }

/*!
    Destroys the label.
*/
QLabel::~QLabel() = default;

/*!
    \fn QString QLabel::text() const

    Returns the text of the label.

    \sa setText()
*/

/*!
    Sets the text of the label to \a text.

    \sa text()
*/
void QLabel::setText(const QString &text) {
    if (m_text == text && m_pixmap.isNull())
        return;
    m_text = text;
    m_selectionStart = m_selectionLength = 0;
    m_pixmap = QPixmap();
    updateDocument();
    updateBuddyShortcut();
    updateGeometry();
    update();
}

/*!
    \since 6.0

    Returns the buddy of the label, or \c nullptr.
*/
QWidget *QLabel::buddy() const { return m_buddy.data(); }

/*!
    \since 6.0

    Sets the buddy of the label to \a buddy: Alt and the mnemonic of the text, as in \c{"&Name:"}, give the buddy
    the focus.

    \code
    auto *label = new QLabel("&Port:", this);
    label->setBuddy(portEdit);
    \endcode
*/
void QLabel::setBuddy(QWidget *buddy) {
    m_buddy = buddy;
    updateBuddyShortcut();
}

void QLabel::updateBuddyShortcut() {
    const QKeySequence key = m_buddy ? QKeySequence::mnemonic(m_text) : QKeySequence();
    if (!m_buddyAction) {
        if (key.isEmpty())
            return;
        m_buddyAction = new QAction(this);
        QObject::connect(m_buddyAction, &QAction::triggered, this, [this] {
            if (m_buddy && m_buddy->isEnabled())
                m_buddy->setFocus();
        });
        addAction(m_buddyAction);
    }
    m_buddyAction->setShortcut(key);
}

/*!
    \fn Qt::TextFormat QLabel::textFormat() const

    Returns how the text of the label is read. The default is Qt::AutoText: a text that
    Qt::mightBeRichText() takes for rich text is shown as rich text, and any other as it is.

    \sa setTextFormat()
*/

/*!
    Sets how the text of the label is read to \a format. Qt::MarkdownText is shown as plain text.

    \sa textFormat()
*/
void QLabel::setTextFormat(Qt::TextFormat format) {
    if (m_format == format)
        return;
    m_format = format;
    updateDocument();
    updateGeometry();
    update();
}

/*!
    \fn Qt::TextInteractionFlags QLabel::textInteractionFlags() const
    \since 6.0

    Returns how the user can interact with the text. The default is Qt::LinksAccessibleByMouse.

    \sa setTextInteractionFlags()
*/

/*!
    \fn void QLabel::setTextInteractionFlags(Qt::TextInteractionFlags flags)
    \since 6.0

    Sets how the user can interact with the text to \a flags. Without Qt::LinksAccessibleByMouse, clicking a
    link emits nothing. Selecting and editing the text are not supported.
*/

/*!
    \fn bool QLabel::openExternalLinks() const

    Returns whether the label is to open the links that the user clicks. Nt has nothing to open them
    with, so this is recorded only.

    \sa setOpenExternalLinks(), linkActivated()
*/

/*!
    \fn void QLabel::setOpenExternalLinks(bool open)

    Sets whether the label is to open links to \a open; recorded only.

    \sa openExternalLinks()
*/

// Makes the document of a text that is rich text, and drops that of one that is not.
void QLabel::updateDocument() {
    const bool rich =
        !m_text.isEmpty() && (m_format == Qt::RichText || (m_format == Qt::AutoText && Qt::mightBeRichText(m_text)));
    setHovered(QString());
    m_pressed.clear();
    if (!rich) {
        m_document.reset();
        return;
    }
    if (!m_document) {
        m_document = std::make_unique<QTextDocument>();
        m_document->setDocumentMargin(0);
        m_document->setResourceProvider(m_resourceProvider);
    }
    m_document->setDefaultFont(font());
    m_document->setHtml(m_text);
    // The mouse is followed without a button too, for the links.
    if (m_text.contains(QStringLiteral("href"), Qt::CaseInsensitive))
        setMouseTracking(true);
}

// The rectangle that the text or the pixmap is placed in: inside the frame, the contents margins and the margin.
QRect QLabel::labelArea() const {
    QRect area =
        QFrame::contentsRect().marginsRemoved(contentsMargins()).adjusted(m_margin, m_margin, -m_margin, -m_margin);
    const int indent = indentWidth();
    if (indent > 0) {
        if (m_alignment & Qt::AlignRight)
            area.setRight(area.right() - indent);
        else if (m_alignment & Qt::AlignLeft)
            area.setLeft(area.left() + indent);
        if (m_alignment & Qt::AlignTop)
            area.setTop(area.top() + indent);
        else if (m_alignment & Qt::AlignBottom)
            area.setBottom(area.bottom() - indent);
    }
    return area;
}

// The indent of the text: indent(), or, if it is negative, half the width of an "x" for a label with a frame.
int QLabel::indentWidth() const {
    if (m_indent >= 0)
        return m_indent;
    return frameWidth() > 0 ? fontMetrics().horizontalAdvance(QLatin1Char('x')) / 2 : 0;
}

// The room around the text or the pixmap: twice the margin, and the frame and the contents margins.
QSize QLabel::labelExtra() const {
    const QMargins m = contentsMargins();
    const int frame = 2 * frameWidth() + 2 * m_margin;
    const int indent = indentWidth();
    const int across = (m_alignment & (Qt::AlignLeft | Qt::AlignRight)) ? indent : 0;
    const int down = (m_alignment & (Qt::AlignTop | Qt::AlignBottom)) ? indent : 0;
    return QSize(frame + m.left() + m.right() + across, frame + m.top() + m.bottom() + down);
}

// Lays the document out for the label as it is, and returns where it is drawn.
QRect QLabel::documentRect() const {
    const QRect area = labelArea();
    m_document->setTextWidth(m_wordWrap ? std::max(1, area.width()) : -1);
    const QSizeF size = m_document->size();
    const int w = static_cast<int>(size.width()), h = static_cast<int>(size.height());
    int x = area.left();
    int y = area.top();
    if (m_alignment & Qt::AlignHCenter)
        x += (area.width() - w) / 2;
    else if (m_alignment & Qt::AlignRight)
        x = area.right() + 1 - w;
    if (m_alignment & Qt::AlignVCenter)
        y += (area.height() - h) / 2;
    else if (m_alignment & Qt::AlignBottom)
        y = area.bottom() + 1 - h;
    return QRect(x, y, w, h);
}

QString QLabel::linkAt(const QPoint &pos) const {
    if (!m_document)
        return QString();
    const QRect where = documentRect();
    return m_document->documentLayout()->anchorAt(QPointF(pos.x() - where.x(), pos.y() - where.y()));
}

void QLabel::setHovered(const QString &link) {
    if (m_hovered == link)
        return;
    m_hovered = link;
    if (link.isEmpty())
        unsetCursor();
    else
        setCursor(Qt::PointingHandCursor);
    emit linkHovered(link);
}

/*!
    Sets the text of the label to \a number, as a decimal number.
*/
void QLabel::setNum(int number) { setText(QString::number(number)); }

/*!
    Shows \a pixmap instead of the text, which is cleared. The pixmap is placed by the alignment, or scaled to the
    label if hasScaledContents() is \c true.

    \sa pixmap(), setText()
*/
void QLabel::setPixmap(const QPixmap &pixmap) {
    m_pixmap = pixmap;
    m_text.clear();
    updateGeometry();
    update();
}

/*!
    \fn QPixmap QLabel::pixmap() const

    Returns the pixmap of the label, or a null pixmap if it shows text.

    \sa setPixmap()
*/

/*!
    \fn bool QLabel::hasScaledContents() const

    Returns whether the pixmap is scaled to fill the label. The default is \c false.
*/

/*!
    Sets whether the pixmap is scaled to fill the label to \a scaled.
*/
void QLabel::setScaledContents(bool scaled) {
    m_scaled = scaled;
    updateGeometry();
    update();
}

/*!
    Clears the text of the label.
*/
void QLabel::clear() { setText(QString()); }

/*!
    \fn Qt::Alignment QLabel::alignment() const

    Returns the alignment of the text in the label. The default is left and vertically centered.

    \sa setAlignment()
*/

/*!
    Sets the alignment of the text to \a alignment.

    \sa alignment()
*/
void QLabel::setAlignment(Qt::Alignment alignment) {
    m_alignment = alignment;
    update();
}

/*!
    \fn bool QLabel::wordWrap() const

    Returns whether lines are broken at the width of the label.

    \sa setWordWrap()
*/

/*!
    Sets whether lines are broken at the width of the label to \a on.

    \sa wordWrap()
*/
void QLabel::setWordWrap(bool on) {
    m_wordWrap = on;
    updateGeometry();
    update();
}

/*!
    \fn int QLabel::margin() const

    Returns the space between the text and the edge of the label, in pixels.

    \sa setMargin()
*/

/*!
    Sets the margin to \a margin pixels.

    \sa margin()
*/
void QLabel::setMargin(int margin) {
    m_margin = margin;
    updateGeometry();
    update();
}

QSize QLabel::textSize(int width) const {
    const int flags = m_wordWrap ? int(Qt::TextWordWrap) : 0;
    return fontMetrics().boundingRect(QRect(0, 0, width, 0), flags, m_text).size();
}

/*!
    \reimp
*/
QSize QLabel::sizeHint() const {
    if (!m_pixmap.isNull())
        return m_pixmap.size() + labelExtra();
    if (m_document) {
        m_document->setTextWidth(-1);
        const QSizeF size = m_document->size();
        return QSize(static_cast<int>(size.width()), static_cast<int>(size.height())) + labelExtra();
    }
    return textSize(0) + labelExtra();
}

/*!
    \reimp
*/
QSize QLabel::minimumSizeHint() const {
    if (!m_pixmap.isNull())
        return m_scaled ? labelExtra() : sizeHint();
    if (m_wordWrap)
        return QSize(fontMetrics().averageCharWidth() * 3, fontMetrics().height()) + labelExtra();
    return sizeHint();
}

/*!
    \reimp
*/
void QLabel::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    drawFrame(&painter);
    if (!m_pixmap.isNull()) {
        const QRect area = labelArea();
        if (m_scaled) {
            painter.drawPixmap(area, m_pixmap);
            return;
        }
        // Placed in the area by the alignment.
        int x = area.left();
        int y = area.top();
        if (m_alignment & Qt::AlignHCenter)
            x += (area.width() - m_pixmap.width()) / 2;
        else if (m_alignment & Qt::AlignRight)
            x = area.right() + 1 - m_pixmap.width();
        if (m_alignment & Qt::AlignVCenter)
            y += (area.height() - m_pixmap.height()) / 2;
        else if (m_alignment & Qt::AlignBottom)
            y = area.bottom() + 1 - m_pixmap.height();
        painter.drawPixmap(x, y, m_pixmap);
        return;
    }
    painter.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::WindowText));
    painter.setFont(font());
    if (m_document) {
        const QRect where = documentRect();
        painter.translate(where.x(), where.y());
        m_document->drawContents(&painter);
        return;
    }
    int flags = int(m_alignment);
    if (m_wordWrap)
        flags |= Qt::TextWordWrap;
    if (m_selectionLength > 0 && !m_wordWrap && !m_text.contains(QLatin1Char('\n'))) {
        // The selection of a line of plain text: highlighted under the text, which is drawn in its color there.
        const QRect area = labelArea();
        const QFontMetrics fm = fontMetrics();
        const QRect bounds = fm.boundingRect(area, flags, m_text);
        const int x0 = bounds.left() + fm.horizontalAdvance(m_text.left(m_selectionStart));
        const int x1 = bounds.left() + fm.horizontalAdvance(m_text.left(m_selectionStart + m_selectionLength));
        const QRect selected(x0, bounds.top(), x1 - x0, bounds.height());
        const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
        painter.fillRect(selected, palette().color(group, QPalette::Highlight));
        painter.drawText(area, flags, m_text);
        painter.setClipRect(selected);
        painter.setPen(palette().color(group, QPalette::HighlightedText));
        painter.drawText(area, flags, m_text);
        return;
    }
    painter.drawText(labelArea(), flags, m_text);
}

/*!
    \reimp
*/
void QLabel::changeEvent(QEvent *event) {
    if (m_document && event->type() == QEvent::FontChange) {
        // The sizes of the HTML are taken of the font.
        m_document->setDefaultFont(font());
        m_document->setHtml(m_text);
        updateGeometry();
    }
    QFrame::changeEvent(event);
}

/*!
    \reimp

    A press on a link is the start of a click on it.
*/
void QLabel::mousePressEvent(QMouseEvent *event) {
    m_pressed = event->button() == Qt::LeftButton && (m_interaction & Qt::LinksAccessibleByMouse) ? linkAt(event->pos())
                                                                                                  : QString();
    if (m_pressed.isEmpty())
        event->ignore();
    else
        event->accept();
}

/*!
    \reimp

    Over a link the cursor is the pointing hand, and linkHovered() is emitted.
*/
void QLabel::mouseMoveEvent(QMouseEvent *event) {
    if (!m_document) {
        event->ignore();
        return;
    }
    setHovered(linkAt(event->pos()));
    if (m_pressed.isEmpty())
        event->ignore();
    else
        event->accept();
}

/*!
    \reimp

    A release on the link that was pressed emits linkActivated().
*/
void QLabel::mouseReleaseEvent(QMouseEvent *event) {
    if (m_pressed.isEmpty() || event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    const QString link = m_pressed;
    m_pressed.clear();
    event->accept();
    if (linkAt(event->pos()) == link)
        emit linkActivated(link);
}

/*!
    \reimp
*/
void QLabel::leaveEvent(QEvent *event) {
    setHovered(QString());
    QFrame::leaveEvent(event);
}

QString QLabel::plainText() const {
    if (m_document)
        return m_document->toPlainText();
    return m_text;
}

/*!
    \fn int QLabel::indent() const
    \since 6.0

    Returns the indent of the text in pixels, or -1, the default, for half the width of an "x" in a label with a
    frame and none in a label without one.
*/

/*!
    \since 6.0

    Indents the text \a indent pixels from the side it is aligned to.
*/
void QLabel::setIndent(int indent) {
    m_indent = indent;
    updateGeometry();
    update();
}

/*!
    \since 6.0

    Returns the selected text, without its markup.

    \sa setSelection()
*/
QString QLabel::selectedText() const { return plainText().mid(m_selectionStart, m_selectionLength); }

/*!
    \fn int QLabel::selectionStart() const
    \since 6.0

    Returns where the selection starts, or -1 if nothing is selected.
*/

/*!
    \fn bool QLabel::hasSelectedText() const
    \since 6.0

    Returns \c true if some of the text is selected.
*/

/*!
    \since 6.0

    Selects \a length characters of the text from \a start, which the label highlights for a line of plain text.
    Nothing happens unless textInteractionFlags() has Qt::TextSelectableByMouse or Qt::TextSelectableByKeyboard.
*/
void QLabel::setSelection(int start, int length) {
    if (!(m_interaction & (Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard)))
        return;
    const int size = static_cast<int>(plainText().size());
    m_selectionStart = std::clamp(start, 0, size);
    m_selectionLength = std::clamp(length, 0, size - m_selectionStart);
    update();
}

/*!
    \fn QTextDocument::ResourceProvider QLabel::resourceProvider() const
    \since 6.0

    Returns the function that gives the resources of rich text, or an empty function.
*/

/*!
    \since 6.0

    Sets the function that gives the resources, such as images, of rich text to \a provider; see
    QTextDocument::setResourceProvider().
*/
void QLabel::setResourceProvider(const QTextDocument::ResourceProvider &provider) {
    m_resourceProvider = provider;
    if (m_document)
        m_document->setResourceProvider(provider);
}

#include "moc_qlabel.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
