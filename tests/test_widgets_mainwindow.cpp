// MIT License - see LICENSE
//
// Tests for the main window of Nt Widgets on the offscreen platform: QMainWindow, QToolBar, QToolButton with a default
// action, QDockWidget and QSizeGrip.
#include <QtCore>
#include <QtGui/qaction.h>
#include <QtGui/qactiongroup.h>
#include <QtGui/qevent.h>
#include <QtGui/qstylehints.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qapplication.h>
#include <QtGui/qpainter.h>
#include <QtGui/qpixmap.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qfocusframe.h>
#include <QtWidgets/qmdiarea.h>
#include <QtWidgets/qmdisubwindow.h>
#include <QtWidgets/qrubberband.h>
#include <QtWidgets/qslider.h>
#include <QtWidgets/qsplashscreen.h>
#include <QtWidgets/qtabbar.h>
#include <QtWidgets/qtextbrowser.h>
#include <QtWidgets/qwidgetaction.h>
#include "platforms/offscreen/qoffscreenintegration_p.h"
#include <QtWidgets/qdockwidget.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qmainwindow.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qmenubar.h>
#include <QtWidgets/qplaintextedit.h>
#include <QtWidgets/qsizegrip.h>
#include <QtWidgets/qstatusbar.h>
#include <QtWidgets/qtoolbar.h>
#include <QtWidgets/qtoolbutton.h>

#include "qpa/qwindowsysteminterface_p.h"

#include <cstdio>
#include <cstdlib>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace {

void settle(QWidget *w) {
    for (int i = 0; i < 3; ++i) {
        QCoreApplication::sendPostedEvents();
        if (w->windowHandle())
            QCoreApplication::sendPostedEvents(w->windowHandle());
    }
}

void mouse(QWidget *w, QEvent::Type type, QPoint local, Qt::MouseButton button, Qt::MouseButtons buttons) {
    QWindow *win = w->window()->windowHandle();
    const QPoint inWindow = w->mapTo(w->window(), local);
    QWindowSystemInterface::handleMouseEvent(win, QPointF(inWindow), QPointF(inWindow + w->window()->pos()), buttons,
                                             button, type, Qt::NoModifier);
}

void click(QWidget *w, QPoint local) {
    mouse(w, QEvent::MouseButtonPress, local, Qt::LeftButton, Qt::LeftButton);
    mouse(w, QEvent::MouseButtonRelease, local, Qt::LeftButton, Qt::NoButton);
}

void testToolButtonAction() {
    QWidget top;
    QToolButton button(&top);
    QAction action("&Bold");
    action.setCheckable(true);
    action.setToolTip("Make it bold");
    int triggered = 0;
    QAction *reported = nullptr;
    QObject::connect(&action, &QAction::triggered, [&] { ++triggered; });
    QObject::connect(&button, &QToolButton::triggered, [&](QAction *a) { reported = a; });
    button.setDefaultAction(&action);
    CHECK(button.defaultAction() == &action && button.actions().contains(&action));
    CHECK(button.text() == QString("Bold") && button.toolTip() == QString("Make it bold") && button.isCheckable());
    top.show();
    settle(&top);

    // A click triggers the action, and the checked state follows it
    button.click();
    CHECK(triggered == 1 && reported == &action && action.isChecked() && button.isChecked());
    action.setChecked(false);
    CHECK(!button.isChecked());
    action.setText("Italic");
    action.setEnabled(false);
    CHECK(button.text() == QString("Italic") && !button.isEnabled());
    button.click();
    CHECK(triggered == 1);
    action.setEnabled(true);

    // The style is recorded
    CHECK(button.toolButtonStyle() == Qt::ToolButtonIconOnly);
    button.setToolButtonStyle(Qt::ToolButtonTextOnly);
    CHECK(button.toolButtonStyle() == Qt::ToolButtonTextOnly);

    // An action group makes checkable tool buttons exclusive
    QActionGroup group(nullptr);
    QAction left("Left"), right("Right");
    group.addAction(&left);
    group.addAction(&right);
    left.setCheckable(true);
    right.setCheckable(true);
    QToolButton a(&top), b(&top);
    a.setDefaultAction(&left);
    b.setDefaultAction(&right);
    a.click();
    b.click();
    CHECK(!left.isChecked() && right.isChecked() && !a.isChecked() && b.isChecked());

    button.setDefaultAction(nullptr);
    CHECK(button.defaultAction() == nullptr);
}

void testToolBar() {
    QWidget top;
    QToolBar bar("Edit", &top);
    CHECK(bar.windowTitle() == QString("Edit") && bar.toggleViewAction()->text() == QString("Edit"));
    CHECK(bar.isMovable() && bar.allowedAreas() == Qt::AllToolBarAreas && bar.orientation() == Qt::Horizontal &&
          !bar.isFloating() && bar.isFloatable() && bar.isAreaAllowed(Qt::LeftToolBarArea));

    QList<QAction *> triggered;
    QObject::connect(&bar, &QToolBar::actionTriggered, [&](QAction *a) { triggered << a; });
    QAction *cut = bar.addAction("Cut");
    int copies = 0;
    QAction *copy = bar.addAction("Copy", [&] { ++copies; });
    QAction *separator = bar.addSeparator();
    auto *combo = new QComboBox;
    QAction *comboAction = bar.addWidget(combo);
    CHECK(bar.actions() == QList<QAction *>({cut, copy, separator, comboAction}));
    CHECK(cut->parent() == &bar && separator->isSeparator());
    auto *cutButton = dynamic_cast<QToolButton *>(bar.widgetForAction(cut));
    CHECK(cutButton && cutButton->defaultAction() == cut && cutButton->autoRaise() &&
          cutButton->text() == QString("Cut"));
    CHECK(bar.widgetForAction(comboAction) == combo && combo->parentWidget() == &bar);
    CHECK(bar.widgetForAction(separator) != nullptr && bar.widgetForAction(nullptr) == nullptr);

    bar.resize(bar.sizeHint());
    top.resize(400, 100);
    top.show();
    settle(&top);

    // In a row, after the grip
    const QRect cutRect = bar.actionGeometry(cut);
    const QRect copyRect = bar.actionGeometry(copy);
    CHECK(cutRect.isValid() && cutRect.left() >= 8 && copyRect.left() > cutRect.right() &&
          bar.actionGeometry(comboAction).left() > bar.actionGeometry(separator).right());
    CHECK(bar.actionAt(cutRect.center()) == cut && bar.actionAt(QPoint(1, 1)) == nullptr);
    bar.setMovable(false);
    settle(&top);
    CHECK(bar.actionGeometry(cut).left() < cutRect.left());

    // Clicking a button triggers its action
    click(cutButton, QPoint(3, 3));
    copy->trigger();
    CHECK(triggered == QList<QAction *>({cut, copy}) && copies == 1);

    // Invisible actions hide their widget; removed actions lose it
    copy->setVisible(false);
    CHECK(bar.widgetForAction(copy)->isHidden());
    copy->setVisible(true);
    CHECK(!bar.widgetForAction(copy)->isHidden());
    QPointer<QWidget> cutWidget(cutButton);
    bar.removeAction(cut);
    CHECK(cutWidget.isNull() && bar.widgetForAction(cut) == nullptr);
    bar.removeAction(comboAction);
    CHECK(combo->parentWidget() == nullptr);
    delete combo;

    // Vertical: a column
    bar.addAction("Paste");
    bar.setOrientation(Qt::Vertical);
    bar.resize(bar.sizeHint());
    settle(&top);
    const QList<QAction *> all = bar.actions();
    CHECK(bar.orientation() == Qt::Vertical &&
          bar.actionGeometry(all.last()).top() > bar.actionGeometry(all.first()).bottom());

    // The style goes to the buttons
    bar.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    CHECK(dynamic_cast<QToolButton *>(bar.widgetForAction(all.last()))->toolButtonStyle() ==
          Qt::ToolButtonTextBesideIcon);

    // The toggle view action shows and hides the tool bar
    bar.toggleViewAction()->trigger();
    CHECK(bar.isHidden() && !bar.toggleViewAction()->isChecked());
    bar.toggleViewAction()->trigger();
    CHECK(!bar.isHidden() && bar.toggleViewAction()->isChecked());
    bar.setWindowTitle("Edit tools");
    CHECK(bar.toggleViewAction()->text() == QString("Edit tools"));

    bar.clear();
    CHECK(bar.actions().isEmpty());
}

void testMainWindow() {
    QMainWindow window;
    window.setGeometry(50, 50, 600, 400);
    CHECK(window.isWindow() && window.menuWidget() == nullptr && window.centralWidget() == nullptr);
    QMenuBar *menu = window.menuBar();
    CHECK(menu && window.menuBar() == menu && window.menuWidget() == menu && menu->parentWidget() == &window);
    menu->addMenu("&File");
    QStatusBar *status = window.statusBar();
    CHECK(status && window.statusBar() == status);
    auto *central = new QPlainTextEdit;
    window.setCentralWidget(central);
    CHECK(window.centralWidget() == central && central->parentWidget() == &window);
    QToolBar *tools = window.addToolBar("Tools");
    tools->addAction("Run");
    CHECK(window.toolBarArea(tools) == Qt::TopToolBarArea && tools->parentWidget() == &window);
    window.show();
    settle(&window);

    // From the top: menu bar, tool bar, central widget, status bar
    CHECK(menu->geometry().top() == 0 && menu->width() == 600);
    CHECK(tools->geometry().top() == menu->geometry().bottom() + 1);
    CHECK(central->geometry().top() == tools->geometry().bottom() + 1 && central->width() == 600);
    CHECK(status->geometry().bottom() == 399 && central->geometry().bottom() == status->geometry().top() - 1);

    // A second tool bar goes on the same line, or a new one after a break
    auto *second = new QToolBar("Second");
    second->addAction("Go");
    window.addToolBar(second);
    settle(&window);
    CHECK(second->geometry().top() == tools->geometry().top() && second->geometry().left() > tools->geometry().right());
    auto *third = new QToolBar("Third");
    third->addAction("Stop");
    window.addToolBarBreak();
    window.addToolBar(Qt::TopToolBarArea, third);
    settle(&window);
    CHECK(window.toolBarBreak(third) && !window.toolBarBreak(second));
    CHECK(third->geometry().top() == tools->geometry().bottom() + 1 &&
          central->geometry().top() > third->geometry().bottom());
    window.removeToolBarBreak(third);
    settle(&window);
    CHECK(third->geometry().top() == tools->geometry().top());

    // Side and bottom tool bars
    auto *side = new QToolBar("Side");
    side->addAction("A");
    side->addAction("B");
    window.addToolBar(Qt::LeftToolBarArea, side);
    auto *low = new QToolBar("Low");
    low->addAction("C");
    window.addToolBar(Qt::BottomToolBarArea, low);
    settle(&window);
    CHECK(side->orientation() == Qt::Vertical && window.toolBarArea(side) == Qt::LeftToolBarArea);
    CHECK(side->geometry().left() == 0 && central->geometry().left() == side->geometry().right() + 1);
    CHECK(low->geometry().bottom() == status->geometry().top() - 1 &&
          central->geometry().bottom() < low->geometry().top());
    auto *inserted = new QToolBar("Inserted");
    window.insertToolBar(side, inserted);
    CHECK(window.toolBarArea(inserted) == Qt::LeftToolBarArea && inserted->orientation() == Qt::Vertical);
    window.removeToolBar(inserted);
    CHECK(window.toolBarArea(inserted) == Qt::NoToolBarArea && inserted->isHidden());
    delete inserted;

    // Hiding a tool bar gives its room to the central widget
    const int centralTop = central->geometry().top();
    second->hide();
    third->hide();
    tools->toggleViewAction()->trigger();
    settle(&window);
    CHECK(central->geometry().top() < centralTop && central->geometry().top() == menu->geometry().bottom() + 1);
    tools->show();
    second->show();
    third->show();

    // The style of the tool buttons
    QStringList styles;
    QObject::connect(&window, &QMainWindow::toolButtonStyleChanged, [&](Qt::ToolButtonStyle) { styles << "changed"; });
    window.setToolButtonStyle(Qt::ToolButtonTextOnly);
    CHECK(window.toolButtonStyle() == Qt::ToolButtonTextOnly && tools->toolButtonStyle() == Qt::ToolButtonTextOnly &&
          styles.size() == 1);

    // Dock widgets: left and right between the top and bottom ones
    auto *left = new QDockWidget("Left");
    left->setWidget(new QLabel("left side"));
    auto *right = new QDockWidget("Right");
    right->setWidget(new QLabel("right side"));
    auto *bottom = new QDockWidget("Bottom");
    bottom->setWidget(new QLabel("bottom"));
    QList<int> locations;
    QObject::connect(left, &QDockWidget::dockLocationChanged, [&](Qt::DockWidgetArea a) { locations << a; });
    window.addDockWidget(Qt::LeftDockWidgetArea, left);
    window.addDockWidget(Qt::RightDockWidgetArea, right);
    window.addDockWidget(Qt::BottomDockWidgetArea, bottom, Qt::Horizontal);
    settle(&window);
    CHECK(locations == QList<int>({Qt::LeftDockWidgetArea}) && left->parentWidget() == &window);
    CHECK(window.dockWidgetArea(left) == Qt::LeftDockWidgetArea &&
          window.dockWidgetArea(bottom) == Qt::BottomDockWidgetArea);
    CHECK(left->geometry().left() == side->geometry().right() + 1 &&
          central->geometry().left() == left->geometry().right() + 1);
    CHECK(right->geometry().left() == central->geometry().right() + 1 && right->geometry().right() == 599);
    CHECK(bottom->geometry().left() == side->geometry().right() + 1 && bottom->geometry().right() == 599 &&
          bottom->geometry().bottom() == low->geometry().top() - 1);
    CHECK(left->geometry().bottom() == bottom->geometry().top() - 1);
    // Two docks in one area share it
    auto *left2 = new QDockWidget("Left 2");
    window.addDockWidget(Qt::LeftDockWidgetArea, left2);
    settle(&window);
    CHECK(left2->geometry().top() == left->geometry().bottom() + 1 &&
          left2->geometry().left() == left->geometry().left());
    // Moving a dock to another area
    window.addDockWidget(Qt::TopDockWidgetArea, left2);
    settle(&window);
    CHECK(window.dockWidgetArea(left2) == Qt::TopDockWidgetArea && left2->geometry().width() == 600 - side->width());

    // The popup menu has the toggle view actions
    QMenu *popup = window.createPopupMenu();
    CHECK(popup && popup->actions().contains(left->toggleViewAction()) &&
          popup->actions().contains(tools->toggleViewAction()));
    delete popup;

    // Removing a dock widget hides it; restoring shows it
    window.removeDockWidget(left2);
    CHECK(window.dockWidgetArea(left2) == Qt::NoDockWidgetArea && left2->isHidden() &&
          !window.restoreDockWidget(left2));
    delete left2;
    left->hide();
    CHECK(window.restoreDockWidget(left) && !left->isHidden());

    // The size hint has room for all the parts
    const QSize hint = window.sizeHint();
    CHECK(hint.height() >= menu->sizeHint().height() + tools->sizeHint().height() + status->sizeHint().height() +
                               central->minimumSizeHint().height());
    CHECK(window.minimumSizeHint().width() <= hint.width());

    // Replacing and taking the central widget, the menu bar and the status bar
    QPointer<QWidget> oldCentral(central);
    window.setCentralWidget(new QLabel("new"));
    CHECK(oldCentral.isNull());
    QWidget *taken = window.takeCentralWidget();
    CHECK(taken && taken->parentWidget() == nullptr && window.centralWidget() == nullptr);
    delete taken;
    QPointer<QStatusBar> oldStatus(status);
    window.setStatusBar(nullptr);
    CHECK(oldStatus.isNull());
    QPointer<QMenuBar> oldMenu(menu);
    window.setMenuWidget(new QLabel("menu"));
    CHECK(oldMenu.isNull() && dynamic_cast<QLabel *>(window.menuWidget()));

    // Deleting a tool bar or a dock widget forgets it
    delete low;
    delete right;
    CHECK(window.toolBarArea(nullptr) == Qt::NoToolBarArea);
    settle(&window);
}

void testDockWidget() {
    QMainWindow window;
    window.setGeometry(100, 100, 500, 300);
    window.setCentralWidget(new QLabel("center"));
    auto *dock = new QDockWidget("Panel", &window);
    auto *content = new QLabel("content");
    dock->setWidget(content);
    window.addDockWidget(Qt::RightDockWidgetArea, dock);
    window.show();
    settle(&window);

    CHECK(dock->widget() == content && content->parentWidget() == dock && dock->windowTitle() == QString("Panel"));
    CHECK(dock->features() ==
          (QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable));
    CHECK(dock->allowedAreas() == Qt::AllDockWidgetAreas && dock->isAreaAllowed(Qt::TopDockWidgetArea) &&
          !dock->isFloating());
    CHECK(content->geometry().top() > 10 && content->geometry().left() >= 1);

    // The title bar has a close and a float button
    QList<QToolButton *> buttons = dock->findChildren<QToolButton *>();
    QToolButton *closeButton = nullptr;
    QToolButton *floatButton = nullptr;
    for (QToolButton *b : buttons) {
        if (b->text() == QString("x"))
            closeButton = b;
        if (b->text() == QString("+"))
            floatButton = b;
    }
    CHECK(closeButton && floatButton && !closeButton->isHidden() && !floatButton->isHidden());

    // Closing hides it and unchecks its toggle view action
    QList<int> visibility;
    QObject::connect(dock, &QDockWidget::visibilityChanged, [&](bool v) { visibility << (v ? 1 : 0); });
    closeButton->click();
    CHECK(dock->isHidden() && !dock->toggleViewAction()->isChecked() && visibility == QList<int>({0}));
    dock->toggleViewAction()->trigger();
    CHECK(!dock->isHidden() && visibility.last() == 1);

    // Floating: a window of its own with the main window as its parent
    QList<int> topLevel;
    QObject::connect(dock, &QDockWidget::topLevelChanged, [&](bool t) { topLevel << (t ? 1 : 0); });
    const QRect docked = dock->geometry();
    floatButton->click();
    settle(dock);
    CHECK(dock->isFloating() && dock->isWindow() && dock->parentWidget() == &window && topLevel == QList<int>({1}));
    CHECK(dock->isVisible() && dock->windowHandle() &&
          dock->windowHandle()->transientParent() == window.windowHandle());
    CHECK(dock->size() == docked.size());
    CHECK(window.centralWidget()->width() == 500); // the room goes to the central widget
    dock->setFloating(false);
    settle(&window);
    CHECK(!dock->isFloating() && !dock->isWindow() && topLevel.last() == 0 && dock->geometry().right() == 499);

    // Double-clicking the title bar floats and docks it
    mouse(dock, QEvent::MouseButtonDblClick, QPoint(10, 5), Qt::LeftButton, Qt::LeftButton);
    CHECK(dock->isFloating());
    mouse(dock, QEvent::MouseButtonDblClick, QPoint(10, 5), Qt::LeftButton, Qt::LeftButton);
    CHECK(!dock->isFloating());

    // Features choose the buttons
    QList<int> features;
    QObject::connect(dock, &QDockWidget::featuresChanged,
                     [&](QDockWidget::DockWidgetFeatures f) { features << static_cast<int>(f); });
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    CHECK(closeButton->isHidden() && floatButton->isHidden() && features.size() == 1);
    dock->close(); // not closable: a child is hidden all the same by close()
    dock->show();
    mouse(dock, QEvent::MouseButtonDblClick, QPoint(10, 5), Qt::LeftButton, Qt::LeftButton);
    CHECK(!dock->isFloating());
    dock->setFeatures(QDockWidget::DockWidgetFeatureMask);

    // A widget can replace the title bar
    auto *title = new QLabel("Custom");
    dock->setTitleBarWidget(title);
    settle(&window);
    CHECK(dock->titleBarWidget() == title && title->parentWidget() == dock && closeButton->isHidden() &&
          title->geometry().top() == 1 && content->geometry().top() == title->geometry().bottom() + 1);
    dock->setTitleBarWidget(nullptr);
    CHECK(!closeButton->isHidden());

    QList<int> areas;
    QObject::connect(dock, &QDockWidget::allowedAreasChanged,
                     [&](Qt::DockWidgetAreas a) { areas << static_cast<int>(a); });
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    CHECK(!dock->isAreaAllowed(Qt::TopDockWidgetArea) && areas.size() == 1);

    // A floating dock widget added to a main window is docked
    dock->setFloating(true);
    window.addDockWidget(Qt::LeftDockWidgetArea, dock);
    CHECK(!dock->isFloating() && window.dockWidgetArea(dock) == Qt::LeftDockWidgetArea);

    // Replacing the widget hides the old one
    auto *other = new QLabel("other");
    dock->setWidget(other);
    CHECK(dock->widget() == other && content->isHidden());
}

void testSizeGrip() {
    QWidget window;
    window.setGeometry(10, 10, 200, 150);
    window.setMinimumSize(120, 100);
    auto *grip = new QSizeGrip(&window);
    grip->setGeometry(window.width() - 13, window.height() - 13, 13, 13);
    window.show();
    settle(&window);
    CHECK(grip->sizeHint() == QSize(13, 13));

    // Dragging the grip resizes the window
    mouse(grip, QEvent::MouseButtonPress, QPoint(5, 5), Qt::LeftButton, Qt::LeftButton);
    const QPoint start = grip->mapTo(&window, QPoint(5, 5));
    QWindow *win = window.windowHandle();
    const auto moveTo = [&](QPoint inWindow) {
        QWindowSystemInterface::handleMouseEvent(win, QPointF(inWindow), QPointF(inWindow + window.pos()),
                                                 Qt::LeftButton, Qt::NoButton, QEvent::MouseMove, Qt::NoModifier);
    };
    moveTo(start + QPoint(30, 20));
    settle(&window);
    CHECK(window.size() == QSize(230, 170));
    // ... not below the minimum size
    moveTo(start + QPoint(-500, -500));
    settle(&window);
    CHECK(window.size() == QSize(120, 100));
    QWindowSystemInterface::handleMouseEvent(win, QPointF(start), QPointF(start + window.pos()), Qt::NoButton,
                                             Qt::LeftButton, QEvent::MouseButtonRelease, Qt::NoModifier);
}

// Counts the events of one type and can stop them.
class Filter : public QObject {
public:
    QEvent::Type type;
    bool stop = false;
    int seen = 0;
    explicit Filter(QEvent::Type t) : type(t) {}
    bool eventFilter(QObject *watched, QEvent *event) override {
        (void)watched;
        if (event->type() != type)
            return false;
        ++seen;
        return stop;
    }
};

QImage grab(QWidget *w) {
    settle(w->window());
    return QOffscreenBackingStore::imageOf(w->window()->windowHandle());
}

void testEventFiltersAndPixmaps() {
    // Event filters see the events first and can stop them
    QWidget top;
    auto *button = new QToolButton(&top);
    button->setText("B");
    button->setGeometry(0, 0, 40, 30);
    int clicks = 0;
    QObject::connect(button, &QToolButton::clicked, [&] { ++clicks; });
    top.resize(100, 60);
    top.show();
    settle(&top);
    Filter filter(QEvent::MouseButtonPress);
    button->installEventFilter(&filter);
    click(button, QPoint(5, 5));
    CHECK(filter.seen == 1 && clicks == 1);
    filter.stop = true;
    click(button, QPoint(5, 5));
    CHECK(filter.seen == 2 && clicks == 1);
    button->removeEventFilter(&filter);
    click(button, QPoint(5, 5));
    CHECK(filter.seen == 2 && clicks == 2);
    {
        Filter appFilter(QEvent::MouseButtonPress); // on the application: all objects
        qApp->installEventFilter(&appFilter);
        click(button, QPoint(5, 5));
        CHECK(appFilter.seen >= 1); // the window and the button
    } // a deleted filter is forgotten
    click(button, QPoint(5, 5));
    CHECK(clicks == 4);

    // A pixmap is painted on and shown by a label
    QPixmap pixmap(20, 10);
    CHECK(!pixmap.isNull() && pixmap.size() == QSize(20, 10) && pixmap.depth() == 32 && QPixmap().isNull());
    pixmap.fill(Qt::red);
    {
        QPainter painter(&pixmap);
        painter.fillRect(QRect(10, 0, 10, 10), Qt::blue);
    }
    QImage image = pixmap.toImage();
    CHECK(image.pixel(2, 2) == QColor(Qt::red).rgb() && image.pixel(15, 5) == QColor(Qt::blue).rgb());
    QPixmap shared = pixmap; // copies share until painted
    {
        QPainter painter(&shared);
        painter.fillRect(shared.rect(), Qt::green);
    }
    CHECK(pixmap.toImage().pixel(2, 2) == QColor(Qt::red).rgb() &&
          shared.toImage().pixel(2, 2) == QColor(Qt::green).rgb());
    CHECK(pixmap.copy(QRect(10, 0, 5, 5)).toImage().pixel(0, 0) == QColor(Qt::blue).rgb());
    CHECK(QPixmap::fromImage(image).size() == QSize(20, 10));
    auto *label = new QLabel("text", &top);
    label->setPixmap(pixmap);
    CHECK(label->text().isEmpty() && label->pixmap().size() == QSize(20, 10) && label->sizeHint() == QSize(20, 10));
    label->setGeometry(50, 10, 20, 10);
    label->show();
    QImage shown = grab(&top);
    CHECK(shown.pixel(52, 12) == QColor(Qt::red).rgb() && shown.pixel(65, 15) == QColor(Qt::blue).rgb());
    label->setScaledContents(true);
    CHECK(label->hasScaledContents());
    label->setText("again");
    CHECK(label->pixmap().isNull());
}

void testSmallDisplayWidgets() {
    // The splash screen: a frameless window in the middle of the screen
    QPixmap picture(120, 60);
    picture.fill(Qt::darkCyan);
    QSplashScreen splash(picture);
    CHECK(splash.isWindow() && splash.windowType() == Qt::SplashScreen && splash.size() == QSize(120, 60));
    CHECK(splash.windowFlags().testFlag(Qt::FramelessWindowHint) && splash.pixmap().size() == QSize(120, 60));
    splash.show();
    settle(&splash);
    QStringList messages;
    QObject::connect(&splash, &QSplashScreen::messageChanged, [&](const QString &m) { messages << m; });
    splash.showMessage("Loading", Qt::AlignBottom | Qt::AlignHCenter, Qt::white);
    CHECK(splash.message() == QString("Loading") && messages == QStringList({"Loading"}));
    splash.clearMessage();
    CHECK(splash.message().isEmpty());
    QWidget main;
    splash.finish(&main);
    CHECK(splash.isVisible()); // waits for the window
    main.show();
    CHECK(!splash.isVisible());

    // A rubber band and a focus frame let the mouse through
    QWidget canvas;
    canvas.resize(200, 150);
    auto *target = new QToolButton(&canvas);
    target->setGeometry(20, 20, 60, 30);
    int clicks = 0;
    QObject::connect(target, &QToolButton::clicked, [&] { ++clicks; });
    QRubberBand band(QRubberBand::Rectangle, &canvas);
    CHECK(band.shape() == QRubberBand::Rectangle && band.isHidden());
    band.setGeometry(10, 10, 100, 60);
    band.show();
    canvas.show();
    settle(&canvas);
    click(target, QPoint(5, 5));
    CHECK(clicks == 1);
    const QImage withBand = grab(&canvas);
    CHECK(withBand.pixel(10, 10) == canvas.palette().color(QPalette::Highlight).rgb());
    band.hide();

    QFocusFrame frame(&canvas);
    frame.setWidget(target);
    CHECK(frame.widget() == target && frame.geometry() == QRect(18, 18, 64, 34) && !frame.isHidden());
    target->move(40, 50);
    CHECK(frame.geometry() == QRect(38, 48, 64, 34));
    target->hide();
    CHECK(frame.isHidden());
    target->show();
    CHECK(!frame.isHidden());
    click(target, QPoint(1, 1));
    CHECK(clicks == 2);
    frame.setWidget(nullptr);
    CHECK(frame.isHidden());

    // A widget action puts its widget in a tool bar
    QToolBar bar;
    auto *action = new QWidgetAction(&bar);
    auto *slider = new QSlider(Qt::Horizontal);
    action->setDefaultWidget(slider);
    CHECK(action->defaultWidget() == slider);
    bar.addAction(action);
    CHECK(bar.widgetForAction(action) == slider && slider->parentWidget() == &bar);
    QToolBar other;
    other.addAction(action); // the default widget is in use
    CHECK(dynamic_cast<QToolButton *>(other.widgetForAction(action)) != nullptr);
    bar.removeAction(action);
    CHECK(slider->parentWidget() == nullptr && action->defaultWidget() == slider);
    class MakingAction : public QWidgetAction {
    public:
        using QWidgetAction::QWidgetAction;
        int made = 0;
        int deleted = 0;
        QList<QWidget *> created() const { return createdWidgets(); }

    protected:
        QWidget *createWidget(QWidget *parent) override {
            ++made;
            return new QLabel("made", parent);
        }
        void deleteWidget(QWidget *widget) override {
            ++deleted;
            delete widget;
        }
    };
    MakingAction making(nullptr);
    bar.addAction(&making);
    other.addAction(&making);
    CHECK(making.made == 2 && making.created().size() == 2 && dynamic_cast<QLabel *>(bar.widgetForAction(&making)));
    bar.removeAction(&making);
    CHECK(making.deleted == 1 && making.created().size() == 1);
    other.removeAction(&making);
    other.removeAction(action);
}

void testTextBrowser() {
    QTemporaryDir temp;
    const QString root = temp.path();
    QDir(root).mkdir("help");
    const auto write = [&](const QString &name, const char *text) {
        QFile file(root + "/" + name);
        file.open(QIODevice::WriteOnly);
        file.write(text);
    };
    write("help/index.txt", "Index");
    write("help/page.txt", "Page");
    write("other.txt", "Other");

    QTextBrowser browser;
    CHECK(browser.isReadOnly() && browser.source().isEmpty() && !browser.isBackwardAvailable());
    QList<int> back, forward;
    QList<QUrl> sources;
    QObject::connect(&browser, &QTextBrowser::backwardAvailable, [&](bool b) { back << (b ? 1 : 0); });
    QObject::connect(&browser, &QTextBrowser::forwardAvailable, [&](bool f) { forward << (f ? 1 : 0); });
    QObject::connect(&browser, &QTextBrowser::sourceChanged, [&](const QUrl &u) { sources << u; });
    browser.setSearchPaths({root});
    browser.setSource(QUrl::fromLocalFile(root + "/help/index.txt"));
    CHECK(browser.toPlainText() == QString("Index") && sources.size() == 1);
    browser.setSource(QUrl("page.txt")); // next to the current source
    CHECK(browser.toPlainText() == QString("Page") && browser.isBackwardAvailable() && back == QList<int>({1}));
    browser.setSource(QUrl("other.txt")); // in the search paths
    CHECK(browser.toPlainText() == QString("Other") && browser.backwardHistoryCount() == 2);
    CHECK(browser.historyTitle(-1) == QString("page.txt") && browser.historyUrl(0) == QUrl("other.txt"));
    browser.backward();
    CHECK(browser.toPlainText() == QString("Page") && browser.isForwardAvailable() && forward == QList<int>({1}));
    browser.backward();
    CHECK(browser.toPlainText() == QString("Index") && !browser.isBackwardAvailable());
    browser.forward();
    CHECK(browser.toPlainText() == QString("Page") && browser.forwardHistoryCount() == 1);
    browser.setSource(QUrl("missing.txt"));
    CHECK(browser.toPlainText().isEmpty() && !browser.isForwardAvailable());
    browser.home();
    CHECK(browser.toPlainText() == QString("Index"));
    write("help/index.txt", "Changed");
    browser.reload();
    CHECK(browser.toPlainText() == QString("Changed"));
    browser.clearHistory();
    CHECK(!browser.isBackwardAvailable() && !browser.isForwardAvailable() && browser.backwardHistoryCount() == 0);
}

void testMdiArea() {
    QMdiArea area;
    area.resize(600, 400);
    area.show();
    settle(&area);
    QList<QMdiSubWindow *> activated;
    QObject::connect(&area, &QMdiArea::subWindowActivated, [&](QMdiSubWindow *w) { activated << w; });

    auto *edit = new QLabel("first");
    edit->setWindowTitle("First");
    QMdiSubWindow *first = area.addSubWindow(edit);
    first->show();
    QMdiSubWindow *second = area.addSubWindow(new QLabel("second"));
    second->setWindowTitle("Second");
    second->show();
    settle(&area);
    CHECK(first->widget() == edit && first->windowTitle() == QString("First") && first->mdiArea() == &area);
    CHECK(area.subWindowList() == QList<QMdiSubWindow *>({first, second}));
    CHECK(area.activeSubWindow() == first && activated == QList<QMdiSubWindow *>({first}));
    edit->setWindowTitle("Renamed"); // the title follows the widget
    CHECK(first->windowTitle() == QString("Renamed"));

    // Activating and the orders
    area.setActiveSubWindow(second);
    CHECK(area.activeSubWindow() == second && second->windowState().testFlag(Qt::WindowActive) &&
          !first->windowState().testFlag(Qt::WindowActive));
    CHECK(area.subWindowList(QMdiArea::StackingOrder).last() == second);
    CHECK(area.subWindowList(QMdiArea::ActivationHistoryOrder) == QList<QMdiSubWindow *>({first, second}));
    area.activateNextSubWindow();
    CHECK(area.activeSubWindow() == first);
    area.activatePreviousSubWindow();
    CHECK(area.activeSubWindow() == second);
    click(first, QPoint(first->width() / 2, 8)); // a press on the title bar
    CHECK(area.activeSubWindow() == first);

    // Moving by the title bar and resizing by an edge
    const QRect before = first->geometry();
    mouse(first, QEvent::MouseButtonPress, QPoint(30, 8), Qt::LeftButton, Qt::LeftButton);
    mouse(first, QEvent::MouseMove, QPoint(80, 48), Qt::NoButton, Qt::LeftButton);
    mouse(first, QEvent::MouseButtonRelease, QPoint(80, 48), Qt::LeftButton, Qt::NoButton);
    CHECK(first->geometry() == before.translated(50, 40));
    const QRect moved = first->geometry();
    const QPoint edge(first->width() - 2, first->height() / 2);
    mouse(first, QEvent::MouseButtonPress, edge, Qt::LeftButton, Qt::LeftButton);
    mouse(first, QEvent::MouseMove, edge + QPoint(30, 0), Qt::NoButton, Qt::LeftButton);
    mouse(first, QEvent::MouseButtonRelease, edge + QPoint(30, 0), Qt::LeftButton, Qt::NoButton);
    CHECK(first->geometry() == moved.adjusted(0, 0, 30, 0));

    // States
    QList<int> states;
    QObject::connect(first, &QMdiSubWindow::windowStateChanged,
                     [&](Qt::WindowStates, Qt::WindowStates n) { states << static_cast<int>(n); });
    first->showMaximized();
    CHECK(first->isMaximized() && first->geometry() == area.viewport()->rect() && !states.isEmpty());
    area.resize(500, 300);
    settle(&area);
    CHECK(first->geometry() == area.viewport()->rect()); // follows the area
    first->showNormal();
    CHECK(!first->isMaximized() && first->geometry() == moved.adjusted(0, 0, 30, 0));
    first->showMinimized();
    CHECK(first->isMinimized() && edit->isHidden() && first->height() < 40);
    first->showNormal();
    CHECK(!edit->isHidden());
    mouse(first, QEvent::MouseButtonDblClick, QPoint(30, 8), Qt::LeftButton, Qt::LeftButton);
    CHECK(first->isMaximized());
    first->showNormal();

    // Tile and cascade
    area.tileSubWindows();
    CHECK(first->geometry().intersected(second->geometry()).isEmpty() && first->width() + second->width() <= 500);
    area.cascadeSubWindows();
    CHECK(second->pos().x() > first->pos().x() && second->pos().y() > first->pos().y());

    // The tabbed view
    area.setViewMode(QMdiArea::TabbedView);
    settle(&area);
    QTabBar *tabs = area.findChild<QTabBar *>();
    CHECK(tabs && tabs->count() == 2 && tabs->tabText(0) == QString("Renamed"));
    QMdiSubWindow *active = area.activeSubWindow();
    CHECK(active->geometry() == area.viewport()->rect() && (active == first ? second : first)->isHidden());
    tabs->setCurrentIndex(tabs->currentIndex() == 0 ? 1 : 0);
    CHECK(area.activeSubWindow() != active);
    area.setTabsClosable(true);
    area.setTabsMovable(true);
    CHECK(area.tabsClosable() && area.tabsMovable());
    area.setViewMode(QMdiArea::SubWindowView);
    CHECK(!first->isHidden() && !second->isHidden() && tabs->isHidden());

    // Closing: the widget may refuse
    class Stubborn : public QLabel {
    public:
        bool refuse = true;
        using QLabel::QLabel;

    protected:
        void closeEvent(QCloseEvent *event) override {
            if (refuse)
                event->ignore();
            else
                event->accept();
        }
    };
    auto *stubborn = new Stubborn("stubborn");
    QMdiSubWindow *third = area.addSubWindow(stubborn);
    third->show();
    area.setActiveSubWindow(third);
    area.closeActiveSubWindow();
    CHECK(area.subWindowList().contains(third) && third->isVisible());
    stubborn->refuse = false;
    QPointer<QMdiSubWindow> thirdGuard(third);
    area.closeActiveSubWindow();
    CHECK(!area.subWindowList().contains(third) && area.activeSubWindow() != third);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(thirdGuard.isNull());

    // Removing
    area.removeSubWindow(second);
    CHECK(second->parentWidget() == nullptr && !area.subWindowList().contains(second));
    delete second;
    QWidget *taken = first->widget();
    area.removeSubWindow(taken);
    CHECK(first->widget() == nullptr && taken->parentWidget() == nullptr);
    delete taken;
    area.closeAllSubWindows();
    CHECK(area.subWindowList().isEmpty() && area.activeSubWindow() == nullptr);
    area.setBackground(QBrush(Qt::darkBlue));
    CHECK(area.background().color() == QColor(Qt::darkBlue));
}
} // namespace

int main(int argc, char **argv) {
    ::setenv("QT_QPA_PLATFORM", "offscreen", 1);
    ::setenv("QT_SCALE_FACTOR", "1", 1);
    QApplication app(argc, argv);
    app.styleHints()->setColorScheme(Qt::ColorScheme::Light);
    app.setQuitOnLastWindowClosed(false);
    testToolButtonAction();
    testToolBar();
    testMainWindow();
    testDockWidget();
    testSizeGrip();
    testEventFiltersAndPixmaps();
    testSmallDisplayWidgets();
    testTextBrowser();
    testMdiArea();
    if (g_failures == 0)
        std::puts("all main window tests passed");
    return g_failures == 0 ? 0 : 1;
}
