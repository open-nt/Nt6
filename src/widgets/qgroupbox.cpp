// MIT License - see LICENSE
#include <QtWidgets/qgroupbox.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>

/*!
    \class QGroupBox
    \brief The QGroupBox class is a frame with a title that groups other widgets.

    A group box draws a frame around its children, with a title on its top edge. Put a layout on it for the children,
    as for any widget. The radio buttons that are children of a group box are exclusive to each other, which is
    the way to have several groups of them in a window:

    \code
    QGroupBox *box = new QGroupBox("Size", window);
    QVBoxLayout *layout = new QVBoxLayout(box);
    layout->addWidget(new QRadioButton("Small", box));
    layout->addWidget(new QRadioButton("Large", box));
    \endcode

    A checkable group box has a check box in its title, and its children are disabled while it is unchecked.

    \differences
    The title is always at the top left: there is no \c setAlignment(), and no \c setFlat(). The children are enabled
    and disabled when the group box is checked and unchecked, but not when they are added later.
    \enddifferences

    \sa QRadioButton, QCheckBox
*/

/*!
    Constructs a group box with no title and the given \a parent.
*/
QGroupBox::QGroupBox(QWidget *parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

/*!
    Constructs a group box with the title \a title and the given \a parent.
*/
QGroupBox::QGroupBox(const QString &title, QWidget *parent) : QGroupBox(parent) { m_title = title; }

/*!
    Destroys the group box.
*/
QGroupBox::~QGroupBox() = default;

/*!
    \fn QString QGroupBox::title() const

    Returns the title.

    \sa setTitle()
*/

/*!
    Sets the title to \a title.

    \sa title()
*/
void QGroupBox::setTitle(const QString &title) {
    if (m_title == title)
        return;
    m_title = title;
    updateGeometry();
    update();
}

/*!
    \fn bool QGroupBox::isCheckable() const

    Returns whether the group box has a check box in its title. The default is \c false.

    \sa setCheckable(), isChecked()
*/

/*!
    Sets whether the group box has a check box in its title to \a checkable. A checkable group box can be
    given the keyboard focus.

    \sa isCheckable()
*/
void QGroupBox::setCheckable(bool checkable) {
    if (m_checkable == checkable)
        return;
    m_checkable = checkable;
    setFocusPolicy(checkable ? Qt::StrongFocus : Qt::NoFocus);
    applyChecked();
    updateGeometry();
    update();
}

/*!
    \fn bool QGroupBox::isChecked() const

    Returns whether a checkable group box is checked, which it is at first.

    \sa setChecked()
*/

/*!
    Checks a checkable group box if \a checked is \c true and unchecks it otherwise, which enables or disables its
    children. Emits toggled() if the state changes.

    \sa isChecked()
*/
void QGroupBox::setChecked(bool checked) {
    if (m_checked == checked)
        return;
    m_checked = checked;
    applyChecked();
    update();
    emit toggled(checked);
}

// The children are enabled while the group box is not checkable or is checked.
void QGroupBox::applyChecked() {
    const bool on = !m_checkable || m_checked;
    for (QObject *child : children())
        if (auto *w = dynamic_cast<QWidget *>(child))
            w->setEnabled(on);
}

QRect QGroupBox::titleRect() const {
    QStyleOptionGroupBox option;
    option.initFrom(this);
    option.text = m_title;
    option.checkable = m_checkable;
    return style()->subElementRect(QStyle::SE_GroupBoxTitle, &option, this);
}

/*!
    \reimp
*/
QSize QGroupBox::sizeHint() const {
    QSize size = QWidget::sizeHint();
    const int title = titleRect().width() + 16;
    if (!size.isValid())
        return QSize(title, fontMetrics().height() + 12);
    return QSize(std::max(size.width(), title), size.height());
}

/*!
    \reimp
*/
QSize QGroupBox::minimumSizeHint() const {
    QSize size = QWidget::minimumSizeHint();
    const int title = titleRect().width() + 16;
    if (!size.isValid())
        return QSize(title, fontMetrics().height() + 12);
    return QSize(std::max(size.width(), title), size.height());
}

/*!
    \reimp
*/
void QGroupBox::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    painter.setFont(font());
    QStyleOptionGroupBox option;
    option.initFrom(this);
    option.text = m_title;
    option.checkable = m_checkable;
    if (m_checkable) {
        option.state |= m_checked ? QStyle::State_On : QStyle::State_Off;
        if (m_down)
            option.state |= QStyle::State_Sunken;
    }
    style()->drawControl(QStyle::CE_GroupBox, &option, &painter, this);
}

/*!
    \reimp
*/
void QGroupBox::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || !m_checkable || !titleRect().contains(event->pos())) {
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
void QGroupBox::mouseReleaseEvent(QMouseEvent *event) {
    if (!m_down) {
        event->ignore();
        return;
    }
    m_down = false;
    update();
    if (titleRect().contains(event->pos())) {
        setChecked(!m_checked);
        emit clicked(m_checked);
    }
    event->accept();
}

/*!
    \reimp
*/
void QGroupBox::keyPressEvent(QKeyEvent *event) {
    if (m_checkable && event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        setChecked(!m_checked);
        emit clicked(m_checked);
        event->accept();
        return;
    }
    event->ignore();
}

/*!
    \reimp
*/
void QGroupBox::changeEvent(QEvent *event) {
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
        updateGeometry();
    update();
    QWidget::changeEvent(event);
}

#include "moc_qgroupbox.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
