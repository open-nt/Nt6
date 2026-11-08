// MIT License - see LICENSE
//
// Tests for actions, menus and menu bars of Nt Widgets on the offscreen platform. The tests play the window system
// through QWindowSystemInterface.
#include <QtCore>
#include <QtGui/qaction.h>
#include <QtGui/qactiongroup.h>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtGui/qstylehints.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qmenubar.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include "platforms/offscreen/qoffscreenintegration_p.h"
#include "qpa/qwindowsysteminterface_p.h"

#include <cstdio>
#include <cstdlib>
#include <string>

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
        QCoreApplication::sendPostedEvents(w);
        if (w->windowHandle())
            QCoreApplication::sendPostedEvents(w->windowHandle());
    }
}

QImage grab(QWidget *w) {
    settle(w->window());
    return QOffscreenBackingStore::imageOf(w->window()->windowHandle());
}

void mouse(QWidget *w, QEvent::Type type, QPoint local, Qt::MouseButton button = Qt::LeftButton,
           Qt::MouseButtons buttons = Qt::LeftButton, Qt::KeyboardModifiers mods = Qt::NoModifier) {
    QWindow *win = w->window()->windowHandle();
    const QPoint inWindow = w->mapTo(w->window(), local);
    QWindowSystemInterface::handleMouseEvent(win, QPointF(inWindow), QPointF(inWindow + w->window()->pos()), buttons,
                                             button, type, mods);
}

void press(QWidget *w, QPoint local, Qt::KeyboardModifiers mods = Qt::NoModifier) {
    mouse(w, QEvent::MouseButtonPress, local, Qt::LeftButton, Qt::LeftButton, mods);
}
void release(QWidget *w, QPoint local) { mouse(w, QEvent::MouseButtonRelease, local, Qt::LeftButton, Qt::NoButton); }
void click(QWidget *w, QPoint local, Qt::KeyboardModifiers mods = Qt::NoModifier) {
    press(w, local, mods);
    release(w, local);
}

void key(QWidget *w, int k, const QString &text = QString(), Qt::KeyboardModifiers mods = Qt::NoModifier) {
    QWindow *win = w->window()->windowHandle();
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyPress, k, mods, text);
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyRelease, k, mods);
}

QPoint inWindow(QWidget *w, QPoint local = QPoint(0, 0)) { return w->mapTo(w->window(), local); }

void hover(QWidget *w, QPoint local) { mouse(w, QEvent::MouseMove, local, Qt::NoButton, Qt::NoButton); }

// Runs the event loop until `condition` holds or `ms` milliseconds have passed.
template <typename Condition> bool waitFor(Condition condition, int ms = 2000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
        if (timer.elapsed() > ms)
            return false;
        QEventLoop loop;
        QTimer::singleShot(3, [&] { loop.quit(); });
        loop.exec();
    }
    return true;
}

// Records the action events that a widget gets.
class Recorder : public QWidget {
public:
    explicit Recorder(QWidget *parent = nullptr) : QWidget(parent) {}
    std::string log;

protected:
    void actionEvent(QActionEvent *event) override {
        log += event->type() == QEvent::ActionAdded     ? "added "
               : event->type() == QEvent::ActionRemoved ? "removed "
                                                        : "changed ";
        if (event->type() == QEvent::ActionAdded)
            log += event->before() ? "before " : "last ";
    }
};

// A window with a menu bar on top and a label below; the bar and the label fill the width.
struct Host {
    QWidget top;
    QVBoxLayout *layout;
    QMenuBar *bar;
    QLabel *label;

    Host() {
        top.resize(420, 300);
        layout = new QVBoxLayout(&top);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        bar = new QMenuBar(&top);
        layout->addWidget(bar);
        label = new QLabel("Content", &top);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label, 1);
    }
    void show() {
        top.show();
        settle(&top);
    }
};

void testWidgetActions() {
    Recorder w;
    QAction a("A"), b("B"), c("C");
    CHECK(w.actions().isEmpty());
    w.addAction(&a);
    w.addAction(&b);
    CHECK(w.log == "added last added last " && w.actions() == (QList<QAction *>{&a, &b}));
    w.log.clear();
    w.insertAction(&b, &c);
    CHECK(w.log == "added before " && w.actions() == (QList<QAction *>{&a, &c, &b}));
    w.log.clear();
    w.insertAction(nullptr, &a); // an action that is there is moved
    CHECK(w.log == "removed added last " && w.actions() == (QList<QAction *>{&c, &b, &a}));
    w.log.clear();
    a.setText("A2");
    CHECK(w.log == "changed ");
    w.log.clear();
    w.removeAction(&b);
    w.removeAction(&b);
    CHECK(w.log == "removed " && w.actions() == (QList<QAction *>{&c, &a}));
    CHECK(b.associatedObjects().isEmpty() && a.associatedObjects() == (QList<QObject *>{&w}));
    b.setText("not heard");
    CHECK(w.log == "removed ");
    w.insertAction(&a, nullptr);
    w.addActions({&b, &b});
    CHECK(w.actions() == (QList<QAction *>{&c, &a, &b}));
    w.insertActions(&c, {&b, &a});
    CHECK(w.actions() == (QList<QAction *>{&b, &a, &c}));
    // Deleting an action removes it
    w.log.clear();
    auto *gone = new QAction("gone");
    w.addAction(gone);
    w.log.clear();
    delete gone;
    CHECK(w.log == "removed " && w.actions().size() == 3);
    // Deleting a widget lets go of its actions
    {
        Recorder temporary;
        temporary.addAction(&a);
        CHECK(a.associatedObjects().size() == 2);
    }
    CHECK(a.associatedObjects() == (QList<QObject *>{&w}));
}

void testShortcuts() {
    Host host;
    int saved = 0, quits = 0, deleted = 0, enters = 0;
    QAction save("Save"), quit("Quit"), del("Delete"), enter("Enter"), disabled("Off"), unused("Unused");
    save.setShortcut(QKeySequence("Ctrl+S"));
    quit.setShortcut(QKeySequence::Quit);
    del.setShortcut(QKeySequence("Del"));
    enter.setShortcut(QKeySequence("Ctrl+Shift+E"));
    disabled.setShortcut(QKeySequence("Ctrl+D"));
    disabled.setEnabled(false);
    QObject::connect(&save, &QAction::triggered, [&] { ++saved; });
    QObject::connect(&quit, &QAction::triggered, [&] { ++quits; });
    QObject::connect(&del, &QAction::triggered, [&] { ++deleted; });
    QObject::connect(&enter, &QAction::triggered, [&] { ++enters; });
    int offs = 0;
    QObject::connect(&disabled, &QAction::triggered, [&] { ++offs; });
    host.label->addAction(&save);
    host.top.addAction(&quit);
    host.top.addAction(&del);
    host.label->addAction(&enter);
    host.label->addAction(&disabled);
    host.show();
    key(&host.top, Qt::Key_S, "s", Qt::ControlModifier);
    CHECK(saved == 1 && quits == 0);
    key(&host.top, Qt::Key_Q, "q", Qt::ControlModifier);
    CHECK(quits == 1 && saved == 1);
    key(&host.top, Qt::Key_S, "s");
    key(&host.top, Qt::Key_S, "S", Qt::ShiftModifier);
    key(&host.top, Qt::Key_S, "s", Qt::ControlModifier | Qt::AltModifier);
    CHECK(saved == 1);
    key(&host.top, Qt::Key_Delete);
    CHECK(deleted == 1);
    key(&host.top, Qt::Key_E, "E", Qt::ControlModifier | Qt::ShiftModifier);
    CHECK(enters == 1);
    key(&host.top, Qt::Key_D, "d", Qt::ControlModifier);
    CHECK(offs == 0);
    disabled.setEnabled(true);
    key(&host.top, Qt::Key_D, "d", Qt::ControlModifier);
    CHECK(offs == 1);
    // A hidden widget does not have shortcuts
    host.label->hide();
    key(&host.top, Qt::Key_S, "s", Qt::ControlModifier);
    CHECK(saved == 1);
    host.label->show();
    key(&host.top, Qt::Key_S, "s", Qt::ControlModifier);
    CHECK(saved == 2);
    // A shortcut in a menu of a menu bar works while the menu is closed, and one of a disabled action does not
    QMenu *file = host.bar->addMenu("&File");
    int opened = 0;
    QAction *open = file->addAction("Open");
    open->setShortcut(QKeySequence("Ctrl+O"));
    QObject::connect(open, &QAction::triggered, [&] { ++opened; });
    QMenu *recent = file->addMenu("Recent");
    int recents = 0;
    QAction *first = recent->addAction("First");
    first->setShortcut(QKeySequence("F6"));
    QObject::connect(first, &QAction::triggered, [&] { ++recents; });
    key(&host.top, Qt::Key_O, "o", Qt::ControlModifier);
    key(&host.top, Qt::Key_F6);
    CHECK(opened == 1 && recents == 1);
    open->setEnabled(false);
    key(&host.top, Qt::Key_O, "o", Qt::ControlModifier);
    CHECK(opened == 1);
    open->setEnabled(true);
    // The focus widget has the key if no action has it
    auto *edit = new QLineEdit(host.label);
    edit->setFocus();
    key(&host.top, Qt::Key_A, "a");
    CHECK(edit->text() == "a");
}

void testMenuGeometry() {
    Host host;
    host.show();
    QMenu menu(&host.top);
    const QFontMetrics fm(menu.font());
    CHECK(menu.isHidden() && menu.isEmpty() && menu.title().isEmpty() && menu.actions().isEmpty() &&
          !menu.sizeHint().isValid());
    CHECK(menu.menuAction()->menu<QMenu *>() == &menu && menu.menuAction()->text().isEmpty());
    menu.setTitle("&Edit");
    CHECK(menu.title() == "&Edit" && menu.menuAction()->text() == "&Edit");
    QAction *open = menu.addAction("Open");
    QAction *save = menu.addAction("Save As");
    save->setShortcut(QKeySequence("Ctrl+S"));
    QAction *separator = menu.addSeparator();
    QAction *wrap = menu.addAction("Wrap");
    wrap->setCheckable(true);
    wrap->setChecked(true);
    QMenu *more = menu.addMenu("More");
    more->addAction("A");
    QAction *disabled = menu.addAction("Disabled");
    disabled->setEnabled(false);
    CHECK(!menu.isEmpty() && open->parent() == &menu && more->parent() == &menu && separator->isSeparator());
    CHECK(menu.actions().size() == 6 && menu.actions()[3] == wrap && menu.actions()[4] == more->menuAction());

    // The same entries as in Qt in the Fusion style: the height of the text and 8 for an entry, 2 for a separator
    const int row = fm.height() + 8;
    const int tab = fm.horizontalAdvance("Ctrl+S");
    const int width = std::max(120, fm.horizontalAdvance("Save As") + 51) + tab;
    CHECK(menu.sizeHint() == QSize(width, row * 5 + 2));
    CHECK(menu.actionGeometry(open) == QRect(0, 0, width, row) &&
          menu.actionGeometry(save) == QRect(0, row, width, row));
    CHECK(menu.actionGeometry(separator) == QRect(0, 2 * row, width, 2) &&
          menu.actionGeometry(wrap) == QRect(0, 2 * row + 2, width, row));
    CHECK(menu.actionGeometry(more->menuAction()) == QRect(0, 3 * row + 2, width, row) &&
          menu.actionGeometry(disabled).y() == 4 * row + 2);
    CHECK(menu.actionAt(QPoint(5, row + 5)) == save && menu.actionAt(QPoint(5, 2 * row + 1)) == separator &&
          menu.actionAt(QPoint(5, 400)) == nullptr && menu.actionAt(QPoint(-1, 5)) == nullptr);
    CHECK(menu.actionGeometry(nullptr).isNull());

    // Hidden actions and separators at the ends or in a row take no room
    QMenu plain(&host.top);
    plain.addSeparator();
    QAction *a = plain.addAction("A");
    QAction *hidden = plain.addAction("Hidden");
    hidden->setVisible(false);
    QAction *s1 = plain.addSeparator();
    QAction *s2 = plain.addSeparator();
    QAction *b = plain.addAction("B");
    QAction *s3 = plain.addSeparator();
    CHECK(plain.sizeHint() == QSize(120, row + 2 + row) && plain.actionGeometry(a) == QRect(0, 0, 120, row) &&
          plain.actionGeometry(hidden).isNull());
    CHECK(plain.actionGeometry(s1) == QRect(0, row, 120, 2) && plain.actionGeometry(s2).isNull() &&
          plain.actionGeometry(b).y() == row + 2 && plain.actionGeometry(s3).isNull());
    hidden->setVisible(true);
    CHECK(plain.sizeHint().height() == 3 * row + 2);
    QMenu onlySeparators(&host.top);
    onlySeparators.addSeparator();
    CHECK(onlySeparators.isEmpty() && !onlySeparators.sizeHint().isValid());

    // Width is the widest text, plus the room for the check box and the margins, and no less than 120
    QMenu wide(&host.top);
    const QString text = "A long menu item text here";
    wide.addAction(text);
    CHECK(wide.sizeHint().width() == fm.horizontalAdvance(text) + 51);
    QAction *check = wide.addAction("check");
    check->setCheckable(true);
    CHECK(wide.sizeHint().width() == fm.horizontalAdvance(text) + 51);
    QMenu withSub(&host.top);
    withSub.addMenu(text)->addAction("x");
    CHECK(withSub.sizeHint().width() == fm.horizontalAdvance(text) + 63);
    // A shortcut adds its width after the widest entry; the room that an entry with a shortcut takes follows the shortcuts before it
    QMenu shortcuts(&host.top);
    shortcuts.addAction("x")->setShortcut(QKeySequence("F1"));
    shortcuts.addAction(text)->setShortcut(QKeySequence("Ctrl+Shift+Q"));
    CHECK(shortcuts.sizeHint().width() ==
          fm.horizontalAdvance(text) + fm.horizontalAdvance("F1") + 51 + fm.horizontalAdvance("Ctrl+Shift+Q"));
    QMenu mnemonic(&host.top);
    mnemonic.addAction("&Open");
    mnemonic.addAction("Fish && Chips");
    CHECK(mnemonic.sizeHint().width() == std::max(120, fm.horizontalAdvance("Fish & Chips") + 51));

    // Changing an action changes the menu
    const QSize before = menu.sizeHint();
    wrap->setVisible(false);
    CHECK(menu.sizeHint().height() == before.height() - row);
    wrap->setVisible(true);
    CHECK(menu.sizeHint() == before);
    save->setShortcut(QKeySequence());
    CHECK(menu.sizeHint().width() == std::max(120, fm.horizontalAdvance("Save As") + 51));
    save->setText("Save with a longer text than before");
    CHECK(menu.sizeHint().width() == fm.horizontalAdvance("Save with a longer text than before") + 51);
    QFont big = menu.font();
    big.setPointSize(big.pointSize() + 6);
    save->setFont(big);
    CHECK(menu.actionGeometry(save).height() == QFontMetrics(big).height() + 8);
    menu.clear();
    CHECK(menu.isEmpty() && menu.actions().isEmpty());
}

// The menu of the tests: the entries of the probes against Qt, and the log of what happens to them.
struct Entries {
    QMenu menu;
    QAction *open, *save, *separator, *wrap, *moreAction, *disabled;
    QMenu *more;
    QAction *alpha, *beta;
    std::string log;

    explicit Entries(QWidget *parent) : menu(parent) {
        open = menu.addAction("Open");
        save = menu.addAction("Save As");
        save->setShortcut(QKeySequence("Ctrl+S"));
        separator = menu.addSeparator();
        wrap = menu.addAction("Wrap");
        wrap->setCheckable(true);
        wrap->setChecked(true);
        more = menu.addMenu("More");
        moreAction = more->menuAction();
        alpha = more->addAction("Alpha");
        beta = more->addAction("Beta Beta");
        disabled = menu.addAction("Disabled");
        disabled->setEnabled(false);
        QObject::connect(&menu, &QMenu::triggered,
                         [this](QAction *a) { log += "menu:" + a->text().toStdString() + " "; });
        QObject::connect(more, &QMenu::triggered,
                         [this](QAction *a) { log += "more:" + a->text().toStdString() + " "; });
        QObject::connect(&menu, &QMenu::hovered,
                         [this](QAction *a) { log += "hover:" + a->text().toStdString() + " "; });
        QObject::connect(&menu, &QMenu::aboutToShow, [this] { log += "show "; });
        QObject::connect(&menu, &QMenu::aboutToHide, [this] { log += "hide "; });
        for (QAction *a : {open, save, wrap, alpha})
            QObject::connect(a, &QAction::triggered,
                             [this, a](bool c) { log += "do:" + a->text().toStdString() + (c ? "(1) " : "(0) "); });
    }
    QPoint at(QAction *a, int dx = 10) {
        return menu.actionGeometry(a).topLeft() + QPoint(dx, a->isSeparator() ? 0 : 10);
    }
    std::string take() {
        std::string out = log;
        log.clear();
        return out;
    }
};

QColor pixelColor(const QImage &image, QPoint p) { return QColor::fromRgb(image.pixel(p)); }

void testMenuPopup() {
    Host host;
    host.show();
    Entries e(&host.top);
    QMenu &menu = e.menu;
    auto *edit = new QLineEdit(host.label);
    edit->setFocus();
    menu.popup(host.top.mapToGlobal(QPoint(50, 40)));
    CHECK(menu.isVisible() && menu.parentWidget() == &host.top && menu.pos() == QPoint(50, 40) &&
          menu.size() == menu.sizeHint());
    CHECK(menu.hasFocus() && menu.activeAction() == nullptr && e.take() == "show ");

    // The look: the frame, the background, a highlighted entry, a separator and a check box
    const QPalette pal = menu.palette();
    QImage image = grab(&host.top);
    const QPoint origin = inWindow(&menu);
    CHECK(pixelColor(image, origin) == pal.color(QPalette::Window).darker(160));
    CHECK(pixelColor(image, origin + QPoint(menu.width() - 1, 40)) == pal.color(QPalette::Window).darker(160));
    CHECK(pixelColor(image, origin + QPoint(3, 3)) == pal.color(QPalette::Base) &&
          pixelColor(image, origin + QPoint(menu.width() - 4, menu.height() - 4)) == pal.color(QPalette::Base));
    const int sepY = menu.actionGeometry(e.separator).top(), row = menu.actionGeometry(e.open).height();
    const QColor sep = pixelColor(image, origin + QPoint(60, sepY));
    CHECK(sep != pal.color(QPalette::Base) &&
          pixelColor(image, origin + QPoint(60, sepY + 1)) == pal.color(QPalette::Base) &&
          pixelColor(image, origin + QPoint(2, sepY)) == pal.color(QPalette::Base) &&
          pixelColor(image, origin + QPoint(menu.width() - 3, sepY)) == pal.color(QPalette::Base));
    CHECK(pixelColor(image, origin + QPoint(5, sepY)) == sep &&
          pixelColor(image, origin + QPoint(menu.width() - 6, sepY)) == sep);
    CHECK(pixelColor(image, origin + QPoint(8, menu.actionGeometry(e.wrap).top() + (row - 14) / 2)) !=
          pal.color(QPalette::Base)); // the border of the check box
    CHECK(pixelColor(image, origin + QPoint(10, menu.actionGeometry(e.moreAction).top() + 3)) ==
          pal.color(QPalette::Base));
    menu.setActiveAction(e.save);
    CHECK(menu.activeAction() == e.save && e.take() == "hover:Save As ");
    image = grab(&host.top);
    const QRect saveRect = menu.actionGeometry(e.save);
    CHECK(pixelColor(image, origin + saveRect.topLeft()) == pal.color(QPalette::Highlight) &&
          pixelColor(image, origin + saveRect.bottomRight()) == pal.color(QPalette::Highlight) &&
          pixelColor(image, origin + QPoint(menu.width() / 2, saveRect.top() - 1)) != pal.color(QPalette::Highlight) &&
          pixelColor(image, origin + QPoint(menu.width() / 2, saveRect.bottom() + 1)) !=
              pal.color(QPalette::Highlight));
    menu.setActiveAction(e.disabled); // a disabled entry is current but not drawn highlighted
    image = grab(&host.top);
    CHECK(pixelColor(image, origin + QPoint(5, menu.actionGeometry(e.disabled).top() + 3)) ==
          pal.color(QPalette::Base));
    menu.setActiveAction(e.separator);
    CHECK(menu.activeAction() == nullptr);
    menu.setActiveAction(nullptr);
    e.take();

    // Closing returns the focus
    menu.hide();
    CHECK(e.take() == "hide " && edit->hasFocus());

    // Near the edges the menu moves: left of the right edge, above the bottom edge
    menu.popup(host.top.mapToGlobal(QPoint(410, 290)));
    CHECK(menu.x() == 420 - menu.width() && menu.y() == 290 - menu.height());
    menu.hide();
    menu.popup(host.top.mapToGlobal(QPoint(10, 10)), e.wrap);
    CHECK(menu.y() == std::max(0, 10 - menu.actionGeometry(e.wrap).top()) && menu.x() == 10);
    CHECK(menu.activeAction() == e.wrap);
    menu.hide();
    QMenu empty(&host.top);
    empty.popup(host.top.mapToGlobal(QPoint(10, 10)));
    CHECK(!empty.isVisible());
    e.take();

    // A menu without a parent opens in the active window
    QMenu loose;
    loose.addAction("x");
    host.top.activateWindow();
    loose.popup(host.top.mapToGlobal(QPoint(30, 30)));
    CHECK(loose.isVisible() == (QApplication::activeWindow() != nullptr));
    loose.hide();
}

void testMenuMouse() {
    Host host;
    host.show();
    Entries e(&host.top);
    QMenu &menu = e.menu;
    menu.popup(host.top.mapToGlobal(QPoint(50, 40)));
    e.take();
    hover(&menu, e.at(e.save));
    CHECK(menu.activeAction() == e.save && e.take() == "hover:Save As ");
    hover(&menu, e.at(e.save, 20));
    CHECK(e.take().empty());
    hover(&menu, e.at(e.open));
    CHECK(menu.activeAction() == e.open);
    hover(&menu, e.at(e.separator, 20));
    CHECK(menu.activeAction() == nullptr);
    hover(&menu, e.at(e.disabled));
    CHECK(menu.activeAction() == e.disabled);
    e.take();
    // Leaving the menu clears the entry
    hover(&host.top, QPoint(300, 200));
    CHECK(menu.activeAction() == nullptr);

    // A click chooses: the menu closes, then the action triggers, then the menu reports it
    click(&menu, e.at(e.open));
    CHECK(menu.isHidden() && e.take() == "hover:Open hide do:Open(0) menu:Open ");
    // A checkable entry toggles
    menu.popup(host.top.mapToGlobal(QPoint(50, 40)));
    e.take();
    click(&menu, e.at(e.wrap));
    CHECK(!e.wrap->isChecked() && menu.isHidden() && e.take() == "hover:Wrap hide do:Wrap(0) menu:Wrap ");
    // A disabled entry, a separator and a submenu do not choose and do not close the menu
    menu.popup(host.top.mapToGlobal(QPoint(50, 40)));
    e.take();
    click(&menu, e.at(e.disabled));
    click(&menu, e.at(e.separator, 20));
    CHECK(menu.isVisible() && e.take().find("do:") == std::string::npos);
    // Press on one entry and release on another chooses the second
    press(&menu, e.at(e.open));
    release(&menu, e.at(e.save));
    CHECK(menu.isHidden() && e.take().find("do:Save As") != std::string::npos);
    // A click outside closes the menu, and goes nowhere else
    menu.popup(host.top.mapToGlobal(QPoint(50, 40)));
    e.take();
    click(&host.top, QPoint(300, 200));
    CHECK(menu.isHidden() && e.take() == "hide ");

    // A submenu opens when the mouse rests on its entry, and closes when the mouse goes to another
    menu.popup(host.top.mapToGlobal(QPoint(50, 40)));
    e.take();
    hover(&menu, e.at(e.moreAction));
    CHECK(!e.more->isVisible());
    CHECK(waitFor([&] { return e.more->isVisible(); }, 1500));
    CHECK(e.more->pos() == menu.pos() + QPoint(menu.width() - 2, menu.actionGeometry(e.moreAction).top()) &&
          e.more->parentWidget() == &host.top);
    CHECK(menu.activeAction() == e.moreAction && e.more->activeAction() == nullptr);
    // Moving into the submenu keeps the entry of the menu current
    hover(&menu, e.at(e.moreAction, menu.width() - 5));
    hover(e.more, e.more->actionGeometry(e.alpha).center());
    CHECK(e.more->activeAction() == e.alpha && menu.activeAction() == e.moreAction && e.more->isVisible());
    // A press in the submenu is not a press outside
    press(e.more, e.more->actionGeometry(e.beta).center());
    CHECK(menu.isVisible() && e.more->isVisible());
    release(e.more, e.more->actionGeometry(e.beta).center());
    CHECK(menu.isHidden() && e.more->isHidden());
    e.take();
    menu.popup(host.top.mapToGlobal(QPoint(50, 40)));
    hover(&menu, e.at(e.moreAction));
    CHECK(waitFor([&] { return e.more->isVisible(); }, 1500));
    hover(&menu, e.at(e.open));
    CHECK(e.more->isHidden() && menu.activeAction() == e.open);
    // A press on the entry of a submenu opens it at once
    press(&menu, e.at(e.moreAction));
    CHECK(e.more->isVisible());
    release(&menu, e.at(e.moreAction));
    CHECK(menu.isVisible() && e.more->isVisible());
    // A click in the submenu closes both, and both report it
    e.take();
    click(e.more, e.more->actionGeometry(e.alpha).center());
    CHECK(menu.isHidden() && e.more->isHidden());
    const std::string log = e.take();
    CHECK(log.find("do:Alpha(0)") != std::string::npos && log.find("more:Alpha") != std::string::npos &&
          log.find("menu:Alpha") != std::string::npos);
    // A click outside closes both
    menu.popup(host.top.mapToGlobal(QPoint(50, 40)));
    press(&menu, e.at(e.moreAction));
    release(&menu, e.at(e.moreAction));
    click(&host.top, QPoint(380, 280));
    CHECK(menu.isHidden() && e.more->isHidden());
    // Near the right edge the submenu opens to the left of the menu
    menu.popup(host.top.mapToGlobal(QPoint(300, 40)));
    press(&menu, e.at(e.moreAction));
    release(&menu, e.at(e.moreAction));
    CHECK(e.more->isVisible() && e.more->x() + e.more->width() == menu.x() + 2);
    menu.hide();
    CHECK(e.more->isHidden());
}

void testMenuKeys() {
    Host host;
    host.show();
    Entries e(&host.top);
    QMenu &menu = e.menu;
    auto *edit = new QLineEdit(host.label);
    edit->setFocus();
    menu.popup(host.top.mapToGlobal(QPoint(50, 40)));
    e.take();
    key(&menu, Qt::Key_Down);
    CHECK(menu.activeAction() == e.open);
    key(&menu, Qt::Key_Down);
    CHECK(menu.activeAction() == e.save);
    key(&menu, Qt::Key_Down); // the separator is skipped
    CHECK(menu.activeAction() == e.wrap);
    key(&menu, Qt::Key_Down);
    CHECK(menu.activeAction() == e.moreAction);
    key(&menu, Qt::Key_Down); // the disabled entry is skipped, and the end goes around
    CHECK(menu.activeAction() == e.open);
    key(&menu, Qt::Key_Up);
    CHECK(menu.activeAction() == e.moreAction);
    key(&menu, Qt::Key_Home);
    CHECK(menu.activeAction() == e.open);
    key(&menu, Qt::Key_End);
    CHECK(menu.activeAction() == e.moreAction);
    menu.setActiveAction(nullptr);
    key(&menu, Qt::Key_Up);
    CHECK(menu.activeAction() == e.moreAction);
    // Enter, Return and Space choose
    key(&menu, Qt::Key_Home);
    e.take();
    key(&menu, Qt::Key_Return);
    CHECK(menu.isHidden() && e.take() == "hide do:Open(0) menu:Open " && edit->hasFocus());
    menu.popup(host.top.mapToGlobal(QPoint(50, 40)));
    key(&menu, Qt::Key_Down);
    key(&menu, Qt::Key_Down);
    e.take();
    key(&menu, Qt::Key_Space);
    CHECK(menu.isHidden() && e.take() == "hide do:Save As(0) menu:Save As ");
    menu.popup(host.top.mapToGlobal(QPoint(50, 40)));
    menu.setActiveAction(e.wrap);
    key(&menu, Qt::Key_Enter);
    CHECK(!e.wrap->isChecked());
    // Return without a current entry does nothing
    menu.popup(host.top.mapToGlobal(QPoint(50, 40)));
    e.take();
    key(&menu, Qt::Key_Return);
    CHECK(menu.isVisible() && e.take().empty());
    // Escape closes
    key(&menu, Qt::Key_Escape);
    CHECK(menu.isHidden() && edit->hasFocus() && e.take() == "hide ");
    // Keys that the menu does not use go nowhere, and the shortcuts of the window are off while it is open
    menu.popup(host.top.mapToGlobal(QPoint(50, 40)));
    e.take();
    key(&menu, Qt::Key_S, "s", Qt::ControlModifier);
    key(&menu, Qt::Key_X, "x");
    CHECK(menu.isVisible() && e.take().empty() && edit->text().isEmpty());
    menu.hide();

    // A submenu: Right opens it with its first entry current, Left and Escape close it
    menu.popup(host.top.mapToGlobal(QPoint(50, 40)));
    menu.setActiveAction(e.moreAction);
    e.take();
    key(&menu, Qt::Key_Right);
    CHECK(e.more->isVisible() && e.more->activeAction() == e.alpha && menu.activeAction() == e.moreAction &&
          e.more->hasFocus());
    key(e.more, Qt::Key_Down);
    CHECK(e.more->activeAction() == e.beta);
    key(e.more, Qt::Key_Left);
    CHECK(e.more->isHidden() && menu.isVisible() && menu.hasFocus() && menu.activeAction() == e.moreAction);
    key(&menu, Qt::Key_Return);
    CHECK(e.more->isVisible() && e.more->activeAction() == e.alpha);
    key(e.more, Qt::Key_Escape);
    CHECK(e.more->isHidden() && menu.isVisible());
    key(&menu, Qt::Key_Right);
    key(e.more, Qt::Key_Return);
    CHECK(menu.isHidden() && e.more->isHidden() && e.take().find("do:Alpha(0)") != std::string::npos);
    // Right on an entry with no submenu does nothing without a menu bar
    menu.popup(host.top.mapToGlobal(QPoint(50, 40)));
    menu.setActiveAction(e.open);
    key(&menu, Qt::Key_Right);
    key(&menu, Qt::Key_Left);
    CHECK(menu.isVisible());
    menu.hide();

    // Mnemonics
    QMenu letters(&host.top);
    std::string chosen;
    for (const char *text : {"&Open", "&Save", "Se&nd", "Fish && Chips", "&Share"}) {
        QAction *a = letters.addAction(text);
        QObject::connect(a, &QAction::triggered, [&chosen, text] {
            chosen += text;
            chosen += ' ';
        });
    }
    letters.addMenu("S&ub")->addAction("x");
    letters.popup(host.top.mapToGlobal(QPoint(50, 40)));
    key(&letters, Qt::Key_O, "o");
    CHECK(chosen == "&Open " && letters.isHidden());
    chosen.clear();
    letters.popup(host.top.mapToGlobal(QPoint(50, 40)));
    key(&letters, Qt::Key_N, "n");
    CHECK(chosen == "Se&nd ");
    chosen.clear();
    letters.popup(host.top.mapToGlobal(QPoint(50, 40)));
    key(&letters, Qt::Key_S, "s"); // two entries have the S: the first is current, and the next S moves on
    CHECK(letters.isVisible() && chosen.empty() && letters.activeAction() != nullptr &&
          letters.activeAction()->text() == "&Save");
    key(&letters, Qt::Key_S, "s");
    CHECK(letters.activeAction()->text() == "&Share");
    key(&letters, Qt::Key_S, "s");
    CHECK(letters.activeAction()->text() == "&Save");
    key(&letters, Qt::Key_Return);
    CHECK(chosen == "&Save ");
    letters.popup(host.top.mapToGlobal(QPoint(50, 40)));
    key(&letters, Qt::Key_U, "u"); // a submenu is opened by its mnemonic
    QMenu *sub = letters.actions().back()->menu<QMenu *>();
    CHECK(sub->isVisible() && sub->activeAction() != nullptr);
    key(sub, Qt::Key_Escape);
    key(&letters, Qt::Key_Z, "z");
    CHECK(letters.isVisible());
    letters.hide();
}

void testMenuExec() {
    Host host;
    host.show();
    Entries e(&host.top);
    QMenu &menu = e.menu;
    QAction *result = nullptr;
    QTimer::singleShot(10, [&] { click(&menu, e.at(e.save)); });
    result = menu.exec(host.top.mapToGlobal(QPoint(50, 40)));
    CHECK(result == e.save && menu.isHidden());
    QTimer::singleShot(10, [&] { key(&menu, Qt::Key_Escape); });
    result = menu.exec(host.top.mapToGlobal(QPoint(50, 40)));
    CHECK(result == nullptr && menu.isHidden());
    QTimer::singleShot(10, [&] { click(&host.top, QPoint(380, 280)); });
    result = menu.exec(host.top.mapToGlobal(QPoint(50, 40)), e.wrap);
    CHECK(result == nullptr);
    QMenu empty(&host.top);
    CHECK(empty.exec(host.top.mapToGlobal(QPoint(50, 40))) == nullptr);
    // At a place with an entry under it
    QTimer::singleShot(10, [&] {
        CHECK(menu.activeAction() == e.wrap);
        key(&menu, Qt::Key_Escape);
    });
    menu.exec(host.top.mapToGlobal(QPoint(100, 100)), e.wrap);
    CHECK(menu.y() == 100 - menu.actionGeometry(e.wrap).top());
}

void testMenuBar() {
    Host host;
    const QFontMetrics fm(host.bar->font());
    CHECK(host.bar->actions().isEmpty() && host.bar->sizeHint() == QSize(0, 0) && host.bar->activeAction() == nullptr);
    CHECK(host.bar->sizePolicy().horizontalPolicy() == QSizePolicy::MinimumExpanding &&
          host.bar->sizePolicy().verticalPolicy() == QSizePolicy::Minimum);
    QMenu *file = host.bar->addMenu("&File");
    QMenu *edit = host.bar->addMenu("Edit");
    QMenu *help = host.bar->addMenu("Help");
    int quits = 0;
    QAction *quit = host.bar->addAction("Quit", [&] { ++quits; });
    CHECK(host.bar->actions().size() == 4 && host.bar->actions()[0] == file->menuAction() &&
          file->parent() == host.bar);
    std::string log;
    QAction *open = file->addAction("Open");
    file->addAction("Save");
    edit->addAction("Copy");
    edit->addAction("Paste");
    help->addAction("About");
    QObject::connect(host.bar, &QMenuBar::triggered,
                     [&](QAction *a) { log += "bar:" + a->text().toStdString() + " "; });
    QObject::connect(host.bar, &QMenuBar::hovered,
                     [&](QAction *a) { log += "hover:" + a->text().toStdString() + " "; });
    QObject::connect(open, &QAction::triggered, [&] { log += "open "; });
    host.show();

    // The titles: 6 pixels from the left and between, the width of the text and 9, as high as the text and 5
    const int height = fm.height() + 5;
    const int wFile = fm.horizontalAdvance("File") + 9, wEdit = fm.horizontalAdvance("Edit") + 9,
              wHelp = fm.horizontalAdvance("Help") + 9;
    const int wQuit = fm.horizontalAdvance("Quit") + 9;
    CHECK(host.bar->actionGeometry(file->menuAction()) == QRect(6, 0, wFile, height));
    CHECK(host.bar->actionGeometry(edit->menuAction()) == QRect(6 + wFile + 6, 0, wEdit, height));
    CHECK(host.bar->actionGeometry(help->menuAction()) == QRect(6 + wFile + 6 + wEdit + 6, 0, wHelp, height));
    CHECK(host.bar->sizeHint() == QSize(6 + wFile + wEdit + wHelp + wQuit + 3 * 6, height));
    CHECK(host.bar->minimumSizeHint() == QSize(std::min({wFile, wEdit, wHelp, wQuit}), height));
    CHECK(host.bar->size() == QSize(420, height) && host.bar->actionGeometry(nullptr).isNull());
    CHECK(host.bar->actionAt(QPoint(10, 5)) == file->menuAction() && host.bar->actionAt(QPoint(2, 5)) == nullptr &&
          host.bar->actionAt(QPoint(6 + wFile + 3, 5)) == nullptr && host.bar->actionAt(QPoint(400, 5)) == nullptr);

    // The look: the background with a line under it, and the title that is current
    const QPalette pal = host.bar->palette();
    QImage image = grab(&host.top);
    CHECK(pixelColor(image, QPoint(200, 5)) == pal.color(QPalette::Window) &&
          pixelColor(image, QPoint(2, height - 1)) == pal.color(QPalette::Window).darker(112) &&
          pixelColor(image, QPoint(400, height - 1)) == pal.color(QPalette::Window).darker(112) &&
          pixelColor(image, QPoint(200, height)) != pal.color(QPalette::Window).darker(112));
    hover(host.bar, QPoint(10, 5));
    CHECK(host.bar->activeAction() == file->menuAction() && log == "hover:&File ");
    log.clear();
    image = grab(&host.top);
    CHECK(pixelColor(image, QPoint(8, 5)) == pal.color(QPalette::Highlight) &&
          pixelColor(image, QPoint(6, 5)) == pal.color(QPalette::Highlight).darker(125) &&
          pixelColor(image, QPoint(6, 0)) == pal.color(QPalette::Highlight).darker(125) &&
          pixelColor(image, QPoint(6 + wFile - 1, height - 1)) == pal.color(QPalette::Highlight).darker(125) &&
          pixelColor(image, QPoint(5, 5)) == pal.color(QPalette::Window));
    hover(host.bar, QPoint(6 + wFile + 3, 5)); // between two titles
    CHECK(host.bar->activeAction() == nullptr);
    hover(host.bar, QPoint(10, 5));
    hover(&host.top, QPoint(200, 200)); // away from the bar
    CHECK(host.bar->activeAction() == nullptr);
    log.clear();

    // A click opens the menu below the title, and a click on the title again closes it
    click(host.bar, QPoint(10, 5));
    const QRect fileRect = host.bar->actionGeometry(file->menuAction());
    CHECK(file->isVisible() && file->pos() == QPoint(fileRect.left() + 2, height + 2) &&
          file->parentWidget() == &host.top);
    CHECK(host.bar->activeAction() == file->menuAction() && file->activeAction() == nullptr);
    click(host.bar, QPoint(10, 5));
    CHECK(file->isHidden() && host.bar->activeAction() != file->menuAction());
    click(host.bar, QPoint(10, 5));
    CHECK(file->isVisible());
    // With a menu open, moving to another title opens that menu in its place
    hover(host.bar, QPoint(host.bar->actionGeometry(edit->menuAction()).center()));
    CHECK(file->isHidden() && edit->isVisible() && host.bar->activeAction() == edit->menuAction() &&
          edit->x() == host.bar->actionGeometry(edit->menuAction()).left() + 2);
    hover(host.bar, QPoint(6 + wFile + 3, 5)); // between: the menu stays
    CHECK(edit->isVisible());
    // The title that is open stays highlighted while the mouse is in the menu
    hover(edit, QPoint(10, 10));
    CHECK(host.bar->activeAction() == edit->menuAction());
    // A click outside closes the menu and the highlight goes
    click(&host.top, QPoint(300, 200));
    CHECK(edit->isHidden() && host.bar->activeAction() == nullptr);
    // A click on an empty part of the bar closes it too
    click(host.bar, QPoint(10, 5));
    CHECK(file->isVisible());
    click(host.bar, QPoint(350, 5));
    CHECK(file->isHidden());
    // Choosing an action in a menu: the bar reports it as well
    log.clear();
    click(host.bar, QPoint(10, 5));
    click(file, file->actionGeometry(open).center());
    CHECK(file->isHidden() && host.bar->activeAction() == nullptr && log.find("open") != std::string::npos &&
          log.find("bar:Open") != std::string::npos);
    // A title without a menu triggers when it is clicked
    log.clear();
    click(host.bar, host.bar->actionGeometry(quit).center());
    CHECK(quits == 1 && log.find("bar:Quit") != std::string::npos);
    // Disabled titles do nothing
    edit->menuAction()->setEnabled(false);
    click(host.bar, host.bar->actionGeometry(edit->menuAction()).center());
    CHECK(edit->isHidden());
    edit->menuAction()->setEnabled(true);

    // Keys in a menu: Right and Left go to the next and the previous menu, around
    click(host.bar, QPoint(10, 5));
    key(file, Qt::Key_Down);
    CHECK(file->activeAction() == open);
    key(file, Qt::Key_Right);
    CHECK(file->isHidden() && edit->isVisible() && edit->activeAction() == edit->actions()[0] &&
          host.bar->activeAction() == edit->menuAction());
    key(edit, Qt::Key_Right);
    CHECK(help->isVisible() && edit->isHidden());
    key(help, Qt::Key_Right); // Quit has no menu: around to the first
    CHECK(file->isVisible() && help->isHidden());
    key(file, Qt::Key_Left);
    CHECK(help->isVisible() && file->isHidden());
    key(help, Qt::Key_Left);
    key(edit, Qt::Key_Escape);
    CHECK(edit->isHidden() && host.bar->activeAction() == nullptr);
    // In a submenu Right goes on to the next menu of the bar too
    QMenu *sub = file->addMenu("Recent");
    sub->addAction("x");
    click(host.bar, QPoint(10, 5));
    file->setActiveAction(sub->menuAction());
    key(file, Qt::Key_Right);
    CHECK(sub->isVisible());
    key(sub, Qt::Key_Left);
    CHECK(sub->isHidden() && file->isVisible());
    key(file, Qt::Key_Escape);

    // Alt and the letter of the mnemonic open the menu
    key(&host.top, Qt::Key_F, "f", Qt::AltModifier);
    CHECK(file->isVisible() && file->activeAction() == file->actions()[0] &&
          host.bar->activeAction() == file->menuAction());
    key(file, Qt::Key_Escape);
    key(&host.top, Qt::Key_E, "e", Qt::AltModifier);
    CHECK(edit->isHidden());        // Edit has no mnemonic
    key(&host.top, Qt::Key_F, "f"); // without Alt it is for the focus widget
    CHECK(file->isHidden());

    // Clearing
    host.bar->clear();
    CHECK(host.bar->actions().isEmpty() && host.bar->sizeHint() == QSize(0, 0));
}

} // namespace

int main(int argc, char **argv) {
    ::setenv("QT_QPA_PLATFORM", "offscreen", 1);
    ::setenv("QT_SCALE_FACTOR", "1", 1);
    QApplication app(argc, argv);
    app.styleHints()->setColorScheme(Qt::ColorScheme::Light);
    testWidgetActions();
    testShortcuts();
    testMenuGeometry();
    testMenuPopup();
    testMenuMouse();
    testMenuKeys();
    testMenuExec();
    testMenuBar();
    if (g_failures == 0)
        std::puts("all menu tests passed");
    return g_failures == 0 ? 0 : 1;
}
