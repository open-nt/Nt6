// MIT License - see LICENSE
#include <QtWidgets/qheaderview.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>

/*!
    \class QHeaderView
    \brief The QHeaderView class provides the row or column headers of a table.

    A header is a row of sections, each with a label and a size. It is horizontal, for the columns of a table, or
    vertical, for its rows. The user drags the edge between two sections to resize the one before it, and clicks
    a section if the header has clickable sections.

    A QTableWidget has both headers, and gives them their sections and labels: use QTableWidget::horizontalHeader() and
    QTableWidget::verticalHeader() to reach them. The sizes of the sections of the headers are the widths of the columns
    and the heights of the rows.

    \code
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->resizeSection(0, 160);
    connect(table->horizontalHeader(), &QHeaderView::sectionClicked, [](int column) { qDebug() << column; });
    \endcode

    The headers of a table: the column labels, with the last section stretched, and the row labels.

    \image widgets/qheaderview.png QHeaderView in the light color scheme

    \differences
    There is no model: the sections and their labels come from the table that owns the header, so the count and the
    labels cannot be set on a header on its own. Sections cannot be moved, hidden, sorted or selected, and there are
    no resize modes (\c setSectionResizeMode()); every section but the last can be resized by the user, and the last
    one can be stretched. There are no logical and visual indexes: they are the same.
    \enddifferences

    \sa QTableWidget
*/

/*!
    Constructs an empty header with the given \a orientation and \a parent. The sections of a header are made by the
    table that has it.
*/
QHeaderView::QHeaderView(Qt::Orientation orientation, QWidget *parent) : QWidget(parent), m_orientation(orientation) {
    m_defaultSize = style()->pixelMetric(orientation == Qt::Horizontal ? QStyle::PM_HeaderDefaultSectionSizeHorizontal
                                                                      : QStyle::PM_HeaderDefaultSectionSizeVertical,
                                         nullptr, this);
    setSizePolicy(orientation == Qt::Horizontal ? QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)
                                                : QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding));
}

/*!
    Destroys the header.
*/
QHeaderView::~QHeaderView() = default;

/*!
    \fn Qt::Orientation QHeaderView::orientation() const

    Returns whether the header is horizontal (the headers of the columns) or vertical (the headers of the rows).
*/

/*!
    \fn int QHeaderView::count() const

    Returns the number of sections.
*/

// The size that the section has as set; a stretched last section may be shown larger.
int QHeaderView::storedSize(int logicalIndex) const { return m_sizes[static_cast<std::size_t>(logicalIndex)]; }

/*!
    Returns the size of the section \a logicalIndex: its width in a horizontal header, its height in a vertical one. A
    last section that is stretched can be larger than it was set. The size of a section that does not exist is 0.

    \sa resizeSection(), stretchLastSection()
*/
int QHeaderView::sectionSize(int logicalIndex) const {
    if (logicalIndex < 0 || logicalIndex >= count())
        return 0;
    const int size = storedSize(logicalIndex);
    if (m_stretchLast && logicalIndex == count() - 1) {
        const int available = (m_orientation == Qt::Horizontal ? width() : height()) - sectionPosition(logicalIndex);
        return std::max(size, available);
    }
    return size;
}

/*!
    Returns the position of the start of the section \a logicalIndex, from the start of the header and not counting
    offset(). It is the size of the sections before it.

    \sa logicalIndexAt(), sectionSize()
*/
int QHeaderView::sectionPosition(int logicalIndex) const {
    int position = 0;
    for (int i = 0; i < std::min(logicalIndex, count()); ++i)
        position += storedSize(i);
    return position;
}

/*!
    Returns the total size of all sections.

    \sa sectionSize()
*/
int QHeaderView::length() const {
    int total = 0;
    for (int i = 0; i < count(); ++i)
        total += sectionSize(i);
    return total;
}

/*!
    Returns the section at \a position, which is a position in the widget, along the header: the offset is taken into
    account. It is -1 if there is no section there.

    \sa sectionPosition()
*/
int QHeaderView::logicalIndexAt(int position) const {
    int start = -m_offset;
    for (int i = 0; i < count(); ++i) {
        const int end = start + sectionSize(i);
        if (position >= start && position < end)
            return i;
        start = end;
    }
    return -1;
}

/*!
    Resizes the section \a logicalIndex to \a size pixels, or to minimumSectionSize() if that is more. Emits
    sectionResized().

    \sa sectionSize()
*/
void QHeaderView::resizeSection(int logicalIndex, int size) {
    if (logicalIndex < 0 || logicalIndex >= count())
        return;
    size = std::max(size, m_minimumSize);
    const int old = storedSize(logicalIndex);
    if (old == size)
        return;
    m_sizes[static_cast<std::size_t>(logicalIndex)] = size;
    updateGeometry();
    update();
    emit sectionResized(logicalIndex, old, size);
}

/*!
    \fn int QHeaderView::offset() const

    Returns how far the header is scrolled, in pixels: the part of the first sections that is out of view.

    \sa setOffset()
*/

/*!
    Scrolls the header to \a offset pixels. A table keeps it equal to the scroll position of its contents.

    \sa offset()
*/
void QHeaderView::setOffset(int offset) {
    if (m_offset == offset)
        return;
    m_offset = offset;
    update();
}

/*!
    \fn int QHeaderView::defaultSectionSize() const

    Returns the size of the sections that are made from now on. It is 100 for a horizontal header and 30 for a vertical
    one, as the style says.

    \sa setDefaultSectionSize()
*/

/*!
    Sets the size of the sections that are made from now on to \a size. Sections that exist keep their size.

    \sa defaultSectionSize()
*/
void QHeaderView::setDefaultSectionSize(int size) { m_defaultSize = std::max(1, size); }

/*!
    \fn int QHeaderView::minimumSectionSize() const

    Returns the smallest size that a section can be resized to, 20 by default.

    \sa setMinimumSectionSize()
*/

/*!
    Sets the smallest size that a section can be resized to to \a size.

    \sa minimumSectionSize()
*/
void QHeaderView::setMinimumSectionSize(int size) { m_minimumSize = std::max(1, size); }

/*!
    \fn bool QHeaderView::stretchLastSection() const

    Returns whether the last section takes the room that the others leave. The default is \c false.

    \sa setStretchLastSection()
*/

/*!
    Sets whether the last section grows to fill the header to \a stretch.

    \sa stretchLastSection()
*/
void QHeaderView::setStretchLastSection(bool stretch) {
    if (m_stretchLast == stretch)
        return;
    m_stretchLast = stretch;
    updateGeometry();
    update();
}

/*!
    \fn bool QHeaderView::sectionsClickable() const

    Returns whether a click on a section is reported with sectionClicked(), and the section is drawn pressed while the
    button is down. The default is \c false for a header, and a table turns it on for its own.

    \sa setSectionsClickable()
*/

/*!
    Sets whether the sections can be clicked to \a clickable.

    \sa sectionsClickable()
*/
void QHeaderView::setSectionsClickable(bool clickable) { m_clickable = clickable; }

void QHeaderView::setCount(int newCount) {
    newCount = std::max(0, newCount);
    m_sizes.resize(static_cast<std::size_t>(newCount), m_defaultSize);
    updateGeometry();
    update();
}

void QHeaderView::insertSections(int logicalIndex, int number) {
    logicalIndex = std::clamp(logicalIndex, 0, count());
    m_sizes.insert(m_sizes.begin() + logicalIndex, static_cast<std::size_t>(number), m_defaultSize);
    if (logicalIndex < m_labels.size())
        m_labels.insert(logicalIndex, number, QString());
    updateGeometry();
    update();
}

void QHeaderView::removeSections(int logicalIndex, int number) {
    logicalIndex = std::clamp(logicalIndex, 0, count());
    number = std::min(number, count() - logicalIndex);
    m_sizes.erase(m_sizes.begin() + logicalIndex, m_sizes.begin() + logicalIndex + number);
    if (logicalIndex < m_labels.size())
        m_labels.remove(logicalIndex, std::min<qsizetype>(number, m_labels.size() - logicalIndex));
    updateGeometry();
    update();
}

void QHeaderView::setLabels(const QStringList &labels) {
    m_labels = labels;
    updateGeometry();
    update();
}

// The text of a section: the label that the table gave it, or its number if it has none.
QString QHeaderView::labelAt(int logicalIndex) const {
    if (logicalIndex < m_labels.size() && !m_labels.at(logicalIndex).isEmpty())
        return m_labels.at(logicalIndex);
    return QString::number(logicalIndex + 1);
}

/*!
    \reimp
*/
QSize QHeaderView::sizeHint() const {
    const QFontMetrics fm = fontMetrics();
    const int margin = style()->pixelMetric(QStyle::PM_HeaderMargin, nullptr, this);
    if (m_orientation == Qt::Horizontal)
        return QSize(length(), fm.height() + 2 * margin + 2);
    int widest = 0;
    for (int i = 0; i < count(); ++i)
        widest = std::max(widest, fm.horizontalAdvance(labelAt(i)));
    return QSize(widest + 2 * margin + 2, length());
}

// The section whose far edge is at `pos`, within a few pixels, so that dragging it resizes the section: -1 if none.
int QHeaderView::edgeAt(const QPoint &pos) const {
    const bool horizontal = m_orientation == Qt::Horizontal;
    const int along = horizontal ? pos.x() : pos.y();
    int end = -m_offset;
    for (int i = 0; i < count(); ++i) {
        end += sectionSize(i);
        if (m_stretchLast && i == count() - 1)
            break;
        if (std::abs(along - end) <= 2)
            return i;
    }
    return -1;
}

/*!
    \reimp
*/
void QHeaderView::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    painter.setFont(font());
    const bool horizontal = m_orientation == Qt::Horizontal;
    const int extent = horizontal ? width() : height();
    QStyleOptionHeader option;
    option.initFrom(this);
    option.orientation = m_orientation;
    int start = -m_offset;
    for (int i = 0; i < count() && start < extent; ++i) {
        const int size = sectionSize(i);
        if (start + size > 0) {
            option.rect = horizontal ? QRect(start, 0, size, height()) : QRect(0, start, width(), size);
            option.text = labelAt(i);
            option.lastSection = false;
            option.state &= ~QStyle::State(QStyle::State_Sunken);
            if (i == m_pressed)
                option.state |= QStyle::State_Sunken;
            style()->drawControl(QStyle::CE_Header, &option, &painter, this);
        }
        start += size;
    }
    // What is left after the last section looks like an empty section
    if (start < extent) {
        option.rect = horizontal ? QRect(start, 0, extent - start, height()) : QRect(0, start, width(), extent - start);
        option.text = QString();
        option.lastSection = true;
        option.state &= ~QStyle::State(QStyle::State_Sunken);
        style()->drawControl(QStyle::CE_Header, &option, &painter, this);
    }
}

/*!
    \reimp
*/
void QHeaderView::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    const int edge = edgeAt(event->pos());
    if (edge >= 0) {
        m_resizing = edge;
        m_resizeStart = m_orientation == Qt::Horizontal ? event->pos().x() : event->pos().y();
        m_resizeOriginal = storedSize(edge);
    } else if (m_clickable) {
        m_pressed = logicalIndexAt(m_orientation == Qt::Horizontal ? event->pos().x() : event->pos().y());
        update();
    }
    event->accept();
}

/*!
    \reimp
*/
void QHeaderView::mouseMoveEvent(QMouseEvent *event) {
    if (m_resizing < 0) {
        event->ignore();
        return;
    }
    const int along = m_orientation == Qt::Horizontal ? event->pos().x() : event->pos().y();
    resizeSection(m_resizing, m_resizeOriginal + along - m_resizeStart);
    event->accept();
}

/*!
    \reimp
*/
void QHeaderView::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    const int pressed = m_pressed;
    m_pressed = -1;
    m_resizing = -1;
    update();
    if (pressed >= 0 && pressed == logicalIndexAt(m_orientation == Qt::Horizontal ? event->pos().x() : event->pos().y()))
        emit sectionClicked(pressed);
    event->accept();
}

/*!
    \reimp
*/
void QHeaderView::resizeEvent(QResizeEvent *event) {
    (void)event;
    if (m_stretchLast)
        update();
}

#include "moc_qheaderview.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
