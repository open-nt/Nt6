// MIT License - see LICENSE
#include <QtWidgets/qcombobox.h>

#include "qwidget_p.h"

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>

// The list that drops down from a combo box. It is a child of the window of the combo box, above its other
// children, and not a window of its own.
class QComboPopup : public QWidget {
public:
    QComboPopup(QComboBox *combo, QWidget *parent) : QWidget(parent), m_combo(combo) {
        setFocusPolicy(Qt::ClickFocus);
        setMouseTracking(true);
        hide();
    }

    int itemHeight() const { return fontMetrics().height() + 6; }
    int visibleItems() const { return std::min(m_combo->count(), m_visible); }

    void open() {
        QWidget *top = m_combo->window();
        if (parentWidget() != top)
            setParent(top);
        setFont(m_combo->font());
        setPalette(m_combo->palette());
        const int rows = std::max(1, std::min(m_combo->count(), m_combo->maxVisibleItems()));
        const QPoint below = m_combo->mapTo(top, QPoint(0, m_combo->height()));
        const int above = below.y() - m_combo->height();
        const int room = std::max(0, top->height() - below.y());
        // Open below the combo box, or above it if there is more room there.
        const bool downwards = rows * itemHeight() + 2 <= room || room >= above;
        m_visible = std::max(1, std::min(rows, ((downwards ? room : above) - 2) / itemHeight()));
        const int height = m_visible * itemHeight() + 2;
        setGeometry(QRect(QPoint(below.x(), downwards ? below.y() : above - height), QSize(m_combo->width(), height)));
        m_hover = m_combo->currentIndex();
        m_top = 0;
        ensureVisible(m_hover);
        raise();
        show();
        setFocus();
        QtPrivate::WidgetAccess::setPopup(this, this);
        m_combo->update();
    }

    void close() { hide(); }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setFont(font());
        const QPalette::ColorGroup group = QPalette::Active;
        painter.fillRect(rect(), palette().color(group, QPalette::Base));
        painter.setPen(palette().color(group, QPalette::Window).darker(150));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
        const int h = itemHeight();
        for (int row = 0; row < visibleItems(); ++row) {
            const int index = m_top + row;
            const QRect item(1, 1 + row * h, width() - 2, h);
            const bool hot = index == m_hover;
            if (hot)
                painter.fillRect(item, palette().color(group, QPalette::Highlight));
            painter.setPen(palette().color(group, hot ? QPalette::HighlightedText : QPalette::Text));
            painter.drawText(item.adjusted(7, 0, -4, 0), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                             m_combo->itemText(index));
        }
    }
    void mouseMoveEvent(QMouseEvent *event) override { hover(indexAt(event->pos())); }
    void mousePressEvent(QMouseEvent *event) override {
        hover(indexAt(event->pos()));
        event->accept();
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        const int index = indexAt(event->pos());
        if (index >= 0 && rect().contains(event->pos()))
            commit(index);
        event->accept();
    }
    void wheelEvent(QWheelEvent *event) override {
        const int notches = event->angleDelta().y() / 120;
        m_top = std::clamp(m_top - 3 * notches, 0, std::max(0, m_combo->count() - visibleItems()));
        update();
        event->accept();
    }
    void keyPressEvent(QKeyEvent *event) override {
        const int count = m_combo->count();
        switch (event->key()) {
        case Qt::Key_Down:
            hover(std::min(count - 1, m_hover + 1));
            break;
        case Qt::Key_Up:
            hover(std::max(0, m_hover - 1));
            break;
        case Qt::Key_PageDown:
            hover(std::min(count - 1, m_hover + visibleItems()));
            break;
        case Qt::Key_PageUp:
            hover(std::max(0, m_hover - visibleItems()));
            break;
        case Qt::Key_Home:
            hover(0);
            break;
        case Qt::Key_End:
            hover(count - 1);
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            if (m_hover >= 0)
                commit(m_hover);
            break;
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            close();
            break;
        default:
            event->ignore();
            return;
        }
        event->accept();
    }
    void hideEvent(QHideEvent *) override {
        QtPrivate::WidgetAccess::setPopup(this, nullptr);
        m_combo->setFocus();
        m_combo->update();
    }

private:
    int indexAt(const QPoint &pos) const {
        const int row = (pos.y() - 1) / itemHeight();
        const int index = m_top + row;
        return pos.y() >= 1 && row < visibleItems() && index < m_combo->count() ? index : -1;
    }
    void ensureVisible(int index) {
        if (index < 0)
            return;
        if (index < m_top)
            m_top = index;
        else if (index >= m_top + visibleItems())
            m_top = index - visibleItems() + 1;
    }
    void hover(int index) {
        if (index < 0 || index == m_hover)
            return;
        m_hover = index;
        ensureVisible(index);
        update();
        emit m_combo->highlighted(index);
    }
    void commit(int index) {
        close();
        m_combo->choose(index);
    }

    QComboBox *m_combo;
    int m_hover = -1;
    int m_top = 0;
    int m_visible = 1;
};

/*!
    \class QComboBox
    \brief The QComboBox class is a button that drops down a list of items to choose from.

    A combo box shows the current item. Clicking it opens a list in which the user chooses another one, and the
    arrow keys and the mouse wheel change it without opening the list. Connect to currentIndexChanged() or
    activated():

    \code
    QComboBox *font = new QComboBox(window);
    font->addItems({"Small", "Medium", "Large"});
    connect(font, &QComboBox::currentIndexChanged, [](int index) { qDebug() << index; });
    \endcode

    The list is a widget inside the window of the combo box, not a window of its own, so it never reaches outside
    the window. It opens below the combo box, or above it if there is more room there.

    Combo boxes: closed, disabled, and open, with the list below it.

    \image widgets/qcombobox.png QComboBox in the light color scheme

    \differences
    The items are plain texts: there are no icons, no item data (\c setItemData(), \c currentData()), no model or
    delegate, and no \c QComboBox::InsertPolicy. A combo box cannot be edited (\c setEditable()), and has no
    \c setPlaceholderText(), \c setMinimumContentsLength() or \c setSizeAdjustPolicy(). Typing does not jump to an item.
    \enddifferences

    \sa QSpinBox, QRadioButton
*/

/*!
    Constructs an empty combo box with the given \a parent.
*/
QComboBox::QComboBox(QWidget *parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

/*!
    Destroys the combo box.
*/
QComboBox::~QComboBox() { delete m_popup.data(); }

/*!
    \fn int QComboBox::count() const

    Returns the number of items.
*/

/*!
    Returns the text of the item at \a index, or an empty string if there is no such item.

    \sa setItemText()
*/
QString QComboBox::itemText(int index) const { return index >= 0 && index < count() ? m_items.at(index) : QString(); }

/*!
    Sets the text of the item at \a index to \a text. Nothing happens if there is no such item.

    \sa itemText()
*/
void QComboBox::setItemText(int index, const QString &text) {
    if (index < 0 || index >= count() || m_items.at(index) == text)
        return;
    m_items[static_cast<qsizetype>(index)] = text;
    updateGeometry();
    update();
    if (index == m_current)
        emit currentTextChanged(text);
}

/*!
    \fn int QComboBox::currentIndex() const

    Returns the index of the current item, or -1 if there is none. The first item added becomes the current one.

    \sa setCurrentIndex()
*/

/*!
    \fn QString QComboBox::currentText() const

    Returns the text of the current item, or an empty string if there is none.

    \sa setCurrentText()
*/

/*!
    Returns the index of the first item whose text is \a text, or -1 if there is none.
*/
int QComboBox::findText(const QString &text) const {
    for (int i = 0; i < count(); ++i)
        if (m_items.at(i) == text)
            return i;
    return -1;
}

/*!
    \fn int QComboBox::maxVisibleItems() const

    Returns how many items the list shows at most, 10 by default. A longer list scrolls.

    \sa setMaxVisibleItems()
*/

/*!
    Sets the number of items that the list shows at most to \a count.

    \sa maxVisibleItems()
*/
void QComboBox::setMaxVisibleItems(int count) { m_maxVisible = std::max(1, count); }

/*!
    Returns whether the list of items is open.

    \sa showPopup()
*/
bool QComboBox::isPopupVisible() const { return m_popup && !m_popup->isHidden(); }

/*!
    Adds an item with \a text at the end.

    \sa insertItem()
*/
void QComboBox::addItem(const QString &text) { insertItem(count(), text); }

/*!
    Adds an item for each of \a texts at the end.

    \sa addItem()
*/
void QComboBox::addItems(const QStringList &texts) {
    for (const QString &text : texts)
        addItem(text);
}

/*!
    Inserts an item with \a text at \a index, which is brought into the range of the items. The current item stays
    the same, except that the first item that is added becomes the current one.

    \sa addItem(), removeItem()
*/
void QComboBox::insertItem(int index, const QString &text) {
    const int at = std::clamp(index, 0, count());
    m_items.insert(at, text);
    updateGeometry();
    update();
    if (m_current < 0) {
        m_current = 0;
        emit currentIndexChanged(0);
        emit currentTextChanged(currentText());
    } else if (at <= m_current) {
        ++m_current;
    }
}

/*!
    Removes the item at \a index. If it was the current one, the item that takes its place becomes current.

    \sa clear(), insertItem()
*/
void QComboBox::removeItem(int index) {
    if (index < 0 || index >= count())
        return;
    m_items.removeAt(index);
    updateGeometry();
    update();
    if (index < m_current) {
        --m_current;
    } else if (index == m_current) {
        m_current = count() > 0 ? std::min(index, count() - 1) : -1;
        emit currentIndexChanged(m_current);
        emit currentTextChanged(currentText());
    }
}

/*!
    Removes all items.

    \sa removeItem()
*/
void QComboBox::clear() {
    hidePopup();
    if (m_items.isEmpty())
        return;
    m_items.clear();
    m_current = -1;
    updateGeometry();
    update();
    emit currentIndexChanged(-1);
    emit currentTextChanged(QString());
}

/*!
    Makes the item at \a index the current one. An \a index outside the items makes it -1, which shows nothing.
    Emits currentIndexChanged() and currentTextChanged() if the item changes.

    \sa currentIndex()
*/
void QComboBox::setCurrentIndex(int index) {
    const int target = index >= 0 && index < count() ? index : -1;
    if (target == m_current)
        return;
    m_current = target;
    update();
    emit currentIndexChanged(target);
    emit currentTextChanged(currentText());
}

/*!
    Makes the first item with \a text the current one. Nothing happens if there is none.

    \sa currentText(), findText()
*/
void QComboBox::setCurrentText(const QString &text) {
    const int index = findText(text);
    if (index >= 0)
        setCurrentIndex(index);
}

/*!
    Opens the list of items.

    \sa hidePopup()
*/
void QComboBox::showPopup() {
    if (count() == 0 || !isEnabled() || isPopupVisible() || !window())
        return;
    if (!m_popup)
        m_popup = new QComboPopup(this, window());
    static_cast<QComboPopup *>(m_popup.data())->open();
}

/*!
    Closes the list of items, if it is open.

    \sa showPopup()
*/
void QComboBox::hidePopup() {
    if (isPopupVisible())
        static_cast<QComboPopup *>(m_popup.data())->close();
}

// The user chose the item at `index`.
void QComboBox::choose(int index) {
    setCurrentIndex(index);
    emit activated(index);
}

/*!
    \reimp
*/
QSize QComboBox::sizeHint() const {
    const QFontMetrics fm = fontMetrics();
    int width = fm.horizontalAdvance(QString(7, QChar('x')));
    for (const QString &item : m_items)
        width = std::max(width, fm.horizontalAdvance(item));
    QStyleOptionComboBox option;
    option.initFrom(this);
    return style()->sizeFromContents(QStyle::CT_ComboBox, &option, QSize(width, fm.height()), this);
}

/*!
    \reimp
*/
QSize QComboBox::minimumSizeHint() const { return sizeHint(); }

/*!
    \reimp
*/
void QComboBox::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    painter.setFont(font());
    QStyleOptionComboBox option;
    option.initFrom(this);
    option.currentText = currentText();
    option.state |= m_down ? QStyle::State_Sunken : QStyle::State_Raised;
    if (isPopupVisible())
        option.state |= QStyle::State_On;
    style()->drawControl(QStyle::CE_ComboBox, &option, &painter, this);
}

/*!
    \reimp
*/
void QComboBox::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    m_down = true;
    update();
    event->accept();
}

/*!
    \reimp
*/
void QComboBox::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || !m_down) {
        event->ignore();
        return;
    }
    m_down = false;
    update();
    if (rect().contains(event->pos()))
        showPopup();
    event->accept();
}

/*!
    \reimp
*/
void QComboBox::wheelEvent(QWheelEvent *event) {
    const int notches = event->angleDelta().y() / 120;
    if (notches == 0 || count() == 0) {
        event->ignore();
        return;
    }
    const int index = std::clamp(m_current - notches, 0, count() - 1);
    if (index != m_current)
        choose(index);
    event->accept();
}

/*!
    \reimp
*/
void QComboBox::keyPressEvent(QKeyEvent *event) {
    const int key = event->key();
    int target = m_current;
    if (key == Qt::Key_Down && (event->modifiers() & Qt::AltModifier)) {
        showPopup();
        event->accept();
        return;
    }
    switch (key) {
    case Qt::Key_Up:
        target = m_current - 1;
        break;
    case Qt::Key_Down:
        target = m_current + 1;
        break;
    case Qt::Key_Home:
        target = 0;
        break;
    case Qt::Key_End:
        target = count() - 1;
        break;
    case Qt::Key_F4:
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        showPopup();
        event->accept();
        return;
    default:
        event->ignore();
        return;
    }
    if (target >= 0 && target < count() && target != m_current)
        choose(target);
    event->accept();
}

/*!
    \reimp
*/
void QComboBox::changeEvent(QEvent *event) {
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
        updateGeometry();
    update();
    QWidget::changeEvent(event);
}

#include "moc_qcombobox.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
