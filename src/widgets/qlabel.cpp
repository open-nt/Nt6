// MIT License - see LICENSE
#include <QtWidgets/qlabel.h>

#include <QtGui/qpainter.h>
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

    A label: plain, bold and larger, word-wrapped, right-aligned, and disabled.

    \image widgets/qlabel.png QLabel in the light color scheme

    \differences
    The label shows plain text or a pixmap: no rich text, movies, links, selection or buddies. It has no frame
    (it is not a QFrame), \c indent(), \c textFormat() or \c setBuddy(). A word-wrapped label
    has no height-for-width, so its size hint is the height of the text on one line unless it has new lines.
    \enddifferences

    \sa QPushButton, QWidget
*/

/*!
    Constructs an empty label with the given \a parent.
*/
QLabel::QLabel(QWidget *parent) : QWidget(parent) {}

/*!
    Constructs a label that shows \a text, with the given \a parent.
*/
QLabel::QLabel(const QString &text, QWidget *parent) : QWidget(parent), m_text(text) {}

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
    m_pixmap = QPixmap();
    updateGeometry();
    update();
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
        return QSize(m_pixmap.width() + 2 * m_margin, m_pixmap.height() + 2 * m_margin);
    const QSize s = textSize(0);
    return QSize(s.width() + 2 * m_margin, s.height() + 2 * m_margin);
}

/*!
    \reimp
*/
QSize QLabel::minimumSizeHint() const {
    if (!m_pixmap.isNull())
        return m_scaled ? QSize(2 * m_margin, 2 * m_margin) : sizeHint();
    if (m_wordWrap) {
        const int widest = fontMetrics().averageCharWidth() * 3;
        return QSize(widest + 2 * m_margin, fontMetrics().height() + 2 * m_margin);
    }
    return sizeHint();
}

/*!
    \reimp
*/
void QLabel::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    if (!m_pixmap.isNull()) {
        const QRect area = rect().adjusted(m_margin, m_margin, -m_margin, -m_margin);
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
    int flags = int(m_alignment);
    if (m_wordWrap)
        flags |= Qt::TextWordWrap;
    painter.drawText(rect().adjusted(m_margin, m_margin, -m_margin, -m_margin), flags, m_text);
}

/*!
    \reimp
*/
void QLabel::changeEvent(QEvent *event) { QWidget::changeEvent(event); }

#include "moc_qlabel.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
