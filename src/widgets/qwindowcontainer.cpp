// MIT License - see LICENSE
//
// QWidget::createWindowContainer(): a widget that holds a QWindow, as a child window of the widget's top-level
// window, at the widget's place in it. The window keeps its own native window, so that a QVulkanWindow or an OpenGL
// window draws there directly while the widgets around it are painted as usual.
#include <QtWidgets/qwidget.h>

#include <QtCore/qcoreevent.h>
#include <QtCore/qlist.h>
#include <QtCore/qpointer.h>
#include <QtGui/qevent.h>
#include <QtGui/qwindow.h>

namespace {

class QWindowContainer : public QWidget {
public:
    QWindowContainer(QWindow *window, QWidget *parent, Qt::WindowFlags flags)
        : QWidget(parent, flags), m_window(window) {
        setFocusPolicy(Qt::StrongFocus);
        if (m_window) {
            // Until the container shows, the window is hidden: a window that showed on its own becomes a child.
            m_window->hide();
            watchAncestors();
        }
    }

    ~QWindowContainer() override {
        unwatchAncestors();
        // The container owns the window. Its top-level window may already have taken it with it.
        delete m_window.data();
    }

protected:
    bool event(QEvent *event) override {
        switch (event->type()) {
        case QEvent::Show:
            m_shown = true;
            sync();
            break;
        case QEvent::Hide:
            m_shown = false;
            sync();
            break;
        case QEvent::Move:
        case QEvent::Resize:
            sync();
            break;
        case QEvent::ParentChange:
            watchAncestors();
            sync();
            break;
        case QEvent::FocusIn:
            if (m_window && m_window->isVisible())
                m_window->requestActivate();
            break;
        default:
            break;
        }
        return QWidget::event(event);
    }

    // The ancestors move the container within its top-level window without telling it, and a new parent among them
    // may put it in another top-level window.
    bool eventFilter(QObject *watched, QEvent *event) override {
        switch (event->type()) {
        case QEvent::Move:
        case QEvent::Resize:
            sync();
            break;
        case QEvent::ParentChange:
            watchAncestors();
            sync();
            break;
        default:
            break;
        }
        return QWidget::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent *event) override { (void)event; } // the window covers the widget

private:
    void unwatchAncestors() {
        for (const QPointer<QWidget> &w : std::as_const(m_watched)) {
            if (w)
                w->removeEventFilter(this);
        }
        m_watched.clear();
    }

    void watchAncestors() {
        unwatchAncestors();
        for (QWidget *w = parentWidget(); w; w = w->parentWidget()) {
            w->installEventFilter(this);
            m_watched.append(w);
            if (w->isWindow())
                break;
        }
    }

    // Puts the window in the top-level window at the container's place, and shows it while the container shows.
    void sync() {
        if (!m_window)
            return;
        QWidget *top = window();
        QWindow *host = top->windowHandle();
        if (!host) {
            m_window->hide();
            return;
        }
        if (m_window->parent() != host)
            m_window->setParent(host);
        const QRect geometry(mapTo(top, QPoint(0, 0)), size());
        if (m_window->geometry() != geometry)
            m_window->setGeometry(geometry);
        m_window->setVisible(m_shown && isVisibleTo(top) && !geometry.isEmpty());
    }

    QPointer<QWindow> m_window;
    QList<QPointer<QWidget>> m_watched;
    bool m_shown = false; // between the container's show and hide events
};

} // namespace

/*!
    \since 6.0
    Returns a widget, a child of \a parent with the window flags \a flags, that shows \a window inside it.

    The window becomes a child window (see QWindow::setParent()) of the top-level window that the widget is in, at
    the widget's place and size, and shows while the widget shows. Lay the widget out like any other: the window
    follows it when it or one of its parents moves or is resized. The window keeps its own native window and gets
    its own input events, so this is how a QVulkanWindow or an OpenGL QWindow is put in a widget user interface:

    \code
    auto *vulkan = new MyVulkanWindow;
    vulkan->setVulkanInstance(&instance);
    QWidget *view = QWidget::createWindowContainer(vulkan);
    view->setMinimumSize(320, 240);
    layout->addWidget(view);
    \endcode

    The widget takes ownership of \a window and deletes it when it is deleted. When the widget gets the keyboard
    focus, the window is asked for it; clicking the window gives it the focus too.

    Nt's container is a plain QWidget: there is no \c QWindowContainer class to reach the window through, so keep
    the pointer to it. The window is drawn above the widgets of its top-level window, and widgets that overlap the
    container do not show over it.

    \sa QWindow::setParent(), windowHandle()
*/
QWidget *QWidget::createWindowContainer(QWindow *window, QWidget *parent, Qt::WindowFlags flags) {
    return new QWindowContainer(window, parent, flags);
}
