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
    the controls. The \c hello_widgets example has the Nt logo and one of every widget, on the pages of a QTabWidget.
    The first page, \e Nt, is a showcase: one row for each widget that Nt has, with the name of its class and a live
    example. The pages after it group the widgets as an application would: buttons, check boxes and radio buttons in a
    QGroupBox; line edits, a spin box and a combo box; and sliders and a progress bar that show the same value. Each
    page is a function that builds a widget with a layout:

    \snippet widgets/hello_widgets.cpp form

    \image hello-widgets-nt.png The Nt page of the hello_widgets example, which shows every widget
    \image hello-widgets-light.png The pages of the hello_widgets example in the light color scheme, with the open list of a combo box
    \image hello-widgets-dark.png The pages of the hello_widgets example in the dark color scheme

    The logo is drawn from its outlines by the widget in \c ntlogo.h, which the \c gallery example uses too.

    Link it with \c Nt::Widgets:

    \code
    find_package(Nt6 REQUIRED)
    add_executable(hello_widgets main.cpp)
    target_link_libraries(hello_widgets PRIVATE Nt::Widgets)
    \endcode

    \section1 The Gallery

    The \c gallery example shows what the widgets can do: layouts, check boxes and buttons, and custom widgets
    (a meter, color swatches and a mouse pad) that draw with the palette and handle the mouse, wheel and keys.
    It follows the color scheme of the desktop, and its \e{Dark theme} check box overrides it with
    QStyleHints::setColorScheme(). The logo is drawn from its outlines at the device resolution, so that it stays sharp
    on a high-resolution screen.

    \image widgets-gallery-light.png The gallery example with the light color scheme
    \image widgets-gallery-dark.png The gallery example with the dark color scheme

    \section1 High-Resolution Screens and Dark Mode

    The widgets are measured in window coordinates, and the screen decides how many pixels each one covers:
    QScreen::devicePixelRatio() is 2 on a screen that asks for twice the usual density, and everything a QPainter draws is
    scaled by it. On the \c xcb platform the ratio comes from the \c Xft.dpi resource, or from the
    \c QT_SCALE_FACTOR environment variable. The default style follows the desktop's color scheme, which is read from the
    desktop portal; set your own palette with QApplication::setPalette(), or the scheme with QStyleHints::setColorScheme().

    \section1 Comparing with Qt

    The widget examples use only the API that Nt shares with Qt, so the same sources build against Qt 6. Configure Nt with
    \c{-DNT_EXAMPLES_QT=ON} and the \c qt_examples target builds them against Qt as \c hello_widgets_qt and \c gallery_qt in
    the \c qt-examples directory of the build tree, or build \c examples/qt as a project of its own. Run both with the same
    font and the Fusion style (\c{QT_STYLE_OVERRIDE=Fusion}) to compare. Nt follows the layout of Qt's Fusion style:
    the sizes of buttons and check boxes, the margins and spacing, and how a layout shares out space, so the widgets of the
    two sit at the same coordinates. What remains different is small: Qt snaps text to whole pixel sizes, which makes it
    a little narrower (about 2 percent), the buttons are shaded differently and the check box does not have the same check
    mark, and Nt draws a focus indicator on a check box that has the focus without the keyboard.

    \section1 Layouts

    A QVBoxLayout stacks widgets from top to bottom, and a QHBoxLayout lines them up from left to right.
    Layouts can be put in other layouts with addLayout(). A layout asks every widget for its
    QWidget::sizeHint() and QWidget::sizePolicy(), and shares out the space as Qt does: widgets that want to expand
    (policy \c Expanding) or have a stretch factor get what is left over, and
    widgets with a \c Fixed policy keep their hint, centered across the layout if it is wider than they are. Call QWidget::updateGeometry() when the size hint of your own
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

    \section1 The Widgets

    \table
    \header
    \li Widget
    \li What it is for
    \row
    \li QLabel
    \li A text, with alignment and word wrap.
    \row
    \li QPushButton, QCheckBox, QRadioButton
    \li A button to click, an option to switch, and one choice of a group. Radio buttons with the same parent are exclusive.
    \row
    \li QLineEdit
    \li One line of text to type, with selection, a placeholder and a password mode.
    \row
    \li QSpinBox
    \li A whole number to type or step.
    \row
    \li QComboBox
    \li One item to choose from a list that drops down.
    \row
    \li QSlider, QProgressBar
    \li A value to drag, and one to show.
    \row
    \li QGroupBox
    \li A frame with a title around other widgets, which can have a check box to switch them all.
    \row
    \li QTabWidget
    \li Pages with a row of tabs to choose the page.
    \endtable

    The page of each widget class shows how the widget is drawn: normal, with the keyboard focus, pressed, checked,
    read-only and disabled where that applies, in the light color scheme. The pictures are made by the
    \c render_widgets program, which calls QWidget::render() to paint the widgets on an image.

    \section1 What Is Not There Yet

    There are no lists and tables, text editing of more than a line, menus, dialogs, grid or form layouts, and no
    scrolling. The native window of a top-level widget is a
    QWindow; child widgets are painted into the backing store of that window and are not native windows
    themselves. Each class lists what differs from Qt in its \e Differences from Qt section.

    \sa QApplication, QWidget, QLayout, QStyle
*/
