// MIT License - see LICENSE
//
// Overview page about Nt Widgets for the NDoc documentation. This file holds only a
// documentation comment and is not part of the library.

/*!
    \page nt-widgets.html
    \title Using Nt Widgets
    \brief How to build a user interface from widgets and layouts.

    A user interface is a tree of widgets. The widget without a parent is a window; its children are drawn
    inside it. Layouts place the children, so that a window can be resized without arranging every widget
    by hand, and a QStyle draws the standard controls.

    \section1 A First Form

    Create a QApplication, a top-level QWidget, the widgets, and put them in layouts. Connect to the signals of
    the controls:

    \snippet widgets/hello_widgets.cpp form

    The \c hello_widgets example is this program. Link it with \c Nt::Widgets:

    \code
    find_package(Nt6 REQUIRED)
    add_executable(hello_widgets main.cpp)
    target_link_libraries(hello_widgets PRIVATE Nt::Widgets)
    \endcode

    \section1 Layouts

    A QVBoxLayout stacks widgets from top to bottom, and a QHBoxLayout lines them up from left to right.
    Layouts can be put in other layouts with addLayout(). A layout asks every widget for its
    QWidget::sizeHint() and QWidget::sizePolicy(), and shares out the space: widgets that want to expand
    (policy \c Expanding) or have a stretch factor get what is left over, and
    widgets with a \c Fixed policy keep their hint. Call QWidget::updateGeometry() when the size hint of your own
    widget changes, and the layout is done again from the event loop.

    \section1 Your Own Widgets

    Subclass QWidget, reimplement sizeHint() so that layouts know how large it wants to be, and
    paintEvent() to draw it. Draw with the colors of QWidget::palette() and the QStyle of the widget, so that
    the widget follows the look of the application:

    \code
    class Meter : public QWidget {
    public:
        QSize sizeHint() const override { return QSize(120, 16); }

    protected:
        void paintEvent(QPaintEvent *) override
        {
            QPainter painter(this);
            painter.fillRect(rect(), palette().base());
            painter.fillRect(0, 0, width() * m_value / 100, height(), palette().highlight());
        }

    private:
        int m_value = 40;
    };
    \endcode

    Mouse events go to the deepest widget under the mouse, and to the widget that was pressed until the button is
    released. A widget that does not use an event ignores it, and its parent gets it. Key events go to the widget that
    has the keyboard focus; set a \c focusPolicy so that it can get it, by tabbing or by clicking.

    \section1 What Is Not There Yet

    The widgets are QLabel, QPushButton and QCheckBox. There is no text entry (QLineEdit), no lists, menus,
    dialogs, grid or form layouts, and no scrolling. The native window of a top-level widget is a
    QWindow; child widgets are painted into the backing store of that window and are not native windows
    themselves. Each class lists what differs from Qt in its \e Differences from Qt section.

    \sa QApplication, QWidget, QLayout, QStyle
*/
