// MIT License - see LICENSE
#include <QtWidgets/qtoolbox.h>

#include <QtGui/qpainter.h>
#include <QtWidgets/qabstractbutton.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>

namespace {

// The bar of an item: its text, bold when the item is current.
class ToolBoxButton : public QAbstractButton {
public:
    explicit ToolBoxButton(QWidget *parent) : QAbstractButton(parent) {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        setFocusPolicy(Qt::TabFocus);
    }

    void setCurrent(bool current) {
        m_current = current;
        update();
    }

    QSize sizeHint() const override {
        QStyleOptionButton option;
        option.initFrom(this);
        const QSize text = fontMetrics().size(Qt::TextSingleLine, this->text());
        const QSize s = style()->sizeFromContents(QStyle::CT_PushButton, &option, text, this);
        return QSize(text.width() + 16, s.height());
    }
    QSize minimumSizeHint() const override { return QSize(0, sizeHint().height()); }

protected:
    void paintEvent(QPaintEvent *event) override {
        (void)event;
        QPainter painter(this);
        QStyleOptionButton option;
        option.initFrom(this);
        option.state |= isDown() ? QStyle::State_Sunken : QStyle::State_Raised;
        style()->drawPrimitive(QStyle::PE_PanelButtonCommand, &option, &painter, this);
        QFont f = font();
        f.setBold(m_current);
        painter.setFont(f);
        const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
        painter.setPen(palette().color(group, QPalette::ButtonText));
        painter.drawText(rect().adjusted(8, 0, -4, 0), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text());
    }

private:
    bool m_current = false;
};

} // namespace

/*!
    \class QToolBox
    \brief The QToolBox class provides a column of items, one of which shows its widget at a time.

    Each item has a bar with a text; clicking a bar opens its item, whose widget fills the room under the bar, and
    closes the one that was open:

    \code
    QToolBox *box = new QToolBox(window);
    box->addItem(new QLabel("Brushes"), "Brushes");
    box->addItem(new QLabel("Colors"), "Colors");
    connect(box, &QToolBox::currentChanged, [](int index) { qDebug() << index; });
    \endcode

    \differences
    The items have no icons (\c setItemIcon()), and the change of item is not animated.
    \enddifferences

    \sa QTabWidget, QStackedWidget
*/

/*!
    Constructs an empty tool box with the given \a parent and window flags \a f.
*/
QToolBox::QToolBox(QWidget *parent, Qt::WindowFlags f) : QFrame(parent) {
    if (f & Qt::Window)
        setWindowFlags(f);
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
}

/*!
    Destroys the tool box and its widgets.
*/
QToolBox::~QToolBox() {
    for (const Item &item : m_items)
        QObject::disconnect(item.page, &QObject::destroyed, this, nullptr);
}

/*!
    \fn int QToolBox::addItem(QWidget *widget, const QString &text)

    Adds an item with the given \a widget and \a text at the end and returns its index.
*/

/*!
    Inserts an item with the given \a widget and \a text at \a index, or at the end if \a index is out of range, and
    returns its index. The tool box takes the widget as a child. The first item becomes current.

    \sa removeItem()
*/
int QToolBox::insertItem(int index, QWidget *widget, const QString &text) {
    if (!widget)
        return -1;
    const int at = index >= 0 && index <= count() ? index : count();
    auto *button = new ToolBoxButton(this);
    button->setText(text);
    widget->setParent(this);
    widget->hide();
    m_items.insert(m_items.begin() + at, Item{button, widget});
    QObject::connect(button, &QAbstractButton::clicked, this, [this, button] {
        for (int i = 0; i < count(); ++i)
            if (m_items[static_cast<std::size_t>(i)].button == button)
                setCurrentIndex(i);
    });
    QObject::connect(widget, &QObject::destroyed, this, [this](QObject *page) { pageGone(page); });
    if (m_current >= at)
        ++m_current;
    relayout();
    itemInserted(at);
    if (m_current < 0)
        setCurrentIndex(at);
    return at;
}

/*!
    Removes the item at \a index. The widget is not deleted; it stays a child of the tool box, hidden.

    \sa insertItem()
*/
void QToolBox::removeItem(int index) {
    if (index < 0 || index >= count())
        return;
    QWidget *page = m_items[static_cast<std::size_t>(index)].page;
    QObject::disconnect(page, &QObject::destroyed, this, nullptr);
    pageGone(page);
    page->hide();
}

// Takes the item of `page` out, after the page is deleted or removed.
void QToolBox::pageGone(QObject *page) {
    for (int i = 0; i < count(); ++i) {
        Item &item = m_items[static_cast<std::size_t>(i)];
        if (static_cast<QObject *>(item.page) != page)
            continue;
        delete item.button;
        m_items.erase(m_items.begin() + i);
        if (i < m_current) {
            --m_current;
        } else if (i == m_current) {
            m_current = -1;
            if (count() > 0) {
                setCurrentIndex(std::min(i, count() - 1));
            } else {
                emit currentChanged(-1);
            }
        }
        relayout();
        itemRemoved(i);
        return;
    }
}

/*!
    Enables the item at \a index if \a enabled is \c true, and disables it otherwise. A disabled item cannot be opened
    by the user; if it was current, the next enabled item becomes current.
*/
void QToolBox::setItemEnabled(int index, bool enabled) {
    if (index < 0 || index >= count())
        return;
    m_items[static_cast<std::size_t>(index)].button->setEnabled(enabled);
    if (!enabled && index == m_current) {
        for (int i = 1; i < count(); ++i) {
            const int other = (index + i) % count();
            if (isItemEnabled(other)) {
                setCurrentIndex(other);
                break;
            }
        }
    }
}

/*!
    Returns whether the item at \a index is enabled.
*/
bool QToolBox::isItemEnabled(int index) const {
    return index >= 0 && index < count() && m_items[static_cast<std::size_t>(index)].button->isEnabled();
}

/*!
    Sets the text of the item at \a index to \a text.
*/
void QToolBox::setItemText(int index, const QString &text) {
    if (index >= 0 && index < count())
        m_items[static_cast<std::size_t>(index)].button->setText(text);
}

/*!
    Returns the text of the item at \a index.
*/
QString QToolBox::itemText(int index) const {
    return index >= 0 && index < count() ? m_items[static_cast<std::size_t>(index)].button->text() : QString();
}

/*!
    Sets the tool tip of the bar of the item at \a index to \a toolTip.
*/
void QToolBox::setItemToolTip(int index, const QString &toolTip) {
    if (index >= 0 && index < count())
        m_items[static_cast<std::size_t>(index)].button->setToolTip(toolTip);
}

/*!
    Returns the tool tip of the item at \a index.
*/
QString QToolBox::itemToolTip(int index) const {
    return index >= 0 && index < count() ? m_items[static_cast<std::size_t>(index)].button->toolTip() : QString();
}

/*!
    \fn int QToolBox::currentIndex() const

    Returns the index of the current item, or -1 if there are no items.
*/

/*!
    \fn QWidget *QToolBox::currentWidget() const

    Returns the widget of the current item, or \c nullptr.
*/

/*!
    Returns the widget of the item at \a index, or \c nullptr.
*/
QWidget *QToolBox::widget(int index) const {
    return index >= 0 && index < count() ? m_items[static_cast<std::size_t>(index)].page : nullptr;
}

/*!
    Returns the index of the item of \a widget, or -1.
*/
int QToolBox::indexOf(const QWidget *widget) const {
    for (int i = 0; i < count(); ++i)
        if (m_items[static_cast<std::size_t>(i)].page == widget)
            return i;
    return -1;
}

/*!
    \fn int QToolBox::count() const

    Returns the number of items.
*/

/*!
    Opens the item at \a index and emits currentChanged(), if it is another item.
*/
void QToolBox::setCurrentIndex(int index) {
    if (index < 0 || index >= count() || index == m_current)
        return;
    if (QWidget *old = widget(m_current))
        old->hide();
    m_current = index;
    relayout();
    emit currentChanged(index);
}

/*!
    Opens the item of \a widget.
*/
void QToolBox::setCurrentWidget(QWidget *widget) { setCurrentIndex(indexOf(widget)); }

/*!
    Is called after an item is inserted at \a index. The default implementation does nothing.
*/
void QToolBox::itemInserted(int index) { (void)index; }

/*!
    Is called after the item at \a index is removed. The default implementation does nothing.
*/
void QToolBox::itemRemoved(int index) { (void)index; }

// The bars of all the items, and the widget of the current one under its bar, taking the room that is left.
void QToolBox::relayout() {
    while (QLayoutItem *item = m_layout->takeAt(0))
        delete item;
    for (int i = 0; i < count(); ++i) {
        const Item &item = m_items[static_cast<std::size_t>(i)];
        static_cast<ToolBoxButton *>(item.button)->setCurrent(i == m_current);
        m_layout->addWidget(item.button);
        if (i == m_current) {
            m_layout->addWidget(item.page, 1);
            item.page->show();
        } else {
            item.page->hide();
        }
    }
    if (m_current < 0)
        m_layout->addStretch(1);
    updateGeometry();
}

#include "moc_qtoolbox.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
