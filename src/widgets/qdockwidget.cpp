// MIT License - see LICENSE
#include <QtWidgets/qdockwidget.h>

#include <QtGui/qaction.h>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qmainwindow.h>
#include <QtWidgets/qtoolbutton.h>

#include <algorithm>

namespace {

constexpr int kFrame = 1;       // the frame around a dock widget
constexpr int kTitleMargin = 3; // around the text of the title bar

} // namespace

/*!
    \class QDockWidget
    \brief The QDockWidget class provides a panel that sits at a side of a QMainWindow or floats as a window.

    A dock widget has a title bar and a widget. Put it in a main window with QMainWindow::addDockWidget():

    \code
    auto *dock = new QDockWidget("Files", window);
    dock->setWidget(new QListWidget);
    window->addDockWidget(Qt::LeftDockWidgetArea, dock);
    \endcode

    The title bar has a button that closes (hides) the dock widget and one that makes it float as a window of its own
    above the main window; the features say which buttons there are. Double-clicking the title bar, or
    setFloating(), docks a floating dock widget again. toggleViewAction() is a checkable action that shows and hides
    the dock widget.

    \differences
    A dock widget cannot be dragged to another side of the window or into a floating position; setFloating() and the
    title bar move it. A vertical title bar (\c DockWidgetVerticalTitleBar) is drawn horizontally. The title bar
    buttons show "x" and "+" instead of icons.
    \enddifferences

    \sa QMainWindow
*/

/*!
    \enum QDockWidget::DockWidgetFeature

    What the user can do with a dock widget. DockWidgetFeatures is a typedef for \c{QFlags<DockWidgetFeature>}.

    \value DockWidgetClosable The title bar has a button that closes the dock widget.
    \value DockWidgetMovable The dock widget may be moved; recorded only in Nt.
    \value DockWidgetFloatable The title bar has a button that makes the dock widget float.
    \value DockWidgetVerticalTitleBar The title bar is on the left; recorded only in Nt.
    \value DockWidgetFeatureMask All of the above.
    \value NoDockWidgetFeatures None.
*/

/*!
    Constructs a dock widget with the given window \a title, \a parent and window \a flags.
*/
QDockWidget::QDockWidget(const QString &title, QWidget *parent, Qt::WindowFlags flags) : QWidget(parent, flags) {
    init(title);
}

/*!
    Constructs a dock widget with the given \a parent and window \a flags.
*/
QDockWidget::QDockWidget(QWidget *parent, Qt::WindowFlags flags) : QWidget(parent, flags) { init(QString()); }

/*!
    Destroys the dock widget.
*/
QDockWidget::~QDockWidget() = default;

void QDockWidget::init(const QString &title) {
    m_closeButton = new QToolButton(this);
    m_closeButton->setText(QStringLiteral("x"));
    m_closeButton->setAutoRaise(true);
    m_closeButton->setToolTip(QStringLiteral("Close"));
    m_floatButton = new QToolButton(this);
    m_floatButton->setText(QStringLiteral("+"));
    m_floatButton->setAutoRaise(true);
    m_floatButton->setToolTip(QStringLiteral("Float"));
    QObject::connect(m_closeButton, &QToolButton::clicked, this, [this] { close(); });
    QObject::connect(m_floatButton, &QToolButton::clicked, this, [this] { setFloating(!isFloating()); });
    m_toggleView = new QAction(title, this);
    m_toggleView->setCheckable(true);
    m_toggleView->setChecked(!isHidden());
    QObject::connect(m_toggleView, &QAction::triggered, this, [this](bool checked) {
        setVisible(checked);
        if (checked && isWindow())
            raise();
    });
    QObject::connect(this, &QWidget::windowTitleChanged, m_toggleView, &QAction::setText);
    setWindowTitle(title);
    updateButtons();
}

/*!
    \fn QWidget *QDockWidget::widget() const

    Returns the widget of the dock widget, or \c nullptr.

    \sa setWidget()
*/

/*!
    Makes \a widget the widget of the dock widget, under the title bar. The dock widget takes it as a child; a widget
    it had before stays a child, hidden.

    \sa widget()
*/
void QDockWidget::setWidget(QWidget *widget) {
    if (m_widget == widget)
        return;
    if (m_widget)
        m_widget->hide();
    m_widget = widget;
    if (widget) {
        if (widget->parentWidget() != this)
            widget->setParent(this);
        widget->show();
    }
    layoutParts();
    updateGeometry();
}

/*!
    Sets what the user can do with the dock widget to \a features.

    \sa features()
*/
void QDockWidget::setFeatures(DockWidgetFeatures features) {
    features &= DockWidgetFeatureMask;
    if (m_features == features)
        return;
    m_features = features;
    updateButtons();
    emit featuresChanged(features);
}

/*!
    \fn QDockWidget::DockWidgetFeatures QDockWidget::features() const

    Returns what the user can do with the dock widget: by default close, move and float it.

    \sa setFeatures()
*/

/*!
    Makes the dock widget float as a window above its main window if \a floating is \c true, and docks it again
    otherwise. A floating dock widget keeps its size and is placed where it was on the screen.

    \sa isFloating(), topLevelChanged()
*/
void QDockWidget::setFloating(bool floating) {
    if (floating == isFloating() || !parentWidget())
        return;
    const bool shown = !isHidden();
    const QRect onScreen(mapToGlobal(QPoint(0, 0)), size());
    setWindowFlags(floating ? Qt::Tool : Qt::Widget);
    if (floating)
        setGeometry(onScreen);
    if (shown)
        show();
    updateButtons();
    layoutParts();
    if (QWidget *p = parentWidget())
        p->updateGeometry();
    if (auto *window = dynamic_cast<QMainWindow *>(parentWidget()))
        window->relayout();
    emit topLevelChanged(floating);
}

/*!
    \fn bool QDockWidget::isFloating() const

    Returns whether the dock widget floats as a window.

    \sa setFloating()
*/

/*!
    Sets the areas of a QMainWindow that the dock widget may go in to \a areas.

    \sa allowedAreas(), isAreaAllowed()
*/
void QDockWidget::setAllowedAreas(Qt::DockWidgetAreas areas) {
    areas &= Qt::AllDockWidgetAreas;
    if (m_allowedAreas == areas)
        return;
    m_allowedAreas = areas;
    emit allowedAreasChanged(areas);
}

/*!
    \fn Qt::DockWidgetAreas QDockWidget::allowedAreas() const

    Returns the areas of a QMainWindow that the dock widget may go in, Qt::AllDockWidgetAreas by default.

    \sa setAllowedAreas()
*/

/*!
    \fn bool QDockWidget::isAreaAllowed(Qt::DockWidgetArea area) const

    Returns whether the dock widget may go in \a area.
*/

/*!
    Replaces the title bar with \a widget, or brings back the standard title bar if it is \c nullptr. The dock widget
    takes the widget as a child.

    \sa titleBarWidget()
*/
void QDockWidget::setTitleBarWidget(QWidget *widget) {
    if (m_customTitle == widget)
        return;
    if (m_customTitle)
        m_customTitle->hide();
    m_customTitle = widget;
    if (widget) {
        if (widget->parentWidget() != this)
            widget->setParent(this);
        widget->show();
    }
    updateButtons();
    layoutParts();
    updateGeometry();
    update();
}

/*!
    \fn QWidget *QDockWidget::titleBarWidget() const

    Returns the widget that replaces the title bar, or \c nullptr.

    \sa setTitleBarWidget()
*/

/*!
    \fn QAction *QDockWidget::toggleViewAction() const

    Returns a checkable action that shows and hides the dock widget, with its window title as the text.
*/

int QDockWidget::titleHeight() const {
    if (m_customTitle)
        return m_customTitle->sizeHint().height();
    return std::max(fontMetrics().height() + 2 * kTitleMargin, m_closeButton->sizeHint().height());
}

QRect QDockWidget::titleRect() const { return QRect(kFrame, kFrame, width() - 2 * kFrame, titleHeight()); }

void QDockWidget::layoutParts() {
    if (!m_closeButton)
        return;
    const QRect title = titleRect();
    if (m_customTitle)
        m_customTitle->setGeometry(title);
    const int side = title.height();
    int right = title.right() + 1;
    for (QToolButton *button : {m_closeButton, m_floatButton}) {
        if (button->isHidden())
            continue;
        right -= side;
        button->setGeometry(right, title.top(), side, side);
    }
    if (m_widget)
        m_widget->setGeometry(QRect(kFrame, title.bottom() + 1, width() - 2 * kFrame,
                                    std::max(0, height() - title.height() - 2 * kFrame)));
}

void QDockWidget::updateButtons() {
    m_closeButton->setVisible(!m_customTitle && m_features.testFlag(DockWidgetClosable));
    m_floatButton->setVisible(!m_customTitle && m_features.testFlag(DockWidgetFloatable));
    m_floatButton->setToolTip(isFloating() ? QStringLiteral("Dock") : QStringLiteral("Float"));
    layoutParts();
}

/*!
    \reimp
*/
QSize QDockWidget::sizeHint() const {
    const QSize content = m_widget ? m_widget->sizeHint().expandedTo(m_widget->minimumSizeHint()) : QSize(0, 0);
    const int titleWidth = fontMetrics().horizontalAdvance(windowTitle()) + 2 * kTitleMargin + 2 * titleHeight();
    return QSize(std::max(content.width(), titleWidth) + 2 * kFrame, content.height() + titleHeight() + 2 * kFrame);
}

/*!
    \reimp
*/
QSize QDockWidget::minimumSizeHint() const {
    const QSize content = m_widget ? m_widget->minimumSizeHint() : QSize(0, 0);
    return QSize(std::max(content.width(), 2 * titleHeight()) + 2 * kFrame,
                 std::max(0, content.height()) + titleHeight() + 2 * kFrame);
}

/*!
    \reimp
*/
void QDockWidget::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    painter.setPen(palette().color(group, QPalette::Mid));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
    if (m_customTitle)
        return;
    const QRect title = titleRect();
    painter.fillRect(title, palette().color(group, QPalette::Button));
    painter.setPen(palette().color(group, QPalette::ButtonText));
    int right = title.right();
    for (QToolButton *button : {m_closeButton, m_floatButton})
        if (!button->isHidden())
            right -= title.height();
    painter.drawText(
        QRect(title.left() + kTitleMargin, title.top(), right - title.left() - kTitleMargin, title.height()),
        Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, windowTitle());
}

/*!
    \reimp
*/
void QDockWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    layoutParts();
}

/*!
    \reimp

    Double-clicking the title bar of a floatable dock widget makes it float, or docks it again.
*/
void QDockWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && titleRect().contains(event->pos()) && !m_customTitle &&
        m_features.testFlag(DockWidgetFloatable)) {
        setFloating(!isFloating());
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

/*!
    \reimp

    A dock widget that is not closable ignores the close.
*/
void QDockWidget::closeEvent(QCloseEvent *event) {
    if (!m_features.testFlag(DockWidgetClosable)) {
        event->ignore();
        return;
    }
    QWidget::closeEvent(event);
    if (!isWindow())
        hide();
}

/*!
    \reimp
*/
void QDockWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    m_toggleView->setChecked(true);
    emit visibilityChanged(true);
}

/*!
    \reimp
*/
void QDockWidget::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    if (isHidden())
        m_toggleView->setChecked(false);
    emit visibilityChanged(false);
}

/*!
    \reimp
*/
void QDockWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::WindowTitleChange || event->type() == QEvent::FontChange) {
        updateGeometry();
        layoutParts();
        update();
    }
    QWidget::changeEvent(event);
}

#include "moc_qdockwidget.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
