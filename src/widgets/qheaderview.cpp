// MIT License - see LICENSE
#include <QtWidgets/qheaderview.h>

#include <QtWidgets/qtooltip.h>

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

    A QTableWidget or QTableView has both headers, and gives them their sections and labels: use
    QTableView::horizontalHeader() and QTableView::verticalHeader() to reach them. The sizes of the sections of the
    headers are the widths of the columns and the heights of the rows; a hidden section is a hidden row or column (see
    hideSection()). QTreeView has a horizontal header for its columns.

    \code
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->resizeSection(0, 160);
    connect(table->horizontalHeader(), &QHeaderView::sectionClicked, [](int column) { qDebug() << column; });
    \endcode

    The headers of a table: the column labels, with the last section stretched, and the row labels.

    \image widgets/qheaderview.png QHeaderView in the light color scheme

    \differences
    There is no model: the sections and their labels come from the table that owns the header, so the count and the
    labels cannot be set on a header on its own. Sections cannot be moved, sorted or selected, and there are
    no resize modes (\c setSectionResizeMode()); every section but the last can be resized by the user, and the last
    one can be stretched. There are no logical and visual indexes: they are the same.
    \enddifferences

    \sa QTableView, QTableWidget, QTreeView
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

// The size that the section has as set; a stretched last section may be shown larger, a hidden one is not shown.
int QHeaderView::storedSize(int logicalIndex) const { return m_sizes[static_cast<std::size_t>(logicalIndex)]; }

// The last section that is not hidden, which is the one that stretchLastSection() stretches; -1 if there is none.
int QHeaderView::lastVisible() const {
    for (int i = count() - 1; i >= 0; --i) {
        if (!m_hidden[static_cast<std::size_t>(i)])
            return i;
    }
    return -1;
}

/*!
    Returns the size of the section \a logicalIndex: its width in a horizontal header, its height in a vertical one. A
    last section that is stretched can be larger than it was set. The size of a section that is hidden or does not
    exist is 0.

    \sa resizeSection(), stretchLastSection()
*/
int QHeaderView::sectionSize(int logicalIndex) const {
    if (logicalIndex < 0 || logicalIndex >= count() || m_hidden[static_cast<std::size_t>(logicalIndex)])
        return 0;
    if (stretchSectionCount() > 0)
        return effectiveSizes()[static_cast<std::size_t>(logicalIndex)];
    const int size = storedSize(logicalIndex);
    if (m_stretchLast && logicalIndex == lastVisible()) {
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
    if (stretchSectionCount() > 0) {
        const std::vector<int> sizes = effectiveSizes();
        for (int i = 0; i < std::min(logicalIndex, count()); ++i)
            position += sizes[static_cast<std::size_t>(i)];
        return position;
    }
    for (int i = 0; i < std::min(logicalIndex, count()); ++i) {
        if (!m_hidden[static_cast<std::size_t>(i)])
            position += storedSize(i);
    }
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
    sectionResized(). A hidden section keeps the size for when it is shown again.

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
    if (m_hidden[static_cast<std::size_t>(logicalIndex)])
        return; // it has that size when it is shown again
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

/*!
    Returns \c true if the section \a logicalIndex is hidden; otherwise returns \c false.

    \sa setSectionHidden(), hiddenSectionCount()
*/
bool QHeaderView::isSectionHidden(int logicalIndex) const {
    return logicalIndex >= 0 && logicalIndex < count() && m_hidden[static_cast<std::size_t>(logicalIndex)];
}

/*!
    Hides the section \a logicalIndex if \a hide is \c true, and shows it again otherwise. A hidden section takes no
    room; it keeps its size for when it is shown. sectionResized() reports the change, from or to a size of 0.

    \sa hideSection(), showSection(), isSectionHidden()
*/
void QHeaderView::setSectionHidden(int logicalIndex, bool hide) {
    if (logicalIndex < 0 || logicalIndex >= count() || isSectionHidden(logicalIndex) == hide)
        return;
    const int size = storedSize(logicalIndex);
    m_hidden[static_cast<std::size_t>(logicalIndex)] = hide ? 1 : 0;
    updateGeometry();
    update();
    emit sectionResized(logicalIndex, hide ? size : 0, hide ? 0 : size);
}

/*!
    \fn void QHeaderView::hideSection(int logicalIndex)

    Hides the section \a logicalIndex.

    \sa showSection(), setSectionHidden()
*/

/*!
    \fn void QHeaderView::showSection(int logicalIndex)

    Shows the section \a logicalIndex again.

    \sa hideSection(), setSectionHidden()
*/

/*!
    Returns the number of sections that are hidden.

    \sa isSectionHidden()
*/
int QHeaderView::hiddenSectionCount() const {
    return static_cast<int>(std::count(m_hidden.begin(), m_hidden.end(), static_cast<char>(1)));
}

/*!
    \enum QHeaderView::ResizeMode
    \since 6.0

    This enum tells how the size of a section is decided.

    \value Interactive The user resizes the section by dragging its edge, and resizeSection() sets its size. This is
           the default.
    \value Stretch The sections in this mode share the space that the other sections leave, and the user cannot
           resize them.
    \value Custom The size is set with resizeSection(); the user cannot resize the section.
    \value ResizeToContents The section is as large as the contents of its column or row need, measured when the
           view lays its items out; the user cannot resize it.
    \value Fixed The same as Custom.
*/

/*!
    \since 6.0

    Sets the resize mode of every section, and of the sections added later, to \a mode.

    \sa sectionResizeMode()
*/
void QHeaderView::setSectionResizeMode(QHeaderView::ResizeMode mode) {
    m_defaultMode = mode;
    std::fill(m_modes.begin(), m_modes.end(), static_cast<char>(mode));
    updateGeometry();
    update();
}

/*!
    \overload
    \since 6.0

    Sets the resize mode of the section \a logicalIndex to \a mode.
*/
void QHeaderView::setSectionResizeMode(int logicalIndex, QHeaderView::ResizeMode mode) {
    if (logicalIndex < 0 || logicalIndex >= count())
        return;
    m_modes[static_cast<std::size_t>(logicalIndex)] = static_cast<char>(mode);
    updateGeometry();
    update();
}

/*!
    \since 6.0

    Returns the resize mode of the section \a logicalIndex; Interactive for a section that does not exist.

    \sa setSectionResizeMode()
*/
QHeaderView::ResizeMode QHeaderView::sectionResizeMode(int logicalIndex) const {
    if (logicalIndex < 0 || logicalIndex >= count())
        return Interactive;
    return static_cast<ResizeMode>(m_modes[static_cast<std::size_t>(logicalIndex)]);
}

/*!
    \since 6.0

    Returns the number of sections that are shown in Stretch mode.
*/
int QHeaderView::stretchSectionCount() const {
    int n = 0;
    for (std::size_t i = 0; i < m_modes.size(); ++i)
        n += m_modes[i] == static_cast<char>(Stretch) && !m_hidden[i] ? 1 : 0;
    return n;
}

std::vector<int> QHeaderView::effectiveSizes() const {
    std::vector<int> sizes(m_sizes.size(), 0);
    int fixed = 0, stretch = 0;
    for (std::size_t i = 0; i < m_sizes.size(); ++i) {
        if (m_hidden[i])
            continue;
        if (m_modes[i] == static_cast<char>(Stretch)) {
            ++stretch;
        } else {
            sizes[i] = m_sizes[i];
            fixed += m_sizes[i];
        }
    }
    const int available = (m_orientation == Qt::Horizontal ? width() : height()) - fixed;
    int share = stretch > 0 ? std::max(m_minimumSize, available / stretch) : 0;
    int left = stretch > 0 ? std::max(available, share * stretch) - share * stretch : 0; // spread the remainder
    for (std::size_t i = 0; i < m_sizes.size(); ++i) {
        if (m_hidden[i] || m_modes[i] != static_cast<char>(Stretch))
            continue;
        sizes[i] = share + (left > 0 ? 1 : 0);
        left = std::max(0, left - 1);
    }
    return sizes;
}

std::vector<int> QHeaderView::contentsSections() const {
    std::vector<int> out;
    for (std::size_t i = 0; i < m_modes.size(); ++i)
        if (m_modes[i] == static_cast<char>(ResizeToContents) && !m_hidden[i])
            out.push_back(static_cast<int>(i));
    return out;
}

void QHeaderView::setCount(int newCount) {
    newCount = std::max(0, newCount);
    m_sizes.resize(static_cast<std::size_t>(newCount), m_defaultSize);
    m_hidden.resize(static_cast<std::size_t>(newCount), 0);
    m_modes.resize(static_cast<std::size_t>(newCount), static_cast<char>(m_defaultMode));
    updateGeometry();
    update();
}

void QHeaderView::insertSections(int logicalIndex, int number) {
    logicalIndex = std::clamp(logicalIndex, 0, count());
    m_sizes.insert(m_sizes.begin() + logicalIndex, static_cast<std::size_t>(number), m_defaultSize);
    m_hidden.insert(m_hidden.begin() + logicalIndex, static_cast<std::size_t>(number), 0);
    m_modes.insert(m_modes.begin() + logicalIndex, static_cast<std::size_t>(number), static_cast<char>(m_defaultMode));
    if (logicalIndex < m_labels.size())
        m_labels.insert(logicalIndex, number, QString());
    updateGeometry();
    update();
}

void QHeaderView::removeSections(int logicalIndex, int number) {
    logicalIndex = std::clamp(logicalIndex, 0, count());
    number = std::min(number, count() - logicalIndex);
    m_sizes.erase(m_sizes.begin() + logicalIndex, m_sizes.begin() + logicalIndex + number);
    m_hidden.erase(m_hidden.begin() + logicalIndex, m_hidden.begin() + logicalIndex + number);
    m_modes.erase(m_modes.begin() + logicalIndex, m_modes.begin() + logicalIndex + number);
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
    const int last = lastVisible();
    for (int i = 0; i < count(); ++i) {
        if (isSectionHidden(i))
            continue;
        end += sectionSize(i);
        if (m_stretchLast && i == last)
            break;
        if (std::abs(along - end) <= 2 && sectionResizeMode(i) == Interactive)
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
        if (size > 0 && start + size > 0) {
            option.rect = horizontal ? QRect(start, 0, size, height()) : QRect(0, start, width(), size);
            option.text = labelAt(i);
            option.lastSection = false;
            option.sortIndicator = !m_sortShown || i != m_sortSection  ? QStyleOptionHeader::None
                                   : m_sortOrder == Qt::AscendingOrder ? QStyleOptionHeader::SortDown
                                                                       : QStyleOptionHeader::SortUp;
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
        option.sortIndicator = QStyleOptionHeader::None;
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
    if (pressed >= 0 &&
        pressed == logicalIndexAt(m_orientation == Qt::Horizontal ? event->pos().x() : event->pos().y())) {
        // A click on the section that is sorted by turns the order round; on another one, sorts by that one.
        if (m_sortShown && m_clickable) {
            setSortIndicator(pressed, pressed == m_sortSection && m_sortOrder == Qt::AscendingOrder
                                          ? Qt::DescendingOrder
                                          : Qt::AscendingOrder);
        }
        emit sectionClicked(pressed);
    }
    event->accept();
}

/*!
    \fn bool QHeaderView::isSortIndicatorShown() const

    Returns whether the header shows which section its view is sorted by. The default is \c false.

    \sa setSortIndicatorShown()
*/

/*!
    Sets whether the section that the view is sorted by has an arrow for the sort order to \a show. With the
    arrow shown and sectionsClickable() on, a click on a section moves the arrow to it, or turns it round, and
    sortIndicatorChanged() tells.

    \sa isSortIndicatorShown(), setSortIndicator()
*/
void QHeaderView::setSortIndicatorShown(bool show) {
    if (m_sortShown == show)
        return;
    m_sortShown = show;
    update();
}

/*!
    Makes the section \a logicalIndex the one that shows the sort indicator, for the order \a order, and emits
    sortIndicatorChanged() if that is a change. A \a logicalIndex of -1 means that the view is not sorted.

    \sa sortIndicatorSection(), sortIndicatorOrder()
*/
void QHeaderView::setSortIndicator(int logicalIndex, Qt::SortOrder order) {
    if (logicalIndex < -1 || logicalIndex >= count())
        return;
    if (m_sortSection == logicalIndex && m_sortOrder == order)
        return;
    m_sortSection = logicalIndex;
    m_sortOrder = order;
    update();
    emit sortIndicatorChanged(logicalIndex, order);
}

/*!
    \fn int QHeaderView::sortIndicatorSection() const

    Returns the section that shows the sort indicator, or -1 if none does.

    \sa setSortIndicator()
*/

/*!
    \fn Qt::SortOrder QHeaderView::sortIndicatorOrder() const

    Returns the order that the sort indicator shows.

    \sa setSortIndicator()
*/

/*!
    \reimp
*/
void QHeaderView::resizeEvent(QResizeEvent *event) {
    (void)event;
    if (m_stretchLast || stretchSectionCount() > 0)
        update();
}

/*!
    \reimp
    \since 6.0

    Shows the tool tip of the section under the mouse for a QEvent::ToolTip \a event: the tool tip of the header
    item of a QTableWidget. Other events go to the base class.
*/
bool QHeaderView::event(QEvent *event) {
    if (event->type() == QEvent::ToolTip) {
        auto *help = static_cast<QHelpEvent *>(event);
        const int section = logicalIndexAt(m_orientation == Qt::Horizontal ? help->pos().x() : help->pos().y());
        if (section >= 0 && section < m_toolTips.size() && !m_toolTips.at(section).isEmpty()) {
            QToolTip::showText(help->globalPos(), m_toolTips.at(section), this);
            return true;
        }
        QToolTip::hideText();
        event->ignore();
        return true;
    }
    return QWidget::event(event);
}

#include "moc_qheaderview.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
