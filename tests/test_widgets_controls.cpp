// MIT License - see LICENSE
//
// Tests for the controls of Nt Widgets on the offscreen platform: radio buttons, line edits, sliders, progress bars,
// spin boxes, combo boxes, group boxes and tab widgets. The tests play the window system through
// QWindowSystemInterface.
#include <QtCore>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtGui/qstylehints.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qcheckbox.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qdial.h>
#include <QtWidgets/qgroupbox.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlcdnumber.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qprogressbar.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qradiobutton.h>
#include <QtWidgets/qslider.h>
#include <QtWidgets/qspinbox.h>
#include <QtWidgets/qsplitter.h>
#include <QtWidgets/qstatusbar.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qtabwidget.h>
#include <QtWidgets/qtoolbutton.h>
#include <QtWidgets/qtooltip.h>

#include "platforms/offscreen/qoffscreenintegration_p.h"
#include "qpa/qwindowsysteminterface_p.h"
#include "qtooltip_p.h"

#include <cmath>
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
void drag(QWidget *w, QPoint local) { mouse(w, QEvent::MouseMove, local, Qt::NoButton, Qt::LeftButton); }
void release(QWidget *w, QPoint local) { mouse(w, QEvent::MouseButtonRelease, local, Qt::LeftButton, Qt::NoButton); }
void click(QWidget *w, QPoint local) {
    press(w, local);
    release(w, local);
}

void key(QWidget *w, int k, const QString &text = QString(), Qt::KeyboardModifiers mods = Qt::NoModifier) {
    QWindow *win = w->window()->windowHandle();
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyPress, k, mods, text);
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyRelease, k, mods);
}

void type(QWidget *w, const QString &text) {
    for (QChar c : text)
        key(w, c.unicode() >= 'a' && c.unicode() <= 'z' ? c.unicode() - 32 : c.unicode(), QString(c));
}

void wheel(QWidget *w, QPoint local, int notches) {
    const QPoint inWindow = w->mapTo(w->window(), local);
    QWindowSystemInterface::handleWheelEvent(w->window()->windowHandle(), QPointF(inWindow), QPointF(inWindow),
                                             QPoint(), QPoint(0, 120 * notches), Qt::NoModifier);
}

int inkCount(const QImage &image, const QRect &area, QRgb background) {
    int n = 0;
    for (int y = area.top(); y <= area.bottom(); ++y)
        for (int x = area.left(); x <= area.right(); ++x)
            if (image.pixel(x, y) != background)
                ++n;
    return n;
}

// Tests that look at the pixels of text need a font; some machines (a minimal container) have none.
bool haveRealFonts() { return QFontMetrics(QFont("sans-serif", 10)).inFont(QChar('A')); }

QPoint inWindow(QWidget *w, QPoint local = QPoint(0, 0)) { return w->mapTo(w->window(), local); }

} // namespace

static void testRadioButtons() {
    QWidget top;
    QRadioButton a("One", &top), b("Two", &top), c("Three", &top);
    auto *layout = new QVBoxLayout(&top);
    layout->addWidget(&a);
    layout->addWidget(&b);
    layout->addWidget(&c);
    top.show();
    settle(&top);

    CHECK(a.isCheckable() && a.autoExclusive() && !a.isChecked());
    CHECK(a.sizeHint().width() > 14 && a.sizeHint().height() >= 14 &&
          a.sizePolicy().verticalPolicy() == QSizePolicy::Fixed);
    int toggles = 0;
    QObject::connect(&a, &QAbstractButton::toggled, [&](bool) { ++toggles; });
    a.setChecked(true);
    CHECK(a.isChecked() && toggles == 1);
    b.setChecked(true);
    CHECK(!a.isChecked() && b.isChecked() && toggles == 2); // checking one unchecks the other, with a signal
    // The checked button of a group cannot be unchecked
    b.setChecked(false);
    CHECK(b.isChecked());
    click(&b, QPoint(5, 5));
    CHECK(b.isChecked());

    // Clicking another one moves the choice
    click(&c, QPoint(5, 5));
    CHECK(c.isChecked() && !b.isChecked());
    click(&a, QPoint(5, 5));
    CHECK(a.isChecked() && !c.isChecked());

    // Only the indicator and the text react to the mouse
    a.setChecked(false);
    QRadioButton lone("x", &top);
    lone.setGeometry(0, 0, 10, 10);
    CHECK(lone.autoExclusive());

    // Arrow keys move the choice
    a.setFocus();
    a.setChecked(true);
    key(&a, Qt::Key_Down);
    CHECK(b.isChecked() && b.hasFocus());
    key(&b, Qt::Key_Right);
    CHECK(c.isChecked());
    key(&c, Qt::Key_Down); // wraps around
    CHECK(a.isChecked());
    key(&a, Qt::Key_Up);
    CHECK(c.isChecked());

    // Radio buttons of different parents are different groups
    QWidget other(&top);
    QRadioButton d("D", &other), e("E", &other);
    d.setChecked(true);
    CHECK(d.isChecked() && c.isChecked());
    e.setChecked(true);
    CHECK(e.isChecked() && !d.isChecked() && c.isChecked());

    // A check box is not exclusive
    QCheckBox box("x", &top);
    CHECK(!box.autoExclusive());
    box.setAutoExclusive(true);
    CHECK(box.autoExclusive());

    // Painting: the checked indicator has a dot, the unchecked one has not
    const QImage image = grab(&top);
    const QRect ia(inWindow(&a, QPoint(0, (a.height() - 14) / 2)), QSize(14, 14)),
        ic(inWindow(&c, QPoint(0, (c.height() - 14) / 2)), QSize(14, 14));
    const QRgb base = top.palette().color(QPalette::Base).rgb();
    CHECK(image.pixel(ic.center()) != base && image.pixel(ia.center()) == base);
}

static void testLineEdit() {
    QWidget top;
    QLineEdit edit(&top);
    auto *layout = new QVBoxLayout(&top);
    layout->addWidget(&edit);
    top.resize(240, 80);
    top.show();
    settle(&top);
    edit.setFocus();

    CHECK(edit.text().isEmpty() && edit.cursorPosition() == 0 && edit.maxLength() == 32767 && !edit.isReadOnly());
    CHECK(edit.hasFrame() && edit.echoMode() == QLineEdit::Normal && edit.focusPolicy() == Qt::StrongFocus);
    CHECK(edit.sizeHint().height() >= edit.fontMetrics().height() && edit.sizeHint().width() > 60);
    CHECK(edit.minimumSizeHint().width() < edit.sizeHint().width());

    QStringList log;
    QObject::connect(&edit, &QLineEdit::textChanged, [&](const QString &t) { log << "changed:" + t; });
    QObject::connect(&edit, &QLineEdit::textEdited, [&](const QString &t) { log << "edited:" + t; });
    QObject::connect(&edit, &QLineEdit::returnPressed, [&] { log << "return"; });
    QObject::connect(&edit, &QLineEdit::editingFinished, [&] { log << "finished"; });
    QObject::connect(&edit, &QLineEdit::cursorPositionChanged,
                     [&](int o, int n) { log << QString("cursor:%1>%2").arg(o).arg(n); });
    QObject::connect(&edit, &QLineEdit::selectionChanged, [&] { log << "selection"; });

    type(&edit, "abc");
    CHECK(edit.text() == QString("abc") && edit.cursorPosition() == 3);
    CHECK(log.contains("changed:abc") && log.contains("edited:abc") && log.contains("cursor:2>3"));
    log.clear();
    edit.setText("hello world"); // programmatic: no textEdited
    CHECK(edit.text() == QString("hello world") && edit.cursorPosition() == 11);
    CHECK(log.contains("changed:hello world") && !log.contains("edited:hello world"));
    log.clear();

    // Navigation and deletion
    key(&edit, Qt::Key_Home);
    CHECK(edit.cursorPosition() == 0);
    key(&edit, Qt::Key_Right);
    key(&edit, Qt::Key_Right);
    CHECK(edit.cursorPosition() == 2);
    key(&edit, Qt::Key_Delete);
    CHECK(edit.text() == QString("helo world"));
    key(&edit, Qt::Key_Backspace);
    CHECK(edit.text() == QString("hlo world") && edit.cursorPosition() == 1);
    key(&edit, Qt::Key_End);
    CHECK(edit.cursorPosition() == 9);
    key(&edit, Qt::Key_Right); // stays at the end
    CHECK(edit.cursorPosition() == 9);
    key(&edit, Qt::Key_Left, QString(), Qt::ControlModifier);
    CHECK(edit.cursorPosition() == 4);
    key(&edit, Qt::Key_Left, QString(), Qt::ControlModifier);
    CHECK(edit.cursorPosition() == 0);
    key(&edit, Qt::Key_Right, QString(), Qt::ControlModifier);
    CHECK(edit.cursorPosition() == 3);

    // Selection with Shift and Ctrl+A
    edit.setCursorPosition(0);
    key(&edit, Qt::Key_Right, QString(), Qt::ShiftModifier);
    key(&edit, Qt::Key_Right, QString(), Qt::ShiftModifier);
    CHECK(edit.hasSelectedText() && edit.selectedText() == QString("hl") && edit.selectionStart() == 0 &&
          edit.selectionEnd() == 2);
    type(&edit, "X"); // typing replaces the selection
    CHECK(edit.text() == QString("Xo world") && !edit.hasSelectedText());
    key(&edit, Qt::Key_A, "a", Qt::ControlModifier);
    CHECK(edit.selectedText() == QString("Xo world"));
    key(&edit, Qt::Key_Left); // collapses to the start
    CHECK(!edit.hasSelectedText() && edit.cursorPosition() == 0);
    edit.selectAll();
    key(&edit, Qt::Key_Backspace);
    CHECK(edit.text().isEmpty());
    edit.setText("abcdef");
    edit.setSelection(1, 3);
    CHECK(edit.selectedText() == QString("bcd"));
    edit.setSelection(4, -2);
    CHECK(edit.selectedText() == QString("cd") && edit.cursorPosition() == 2);
    edit.deselect();
    CHECK(!edit.hasSelectedText() && edit.cursorPosition() == 2);
    edit.home(true);
    CHECK(edit.selectedText() == QString("ab"));
    edit.end(true);
    CHECK(edit.selectedText() == QString("abcdef") || edit.selectedText() == QString("cdef") || edit.hasSelectedText());

    // Return and focus out finish the editing
    log.clear();
    key(&edit, Qt::Key_Return);
    CHECK(log.contains("return") && log.contains("finished"));
    log.clear();
    QLineEdit second(&top);
    second.setGeometry(0, 0, 20, 20);
    second.setFocus();
    CHECK(log.contains("finished"));
    edit.setFocus();

    // Keys that are not text are not taken
    edit.setText("a");
    key(&edit, Qt::Key_F5);
    key(&edit, Qt::Key_C, "c", Qt::ControlModifier);
    CHECK(edit.text() == QString("a"));

    // Limits, read-only, echo modes
    edit.setMaxLength(4);
    edit.setText("abcdefgh");
    CHECK(edit.text() == QString("abcd"));
    type(&edit, "z");
    CHECK(edit.text() == QString("abcd"));
    edit.setMaxLength(100);
    edit.setReadOnly(true);
    type(&edit, "z");
    key(&edit, Qt::Key_Backspace);
    CHECK(edit.text() == QString("abcd") && edit.isReadOnly());
    key(&edit, Qt::Key_Home);
    CHECK(edit.cursorPosition() == 0);
    edit.setReadOnly(false);
    edit.setEchoMode(QLineEdit::Password);
    CHECK(edit.text() == QString("abcd") && edit.displayText() == QString(4, QChar(0x2022)));
    edit.setEchoMode(QLineEdit::NoEcho);
    CHECK(edit.displayText().isEmpty());
    edit.setEchoMode(QLineEdit::Normal);
    CHECK(edit.displayText() == QString("abcd"));
    edit.clear();
    edit.insert("xy");
    edit.insert("z");
    CHECK(edit.text() == QString("xyz"));
    edit.setText("abc");
    edit.setCursorPosition(1);
    edit.backspace();
    edit.del();
    CHECK(edit.text() == QString("c"));

    // The mouse puts the cursor, selects by dragging, and double click selects a word
    edit.setText("one two three");
    settle(&top);
    const QFontMetrics fm = edit.fontMetrics();
    const int left = 4; // the text starts at the left margin of the frame
    auto xAt = [&](int pos) { return left + fm.horizontalAdvance(edit.text().left(pos)) + 1; };
    click(&edit, QPoint(xAt(4), edit.height() / 2));
    CHECK(edit.cursorPosition() == 4);
    press(&edit, QPoint(xAt(0), edit.height() / 2));
    drag(&edit, QPoint(xAt(3), edit.height() / 2));
    release(&edit, QPoint(xAt(3), edit.height() / 2));
    CHECK(edit.selectedText() == QString("one"));
    mouse(&edit, QEvent::MouseButtonDblClick, QPoint(xAt(5), edit.height() / 2));
    mouse(&edit, QEvent::MouseButtonRelease, QPoint(xAt(5), edit.height() / 2), Qt::LeftButton, Qt::NoButton);
    CHECK(edit.selectedText() == QString("two"));
    click(&edit, QPoint(edit.width() - 3, edit.height() / 2));
    CHECK(edit.cursorPosition() == 13);
    CHECK(edit.cursorPositionAt(QPoint(0, 5)) == 0);

    // A text longer than the line edit scrolls to keep the cursor in view
    edit.setText(QString(80, QChar('m')));
    settle(&top);
    CHECK(edit.cursorPosition() == 80);
    const QImage image = grab(&top);
    const QRect area(inWindow(&edit) + QPoint(edit.width() - 12, 3), QSize(8, edit.height() - 6));
    CHECK(inkCount(image, area, edit.palette().color(QPalette::Base).rgb()) > 0); // text reaches the right end
    edit.home();
    CHECK(edit.cursorPosition() == 0);

    // Placeholder text is drawn while empty, in another color than the text
    edit.clear();
    edit.setPlaceholderText("Type here");
    CHECK(edit.placeholderText() == QString("Type here"));
    const QImage withHint = grab(&top);
    const QRect text(inWindow(&edit) + QPoint(3, 3), QSize(80, edit.height() - 6));
    CHECK(!haveRealFonts() || inkCount(withHint, text, edit.palette().color(QPalette::Base).rgb()) > 20);
    edit.setFrame(false);
    CHECK(!edit.hasFrame());
    edit.setFrame(true);

    // A disabled line edit takes no keys
    edit.setText("a");
    edit.setEnabled(false);
    type(&edit, "b");
    CHECK(edit.text() == QString("a"));
    edit.setEnabled(true);
    QLineEdit init("start", &top);
    CHECK(init.text() == QString("start") && init.cursorPosition() == 5);
}

static void testSlider() {
    QWidget top;
    QSlider slider(Qt::Horizontal, &top);
    auto *layout = new QVBoxLayout(&top);
    layout->addWidget(&slider);
    top.resize(200, 60);
    top.show();
    settle(&top);
    slider.setFocus();

    CHECK(slider.minimum() == 0 && slider.maximum() == 99 && slider.value() == 0 && slider.singleStep() == 1 &&
          slider.pageStep() == 10);
    CHECK(slider.orientation() == Qt::Horizontal && slider.hasTracking() && !slider.isSliderDown());
    CHECK(slider.sizePolicy().horizontalPolicy() == QSizePolicy::Expanding &&
          slider.sizePolicy().verticalPolicy() == QSizePolicy::Fixed);
    CHECK(slider.sizeHint().height() == 15 && slider.sizeHint().width() >= 80 && slider.minimumSizeHint().width() < 80);
    CHECK(slider.width() == 200 - 22 && slider.height() == 15);

    QList<int> values, moved;
    int pressed = 0, released = 0;
    QObject::connect(&slider, &QSlider::valueChanged, [&](int v) { values << v; });
    QObject::connect(&slider, &QSlider::sliderMoved, [&](int v) { moved << v; });
    QObject::connect(&slider, &QSlider::sliderPressed, [&] { ++pressed; });
    QObject::connect(&slider, &QSlider::sliderReleased, [&] { ++released; });

    slider.setRange(0, 100);
    slider.setValue(25);
    CHECK(slider.value() == 25 && values == QList<int>({25}));
    slider.setValue(25);
    CHECK(values.size() == 1);
    slider.setValue(500);
    CHECK(slider.value() == 100);
    slider.setValue(-5);
    CHECK(slider.value() == 0);
    slider.setMaximum(50);
    slider.setValue(50);
    slider.setMinimum(60); // the maximum follows
    CHECK(slider.minimum() == 60 && slider.maximum() == 60 && slider.value() == 60);
    slider.setRange(0, 100);
    slider.setValue(50);
    values.clear();

    // Keys
    key(&slider, Qt::Key_Right);
    CHECK(slider.value() == 51);
    key(&slider, Qt::Key_Up);
    CHECK(slider.value() == 52);
    key(&slider, Qt::Key_Left);
    key(&slider, Qt::Key_Down);
    CHECK(slider.value() == 50);
    key(&slider, Qt::Key_PageUp);
    CHECK(slider.value() == 60);
    key(&slider, Qt::Key_PageDown);
    key(&slider, Qt::Key_PageDown);
    CHECK(slider.value() == 40);
    key(&slider, Qt::Key_Home);
    CHECK(slider.value() == 0);
    key(&slider, Qt::Key_End);
    CHECK(slider.value() == 100);
    key(&slider, Qt::Key_Right);
    CHECK(slider.value() == 100);
    key(&slider, Qt::Key_F1);
    slider.setSingleStep(2);
    slider.setValue(10);
    key(&slider, Qt::Key_Right);
    CHECK(slider.value() == 12);
    slider.setSingleStep(1);
    slider.setPageStep(25);
    key(&slider, Qt::Key_PageUp);
    CHECK(slider.value() == 37);
    slider.setPageStep(10);

    // Wheel: up increases, by three steps a notch
    slider.setValue(50);
    wheel(&slider, QPoint(50, 10), 1);
    CHECK(slider.value() == 53);
    wheel(&slider, QPoint(50, 10), -2);
    CHECK(slider.value() == 47);

    // The mouse: a click next to the handle moves by a page
    slider.setValue(50);
    values.clear();
    click(&slider, QPoint(slider.width() - 3, 10));
    CHECK(slider.value() == 60 && values == QList<int>({60}));
    click(&slider, QPoint(2, 10));
    CHECK(slider.value() == 50);

    // ... and dragging the handle follows the mouse over the groove
    QStyleOptionSlider option;
    option.initFrom(&slider);
    option.minimum = slider.minimum();
    option.maximum = slider.maximum();
    option.sliderPosition = slider.value();
    const QRect handle = slider.style()->subElementRect(QStyle::SE_SliderHandle, &option, &slider);
    CHECK(handle.width() == 16 && handle.contains(handle.center()) && handle.center().x() > slider.width() / 3 &&
          handle.center().x() < 2 * slider.width() / 3);
    values.clear();
    press(&slider, handle.center());
    CHECK(slider.isSliderDown() && pressed == 1);
    drag(&slider, QPoint(slider.width() - 8, handle.center().y()));
    CHECK(slider.value() == 100 && !moved.isEmpty() && moved.last() == 100);
    drag(&slider, QPoint(-30, handle.center().y()));
    CHECK(slider.value() == 0);
    drag(&slider, QPoint(slider.width() / 2, handle.center().y()));
    CHECK(slider.value() > 40 && slider.value() < 60);
    release(&slider, QPoint(slider.width() / 2, handle.center().y()));
    CHECK(!slider.isSliderDown() && released == 1);

    // Without tracking the value changes on release
    slider.setTracking(false);
    slider.setValue(0);
    values.clear();
    option.sliderPosition = 0;
    const QRect h0 = slider.style()->subElementRect(QStyle::SE_SliderHandle, &option, &slider);
    press(&slider, h0.center());
    drag(&slider, QPoint(slider.width() - 8, h0.center().y()));
    CHECK(slider.value() == 0 && slider.sliderPosition() == 100 && values.isEmpty());
    release(&slider, QPoint(slider.width() - 8, h0.center().y()));
    CHECK(slider.value() == 100 && values == QList<int>({100}));
    slider.setTracking(true);

    // The handle is drawn where the value is
    slider.setValue(0);
    QImage image = grab(&top);
    const QRgb highlight = top.palette().color(QPalette::Highlight).rgb();
    const QPoint origin = inWindow(&slider);
    CHECK(image.pixel(origin + QPoint(slider.width() / 2, slider.height() / 2)) != highlight);
    slider.setValue(100);
    image = grab(&top);
    CHECK(image.pixel(origin + QPoint(slider.width() / 2, slider.height() / 2)) == highlight);

    // A vertical slider has its smallest value at the bottom
    QSlider vertical(Qt::Vertical, &top);
    vertical.setGeometry(150, 0, 20, 120);
    vertical.show();
    vertical.setRange(0, 10);
    CHECK(vertical.orientation() == Qt::Vertical && vertical.sizePolicy().verticalPolicy() == QSizePolicy::Expanding);
    CHECK(vertical.sizeHint().width() == 15 && vertical.sizeHint().height() >= 80);
    vertical.setValue(0);
    QStyleOptionSlider vo;
    vo.initFrom(&vertical);
    vo.orientation = Qt::Vertical;
    vo.minimum = 0;
    vo.maximum = 10;
    vo.sliderPosition = 0;
    const QRect lowHandle = vertical.style()->subElementRect(QStyle::SE_SliderHandle, &vo, &vertical);
    vo.sliderPosition = 10;
    const QRect highHandle = vertical.style()->subElementRect(QStyle::SE_SliderHandle, &vo, &vertical);
    CHECK(lowHandle.bottom() == vertical.height() - 1 && highHandle.top() == 0);
    click(&vertical, QPoint(10, 3)); // above the handle: up
    CHECK(vertical.value() == 10);
    click(&vertical, QPoint(10, vertical.height() - 3));
    CHECK(vertical.value() == 0);
    vertical.setOrientation(Qt::Horizontal);
    CHECK(vertical.sizePolicy().horizontalPolicy() == QSizePolicy::Expanding);
}

static void testProgressBar() {
    QWidget top;
    QProgressBar bar(&top);
    auto *layout = new QVBoxLayout(&top);
    layout->addWidget(&bar);
    top.resize(200, 60);
    top.show();
    settle(&top);

    CHECK(bar.minimum() == 0 && bar.maximum() == 100 && bar.value() == -1 && bar.isTextVisible());
    CHECK(bar.format() == QString("%p%") && bar.text().isEmpty());
    CHECK(bar.sizeHint().height() > bar.fontMetrics().height() && bar.sizeHint().width() > 60);
    CHECK(bar.sizePolicy().horizontalPolicy() == QSizePolicy::Expanding);

    QList<int> values;
    QObject::connect(&bar, &QProgressBar::valueChanged, [&](int v) { values << v; });
    bar.setValue(40);
    CHECK(bar.value() == 40 && bar.text() == QString("40%") && values == QList<int>({40}));
    bar.setValue(40);
    CHECK(values.size() == 1);
    bar.setValue(150); // outside the range: ignored
    CHECK(bar.value() == 40);
    bar.setRange(0, 200);
    CHECK(bar.value() == 40 && bar.text() == QString("20%"));
    bar.setFormat("%v of %m (%p%)");
    CHECK(bar.text() == QString("40 of 200 (20%)"));
    bar.setRange(10, 30);
    CHECK(bar.value() == 40 || bar.value() == 9); // 40 is out of range now: the bar is reset
    bar.setValue(20);
    CHECK(bar.text() == QString("20 of 20 (50%)"));
    bar.reset();
    CHECK(bar.value() == 9 && bar.text().isEmpty());
    bar.setMaximum(5);
    CHECK(bar.maximum() == 10 || bar.maximum() == 5);
    bar.setMinimum(100);
    CHECK(bar.minimum() == 100 && bar.maximum() == 100);
    bar.setRange(0, 100);
    bar.setFormat("%p%");

    // The bar is filled in proportion to the value, and the text is on it
    bar.setValue(50);
    QImage image = grab(&top);
    const QPoint origin = inWindow(&bar);
    const QRgb highlight = top.palette().color(QPalette::Highlight).rgb();
    CHECK(image.pixel(origin + QPoint(bar.width() / 4, 3)) == highlight);
    CHECK(image.pixel(origin + QPoint(3 * bar.width() / 4, 3)) != highlight);
    bar.setValue(100);
    image = grab(&top);
    CHECK(image.pixel(origin + QPoint(bar.width() - 4, 3)) == highlight);
    bar.setValue(0);
    image = grab(&top);
    CHECK(image.pixel(origin + QPoint(3, 3)) != highlight);
    const QRect textArea(origin + QPoint(bar.width() / 2 - 15, 4), QSize(30, bar.height() - 8));
    const int withText = inkCount(image, textArea, image.pixel(textArea.topLeft()));
    bar.setTextVisible(false);
    CHECK(!bar.isTextVisible());
    image = grab(&top);
    CHECK((!haveRealFonts() || withText > 5) && inkCount(image, textArea, image.pixel(textArea.topLeft())) == 0);
    bar.setTextVisible(true);
}

static void testSpinBox() {
    QWidget top;
    QSpinBox spin(&top);
    auto *layout = new QVBoxLayout(&top);
    layout->addWidget(&spin);
    top.resize(200, 60);
    top.show();
    settle(&top);

    CHECK(spin.minimum() == 0 && spin.maximum() == 99 && spin.value() == 0 && spin.singleStep() == 1 &&
          !spin.wrapping());
    CHECK(spin.text() == QString("0") && spin.cleanText() == QString("0"));
    CHECK(spin.sizeHint().height() >= spin.fontMetrics().height() && spin.sizeHint().width() > 30);

    QList<int> values;
    int finished = 0;
    QObject::connect(&spin, &QSpinBox::valueChanged, [&](int v) { values << v; });
    QObject::connect(&spin, &QSpinBox::editingFinished, [&] { ++finished; });

    spin.setValue(5);
    CHECK(spin.value() == 5 && spin.text() == QString("5") && values == QList<int>({5}));
    spin.setValue(500);
    CHECK(spin.value() == 99);
    spin.setRange(10, 20);
    CHECK(spin.value() == 20 && spin.minimum() == 10 && spin.maximum() == 20);
    spin.setMaximum(5);
    CHECK(spin.minimum() == 5 && spin.maximum() == 5);
    spin.setMinimum(30);
    CHECK(spin.maximum() == 30);
    spin.setRange(0, 10);
    spin.setValue(5);
    values.clear();

    // Steps
    spin.stepUp();
    spin.stepBy(2);
    spin.stepDown();
    CHECK(spin.value() == 7 && values == QList<int>({6, 8, 7}));
    spin.setSingleStep(3);
    spin.stepUp();
    CHECK(spin.value() == 10);
    spin.stepUp(); // stops at the end
    CHECK(spin.value() == 10);
    spin.setSingleStep(1);
    spin.setWrapping(true);
    spin.stepUp();
    CHECK(spin.value() == 0 && spin.wrapping());
    spin.stepDown();
    CHECK(spin.value() == 10);
    spin.setWrapping(false);
    spin.setSingleStep(1);
    spin.setValue(5);

    // Prefix and suffix
    spin.setPrefix("$");
    spin.setSuffix(" pt");
    CHECK(spin.text() == QString("$5 pt") && spin.cleanText() == QString("5") && spin.prefix() == QString("$") &&
          spin.suffix() == QString(" pt"));
    spin.setPrefix(QString());
    spin.setSuffix(QString());

    // Keys on the line edit step the value, letters are not taken
    QLineEdit *edit = nullptr;
    for (QObject *child : spin.children())
        if (auto *e = dynamic_cast<QLineEdit *>(child))
            edit = e;
    CHECK(edit != nullptr && !edit->hasFrame() && edit->text() == QString("5"));
    spin.setFocus();
    CHECK(spin.focusPolicy() == Qt::NoFocus);
    edit->setFocus();
    key(edit, Qt::Key_Up);
    CHECK(spin.value() == 6);
    key(edit, Qt::Key_Down);
    key(edit, Qt::Key_Down);
    CHECK(spin.value() == 4);
    key(edit, Qt::Key_PageUp);
    CHECK(spin.value() == 10);
    key(edit, Qt::Key_PageDown);
    CHECK(spin.value() == 0);
    wheel(&spin, QPoint(10, 5), 2);
    CHECK(spin.value() == 2);

    // Typing a number takes it at once; leaving the field tidies the text
    values.clear();
    edit->selectAll();
    type(edit, "7");
    CHECK(spin.value() == 7 && values == QList<int>({7}) && edit->text() == QString("7"));
    type(edit, "x"); // letters are refused
    CHECK(edit->text() == QString("7"));
    type(edit, "5"); // 75 is out of range: the value stays
    CHECK(spin.value() == 7 && edit->text() == QString("75"));
    key(edit, Qt::Key_Return);
    CHECK(spin.value() == 10 && edit->text() == QString("10") && finished >= 1);
    edit->selectAll();
    key(edit, Qt::Key_Backspace);
    CHECK(spin.value() == 10); // empty is no number
    key(edit, Qt::Key_Return);
    CHECK(edit->text() == QString("10"));
    spin.setRange(-10, 10);
    edit->selectAll();
    type(edit, "-3");
    CHECK(spin.value() == -3);

    // The buttons
    spin.setValue(5);
    QStyleOptionSpinBox option;
    option.initFrom(&spin);
    const QRect up = spin.style()->subElementRect(QStyle::SE_SpinBoxUp, &option, &spin);
    const QRect down = spin.style()->subElementRect(QStyle::SE_SpinBoxDown, &option, &spin);
    const QRect field = spin.style()->subElementRect(QStyle::SE_SpinBoxEditField, &option, &spin);
    CHECK(up.right() < spin.width() && up.left() > field.right() - 1 && up.bottom() < down.top() && up.width() == 16);
    CHECK(edit->geometry() == field);
    click(&spin, up.center());
    CHECK(spin.value() == 6);
    click(&spin, down.center());
    click(&spin, down.center());
    CHECK(spin.value() == 4);
    press(&spin, up.center());
    CHECK(spin.value() == 5);
    release(&spin, up.center());
    CHECK(edit->hasFocus());

    // The buttons are drawn: the up arrow has ink
    const QImage image = grab(&top);
    const QRect arrowArea(inWindow(&spin) + up.topLeft() + QPoint(2, 2), QSize(up.width() - 4, up.height() - 4));
    CHECK(inkCount(image, arrowArea, image.pixel(arrowArea.topLeft())) > 4);
    spin.setEnabled(false);
    click(&spin, up.center());
    CHECK(spin.value() == 5);
    spin.setEnabled(true);
}

static void testComboBox() {
    QWidget top;
    QComboBox combo(&top);
    auto *layout = new QVBoxLayout(&top);
    layout->addWidget(&combo);
    layout->addStretch();
    top.resize(240, 260);
    top.show();
    settle(&top);

    CHECK(combo.count() == 0 && combo.currentIndex() == -1 && combo.currentText().isEmpty() &&
          combo.maxVisibleItems() == 10);
    CHECK(combo.sizePolicy().verticalPolicy() == QSizePolicy::Fixed && combo.focusPolicy() == Qt::StrongFocus);

    QList<int> indices, activated, highlighted;
    QStringList texts;
    QObject::connect(&combo, &QComboBox::currentIndexChanged, [&](int i) { indices << i; });
    QObject::connect(&combo, &QComboBox::currentTextChanged, [&](const QString &t) { texts << t; });
    QObject::connect(&combo, &QComboBox::activated, [&](int i) { activated << i; });
    QObject::connect(&combo, &QComboBox::highlighted, [&](int i) { highlighted << i; });

    combo.addItem("Alpha");
    CHECK(combo.count() == 1 && combo.currentIndex() == 0 && combo.currentText() == QString("Alpha"));
    CHECK(indices == QList<int>({0}) && texts == QStringList({"Alpha"}));
    combo.addItems({"Beta", "Gamma", "Delta"});
    CHECK(combo.count() == 4 && combo.currentIndex() == 0 && indices.size() == 1);
    CHECK(combo.itemText(2) == QString("Gamma") && combo.itemText(9).isEmpty() && combo.itemText(-1).isEmpty());
    CHECK(combo.findText("Delta") == 3 && combo.findText("nothing") == -1);
    const int hint = combo.sizeHint().width();
    combo.addItem("A much longer item than the others");
    CHECK(combo.sizeHint().width() > hint);
    combo.removeItem(4);
    CHECK(combo.sizeHint().width() == hint && combo.count() == 4);

    combo.setCurrentIndex(2);
    CHECK(combo.currentIndex() == 2 && indices.last() == 2 && texts.last() == QString("Gamma") && activated.isEmpty());
    combo.setCurrentIndex(2);
    CHECK(indices.size() == 2);
    combo.setCurrentText("Beta");
    CHECK(combo.currentIndex() == 1);
    combo.setCurrentText("nothing");
    CHECK(combo.currentIndex() == 1);
    combo.insertItem(0, "Zero"); // the current item stays the same
    CHECK(combo.currentIndex() == 2 && combo.currentText() == QString("Beta") && combo.itemText(0) == QString("Zero"));
    combo.setItemText(2, "Beta!");
    CHECK(combo.currentText() == QString("Beta!") && texts.last() == QString("Beta!"));
    combo.removeItem(0);
    CHECK(combo.currentIndex() == 1 && combo.currentText() == QString("Beta!"));
    combo.removeItem(1); // the current one: the next item takes its place
    CHECK(combo.currentIndex() == 1 && combo.currentText() == QString("Gamma") && combo.count() == 3);
    combo.setCurrentIndex(99);
    CHECK(combo.currentIndex() == -1 && combo.currentText().isEmpty());
    combo.setCurrentIndex(0);
    indices.clear();
    activated.clear();

    // Keys and the wheel change the item without a popup
    combo.setFocus();
    key(&combo, Qt::Key_Down);
    CHECK(combo.currentIndex() == 1 && activated == QList<int>({1}) && !combo.isPopupVisible());
    key(&combo, Qt::Key_End);
    CHECK(combo.currentIndex() == 2);
    key(&combo, Qt::Key_Down); // at the end
    CHECK(combo.currentIndex() == 2);
    key(&combo, Qt::Key_Up);
    key(&combo, Qt::Key_Home);
    CHECK(combo.currentIndex() == 0);
    wheel(&combo, QPoint(20, 10), -1);
    CHECK(combo.currentIndex() == 1);
    wheel(&combo, QPoint(20, 10), 1);
    CHECK(combo.currentIndex() == 0);

    // A click opens the popup, which lists the items below the combo box
    CHECK(!combo.isPopupVisible());
    click(&combo, QPoint(20, 10));
    CHECK(combo.isPopupVisible());
    settle(&top);
    QWidget *popup = nullptr;
    for (QObject *child : top.children())
        if (auto *w = dynamic_cast<QWidget *>(child); w && w != &combo && w->parentWidget() == &top && !w->isHidden() &&
                                                      w->metaObject()->className() != QString("QComboBox"))
            popup = w;
    CHECK(popup != nullptr);
    if (popup) {
        CHECK(popup->width() == combo.width() && popup->y() == combo.y() + combo.height() && popup->x() == combo.x());
        CHECK(popup->height() > 3 * combo.fontMetrics().height());
        const int row = (popup->height() - 2) / 3;

        // The mouse over an item highlights it, and a click on it chooses it
        highlighted.clear();
        mouse(popup, QEvent::MouseMove, QPoint(10, 1 + row + row / 2), Qt::NoButton, Qt::NoButton);
        CHECK(highlighted == QList<int>({1}));
        const QImage open = grab(&top);
        CHECK(open.pixel(inWindow(popup, QPoint(10, 1 + row + 2))) == top.palette().color(QPalette::Highlight).rgb());
        indices.clear();
        activated.clear();
        click(popup, QPoint(10, 1 + 2 * row + row / 2));
        CHECK(!combo.isPopupVisible() && combo.currentIndex() == 2 && indices == QList<int>({2}) &&
              activated == QList<int>({2}));
        CHECK(combo.hasFocus());
    }

    // Keys in the popup
    key(&combo, Qt::Key_Space);
    CHECK(combo.isPopupVisible());
    key(&combo, Qt::Key_Up); // goes to the popup, which has the focus
    key(&combo, Qt::Key_Up);
    key(&combo, Qt::Key_Return);
    CHECK(!combo.isPopupVisible() && combo.currentIndex() == 0 && activated.last() == 0);
    combo.showPopup();
    CHECK(combo.isPopupVisible());
    key(&combo, Qt::Key_Escape);
    CHECK(!combo.isPopupVisible() && combo.currentIndex() == 0 && combo.hasFocus());

    // A press outside closes it and is not passed on
    QPushButton other("Other", &top);
    other.setGeometry(10, 150, 100, 30);
    other.show();
    int clicks = 0;
    QObject::connect(&other, &QPushButton::clicked, [&] { ++clicks; });
    combo.showPopup();
    CHECK(combo.isPopupVisible());
    click(&other, QPoint(5, 5));
    CHECK(!combo.isPopupVisible() && clicks == 0);
    click(&other, QPoint(5, 5));
    CHECK(clicks == 1);

    // Clicking the combo box while its popup is open closes it
    combo.showPopup();
    click(&combo, QPoint(20, 10));
    CHECK(!combo.isPopupVisible());

    // A list longer than maxVisibleItems scrolls
    for (int i = 0; i < 20; ++i)
        combo.addItem(QString("Item %1").arg(i));
    combo.setMaxVisibleItems(5);
    combo.setCurrentIndex(0);
    combo.showPopup();
    popup = nullptr;
    for (QObject *child : top.children())
        if (auto *w = dynamic_cast<QWidget *>(child); w && w->parentWidget() == &top && w != &combo && w != &other &&
                                                      !w->isHidden() &&
                                                      w->metaObject()->className() != QString("QComboBox"))
            popup = w;
    CHECK(popup != nullptr);
    if (popup) {
        const int row = (popup->height() - 2) / 5;
        CHECK(popup->height() == 5 * row + 2);
        wheel(popup, QPoint(10, 10), -2);
        activated.clear();
        click(popup, QPoint(10, 1 + row / 2)); // the first visible item after scrolling by 6
        CHECK(activated.size() == 1 && activated[0] == 6);
        CHECK(combo.currentIndex() == 6);
    }
    combo.showPopup();
    key(&combo, Qt::Key_End);
    key(&combo, Qt::Key_Return);
    CHECK(combo.currentIndex() == combo.count() - 1);

    // With no room below, the popup opens above
    QWidget small;
    QComboBox low(&small);
    low.addItems({"One", "Two", "Three"});
    small.resize(200, 150);
    low.setGeometry(10, 110, 120, 26);
    small.show();
    settle(&small);
    low.showPopup();
    QWidget *above = nullptr;
    for (QObject *child : small.children())
        if (auto *w = dynamic_cast<QWidget *>(child); w && w != &low && !w->isHidden())
            above = w;
    CHECK(above != nullptr);
    if (above)
        CHECK(above->y() + above->height() <= low.y() && above->y() >= 0);
    low.hidePopup();
    CHECK(!low.isPopupVisible());

    // clear() and a combo box that is disabled
    combo.clear();
    CHECK(combo.count() == 0 && combo.currentIndex() == -1 && indices.last() == -1);
    combo.showPopup(); // nothing to show
    CHECK(!combo.isPopupVisible());
    combo.addItem("x");
    combo.setEnabled(false);
    combo.showPopup();
    CHECK(!combo.isPopupVisible());
}

static void testGroupBox() {
    QWidget top;
    auto *outer = new QVBoxLayout(&top);
    QGroupBox box("Options", &top);
    auto *inner = new QVBoxLayout(&box);
    QCheckBox first("First", &box);
    QRadioButton second("Second", &box);
    inner->addWidget(&first);
    inner->addWidget(&second);
    outer->addWidget(&box);
    top.resize(240, 140);
    top.show();
    settle(&top);

    CHECK(box.title() == QString("Options") && !box.isCheckable() && box.isChecked() &&
          box.focusPolicy() == Qt::NoFocus);
    CHECK(box.sizeHint().width() >= box.fontMetrics().horizontalAdvance("Options") && box.sizeHint().height() > 40);
    // The layout inside leaves room for the title
    CHECK(inner->contentsMargins().top() >= box.fontMetrics().height() && first.y() >= box.fontMetrics().height());
    CHECK(inner->contentsMargins().left() == 12 && inner->contentsMargins().bottom() == 12);

    QList<int> toggled, clicked;
    QObject::connect(&box, &QGroupBox::toggled, [&](bool on) { toggled << (on ? 1 : 0); });
    QObject::connect(&box, &QGroupBox::clicked, [&](bool on) { clicked << (on ? 1 : 0); });
    box.setChecked(false); // not checkable yet: the children stay enabled
    CHECK(first.isEnabled() && toggled == QList<int>({0}));
    box.setChecked(true);
    toggled.clear();

    box.setCheckable(true);
    CHECK(box.isCheckable() && box.focusPolicy() == Qt::StrongFocus && first.isEnabled());
    box.setChecked(false);
    CHECK(!box.isChecked() && !first.isEnabled() && !second.isEnabled() && box.isEnabled() &&
          toggled == QList<int>({0}));
    box.setChecked(false);
    CHECK(toggled.size() == 1);
    // Clicking the title toggles it
    QStyleOptionGroupBox option;
    option.initFrom(&box);
    option.text = box.title();
    option.checkable = true;
    const QRect title = box.style()->subElementRect(QStyle::SE_GroupBoxTitle, &option, &box);
    const QRect indicator = box.style()->subElementRect(QStyle::SE_GroupBoxIndicator, &option, &box);
    CHECK(title.top() == 0 && title.height() == box.fontMetrics().height() && title.contains(indicator));
    CHECK(box.style()->subElementRect(QStyle::SE_GroupBoxContents, &option, &box).top() > title.height());
    click(&box, indicator.center());
    CHECK(box.isChecked() && first.isEnabled() && clicked == QList<int>({1}) && toggled.last() == 1);
    press(&box, title.center());
    release(&box, QPoint(box.width() - 3, box.height() - 3)); // released outside the title: nothing
    CHECK(box.isChecked());
    click(&box, QPoint(box.width() - 3, box.height() - 3));
    CHECK(box.isChecked());
    box.setFocus();
    key(&box, Qt::Key_Space, " ");
    CHECK(!box.isChecked() && !second.isEnabled() && clicked.last() == 0);
    box.setChecked(true);
    box.setCheckable(false);
    CHECK(first.isEnabled() && box.focusPolicy() == Qt::NoFocus);

    // The title is drawn on the frame: the pixels of the title area are not the frame's
    box.setTitle("A longer title");
    CHECK(box.title() == QString("A longer title"));
    const QImage image = grab(&top);
    const QRgb window = top.palette().color(QPalette::Window).rgb();
    const QRect titleArea(inWindow(&box) + QPoint(10, 1), QSize(80, box.fontMetrics().height() - 2));
    CHECK(!haveRealFonts() || inkCount(image, titleArea, window) > 30);
    // ... and the frame is below the middle of the title
    CHECK(image.pixel(inWindow(&box) + QPoint(0, box.fontMetrics().height() + 10)) != window);

    QGroupBox empty(&top);
    CHECK(empty.title().isEmpty() && empty.sizeHint().height() > 0 && empty.minimumSizeHint().width() > 0);
}

static void testTabWidget() {
    QWidget top;
    auto *outer = new QVBoxLayout(&top);
    QTabWidget tabs(&top);
    outer->addWidget(&tabs);

    QWidget *pageA = new QWidget;
    auto *la = new QVBoxLayout(pageA);
    auto *buttonA = new QPushButton("Button A", pageA);
    la->addWidget(buttonA);
    la->addStretch();
    QWidget *pageB = new QWidget;
    auto *lb = new QVBoxLayout(pageB);
    auto *editB = new QLineEdit(pageB);
    lb->addWidget(editB);
    lb->addStretch();
    QWidget *pageC = new QWidget;

    CHECK(tabs.count() == 0 && tabs.currentIndex() == -1 && tabs.currentWidget() == nullptr);
    CHECK(tabs.sizePolicy().horizontalPolicy() == QSizePolicy::Expanding &&
          tabs.sizePolicy().verticalPolicy() == QSizePolicy::Expanding);
    QList<int> changed;
    QObject::connect(&tabs, &QTabWidget::currentChanged, [&](int i) { changed << i; });

    CHECK(tabs.addTab(pageA, "First") == 0);
    CHECK(tabs.currentIndex() == 0 && changed == QList<int>({0}) && pageA->parentWidget() == &tabs);
    CHECK(tabs.addTab(pageB, "Second") == 1 && tabs.currentIndex() == 0 && changed.size() == 1);
    CHECK(tabs.insertTab(2, pageC, "Third") == 2 && tabs.count() == 3);
    CHECK(tabs.addTab(nullptr, "none") == -1);
    CHECK(tabs.tabText(0) == QString("First") && tabs.tabText(2) == QString("Third") && tabs.tabText(7).isEmpty());
    CHECK(tabs.widget(1) == pageB && tabs.widget(5) == nullptr && tabs.indexOf(pageC) == 2 && tabs.indexOf(&top) == -1);
    CHECK(tabs.isTabEnabled(0) && !tabs.isTabEnabled(9));
    CHECK(tabs.currentWidget() == pageA);

    top.resize(360, 240);
    top.show();
    settle(&top);
    // Only the current page is shown, and it fills the area below the tabs
    CHECK(pageA->isVisible() && !pageB->isVisible() && !pageC->isVisible());
    const int bar = pageA->y();
    CHECK(bar >= tabs.fontMetrics().height() + 8 && pageA->x() == 1 && pageA->width() == tabs.width() - 2 &&
          pageA->height() == tabs.height() - bar - 1);
    CHECK(pageB->geometry() == pageA->geometry() && pageC->geometry() == pageA->geometry());
    // The page has its layout done
    CHECK(buttonA->isVisible() && buttonA->width() == pageA->width() - 18 && buttonA->x() == 9);
    CHECK(tabs.sizeHint().height() > bar && tabs.sizeHint().width() > 100);
    CHECK(tabs.minimumSizeHint().width() <= tabs.sizeHint().width());

    // The tabs: widths from the text, the current one is joined with the page
    QStyleOptionTab tabOption;
    tabOption.initFrom(&tabs);
    tabOption.text = "First";
    const QSize firstSize = tabs.style()->sizeFromContents(QStyle::CT_TabBarTab, &tabOption,
                                                           tabs.fontMetrics().size(Qt::TextSingleLine, "First"), &tabs);
    CHECK(firstSize.width() > tabs.fontMetrics().horizontalAdvance("First") && firstSize.height() == bar);

    // Clicking a tab switches pages
    changed.clear();
    click(&tabs, QPoint(firstSize.width() + 10, bar / 2));
    CHECK(tabs.currentIndex() == 1 && changed == QList<int>({1}) && pageB->isVisible() && !pageA->isVisible());
    click(&tabs, QPoint(firstSize.width() + 10, bar / 2)); // the same tab again: no change
    CHECK(changed.size() == 1);
    click(&tabs, QPoint(firstSize.width() / 2, bar / 2));
    CHECK(tabs.currentIndex() == 0 && pageA->isVisible() && !pageB->isVisible());
    click(&tabs, QPoint(tabs.width() - 5, bar / 2)); // beside the tabs: nothing
    CHECK(tabs.currentIndex() == 0);

    // Keys on the tab bar
    QWidget *barWidget = nullptr;
    for (QObject *child : tabs.children())
        if (auto *w = dynamic_cast<QWidget *>(child); w && w != pageA && w != pageB && w != pageC)
            barWidget = w;
    CHECK(barWidget != nullptr && barWidget->focusPolicy() == Qt::StrongFocus);
    if (barWidget) {
        barWidget->setFocus();
        key(barWidget, Qt::Key_Right);
        CHECK(tabs.currentIndex() == 1);
        tabs.setTabEnabled(2, false);
        CHECK(!tabs.isTabEnabled(2));
        key(barWidget, Qt::Key_Right); // skips the disabled tab: stays
        CHECK(tabs.currentIndex() == 1);
        click(&tabs, QPoint(tabs.width() / 2 - 10, bar / 2));
        tabs.setTabEnabled(2, true);
        key(barWidget, Qt::Key_Right);
        CHECK(tabs.currentIndex() == 2 && pageC->isVisible());
        key(barWidget, Qt::Key_Right);
        CHECK(tabs.currentIndex() == 2);
        key(barWidget, Qt::Key_Left);
        key(barWidget, Qt::Key_Left);
        CHECK(tabs.currentIndex() == 0);
        key(barWidget, Qt::Key_Left);
        CHECK(tabs.currentIndex() == 0);
        key(barWidget, Qt::Key_Up); // not a tab key
    }

    // Focus in a page that is hidden moves to the tabs
    tabs.setCurrentIndex(1);
    editB->setFocus();
    CHECK(editB->hasFocus());
    tabs.setCurrentIndex(0);
    CHECK(!editB->hasFocus() && barWidget && barWidget->hasFocus());
    tabs.setCurrentIndex(7); // not a tab: ignored
    tabs.setCurrentIndex(-1);
    CHECK(tabs.currentIndex() == 0);
    tabs.setCurrentWidget(pageC);
    CHECK(tabs.currentIndex() == 2);
    tabs.setCurrentWidget(&top);
    CHECK(tabs.currentIndex() == 2);
    tabs.setTabText(2, "A much longer label");
    CHECK(tabs.tabText(2) == QString("A much longer label"));
    tabs.setTabText(2, "Third");

    // The page with the current tab is painted below the tab bar; the pages' frame has a border line
    tabs.setCurrentIndex(0);
    const QImage image = grab(&top);
    const QPoint origin = inWindow(&tabs);
    const QRgb window = top.palette().color(QPalette::Window).rgb();
    CHECK(image.pixel(origin + QPoint(0, bar + 20)) != window);                // left border
    CHECK(image.pixel(origin + QPoint(tabs.width() - 1, bar + 20)) != window); // right border
    CHECK(image.pixel(origin + QPoint(tabs.width() - 20, bar - 1)) != window); // top border of the pane
    CHECK(image.pixel(origin + QPoint(firstSize.width() / 2, bar - 1)) ==
          image.pixel(origin + QPoint(40, bar + 5))); // open below the current tab

    // Removing tabs: the page stays, and a neighbour becomes current
    changed.clear();
    tabs.setCurrentIndex(1);
    changed.clear();
    tabs.removeTab(1);
    CHECK(tabs.count() == 2 && tabs.currentIndex() == 1 && tabs.currentWidget() == pageC && changed == QList<int>({1}));
    CHECK(!pageB->isVisible() && pageB->parentWidget() == &tabs);
    tabs.removeTab(0); // before the current one: the index follows
    CHECK(tabs.currentIndex() == 0 && tabs.currentWidget() == pageC && changed.size() == 1);
    tabs.removeTab(5);
    delete pageA;
    delete pageB;
    // Deleting a page takes its tab away
    QWidget *extra = new QWidget;
    tabs.addTab(extra, "Extra");
    CHECK(tabs.count() == 2);
    changed.clear();
    delete extra;
    CHECK(tabs.count() == 1 && tabs.currentWidget() == pageC);
    tabs.clear();
    CHECK(tabs.count() == 0 && tabs.currentIndex() == -1 && changed.last() == -1);
    delete pageC;
}

namespace {

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

void hover(QWidget *w, QPoint local) { mouse(w, QEvent::MouseMove, local, Qt::NoButton, Qt::NoButton); }

// The widget of a tool tip: the visible child of the window that is not one of `known`.
QWidget *tipOf(QWidget *top, std::initializer_list<QWidget *> known) {
    QWidget *tip = nullptr;
    for (QObject *child : top->children()) {
        auto *w = dynamic_cast<QWidget *>(child);
        if (w && !w->isHidden() && std::find(known.begin(), known.end(), w) == known.end())
            tip = w;
    }
    return tip;
}

// A widget with a tool tip that depends on the position of the mouse.
class Areas : public QWidget {
public:
    QPoint lastHelp;

protected:
    bool event(QEvent *e) override {
        if (e->type() == QEvent::ToolTip) {
            auto *help = static_cast<QHelpEvent *>(e);
            lastHelp = help->pos();
            QToolTip::showText(help->globalPos(), help->x() < width() / 2 ? "Left half" : "Right half", this);
            return true;
        }
        return QWidget::event(e);
    }
};

} // namespace

static void testToolTips() {
    QtPrivate::toolTipDelay() = 15;
    QWidget top;
    top.resize(300, 200);
    auto *layout = new QVBoxLayout(&top);
    QPushButton button("Save", &top);
    button.setToolTip("Saves the document");
    QWidget group(&top);
    group.setToolTip("The group\nhas two lines");
    auto *groupLayout = new QVBoxLayout(&group);
    QLabel inherits("Inherits the tool tip of the group", &group);
    QPushButton none("No tool tip of its own", &group);
    groupLayout->addWidget(&inherits);
    groupLayout->addWidget(&none);
    QPushButton off("Disabled", &top);
    off.setToolTip("Tool tips are shown for disabled widgets");
    off.setEnabled(false);
    QPushButton plain("Plain", &top);
    layout->addWidget(&button);
    layout->addWidget(&group);
    layout->addWidget(&off);
    layout->addWidget(&plain);
    top.show();
    settle(&top);

    CHECK(button.toolTip() == QString("Saves the document") && plain.toolTip().isEmpty() &&
          button.property("toolTip").toString() == QString("Saves the document"));
    CHECK(!QToolTip::isVisible() && QToolTip::text().isEmpty());

    // The mouse rests on the button: the tool tip appears after the delay, next to the mouse
    hover(&button, QPoint(20, 10));
    CHECK(!QToolTip::isVisible()); // not yet
    CHECK(waitFor([] { return QToolTip::isVisible(); }));
    CHECK(QToolTip::text() == QString("Saves the document"));
    QWidget *tip = tipOf(&top, {&button, &group, &off, &plain});
    CHECK(tip != nullptr);
    if (tip) {
        const QPoint mouse = button.mapTo(&top, QPoint(20, 10));
        CHECK(tip->x() == mouse.x() + 2 && tip->y() == mouse.y() + 16);
        CHECK(tip->width() > top.fontMetrics().horizontalAdvance("Saves the document") &&
              tip->height() >= top.fontMetrics().height() + 4);
        CHECK(QRect(QPoint(0, 0), top.size()).contains(tip->geometry()));
        const QImage image = grab(&top);
        const QRgb base = top.palette().color(QPalette::ToolTipBase).rgb();
        CHECK(image.pixel(tip->geometry().center() + QPoint(tip->width() / 2 - 4, 0)) == base);
    }
    // Moving within the same widget keeps it; moving to another widget takes it away
    hover(&button, QPoint(40, 12));
    CHECK(QToolTip::isVisible());
    hover(&plain, QPoint(10, 5));
    CHECK(!QToolTip::isVisible());
    CHECK(!waitFor([] { return QToolTip::isVisible(); }, 150)); // the plain button has no tool tip, nor has a parent

    // A widget with no tool tip of its own gets the one of its parent, also several lines
    hover(&inherits, QPoint(10, 5));
    CHECK(waitFor([] { return QToolTip::isVisible(); }));
    CHECK(QToolTip::text() == QString("The group\nhas two lines"));
    tip = tipOf(&top, {&button, &group, &off, &plain});
    CHECK(tip != nullptr && tip->height() >= 2 * top.fontMetrics().height());
    hover(&none, QPoint(10, 5)); // another widget of the group: asked again, the same answer
    CHECK(waitFor([] { return QToolTip::isVisible(); }));
    CHECK(QToolTip::text() == QString("The group\nhas two lines"));

    // A disabled widget shows its tool tip
    hover(&off, QPoint(10, 5));
    CHECK(waitFor([] { return QToolTip::text() == QString("Tool tips are shown for disabled widgets"); }));

    // A mouse button, a key, the wheel and leaving the window take it away
    hover(&button, QPoint(20, 10));
    CHECK(waitFor([] { return QToolTip::text() == QString("Saves the document"); }));
    click(&button, QPoint(20, 10));
    CHECK(!QToolTip::isVisible());
    hover(&button, QPoint(21, 10));
    CHECK(waitFor([] { return QToolTip::isVisible(); }));
    key(&button, Qt::Key_Shift);
    CHECK(!QToolTip::isVisible());
    hover(&button, QPoint(22, 10));
    CHECK(waitFor([] { return QToolTip::isVisible(); }));
    wheel(&button, QPoint(22, 10), 1);
    CHECK(!QToolTip::isVisible());
    hover(&button, QPoint(23, 10));
    CHECK(waitFor([] { return QToolTip::isVisible(); }));
    QEvent leave(QEvent::Leave);
    QCoreApplication::sendEvent(top.windowHandle(), &leave);
    CHECK(!QToolTip::isVisible());

    // The tool tip does not appear while the mouse keeps moving, and not after the mouse button is pressed
    for (int i = 0; i < 5; ++i) {
        hover(&button, QPoint(20 + i, 10));
        CHECK(!waitFor([] { return QToolTip::isVisible(); }, 4));
    }
    CHECK(waitFor([] { return QToolTip::isVisible(); }));
    QToolTip::hideText();
    CHECK(!QToolTip::isVisible());
    press(&button, QPoint(20, 10));
    drag(&button, QPoint(25, 10)); // a button is down: the mouse is dragged, no tool tip
    CHECK(!waitFor([] { return QToolTip::isVisible(); }, 100));
    release(&button, QPoint(25, 10));

    // QToolTip::showText() from the program, and hideText()
    QToolTip::showText(top.mapToGlobal(QPoint(10, 10)), "From the program", &top);
    CHECK(QToolTip::isVisible() && QToolTip::text() == QString("From the program"));
    tip = tipOf(&top, {&button, &group, &off, &plain});
    CHECK(tip && tip->x() == 12 && tip->y() == 26);
    QToolTip::showText(top.mapToGlobal(QPoint(10, 10)), "Replaced", &top);
    CHECK(QToolTip::text() == QString("Replaced"));
    QToolTip::showText(QPoint(), QString());
    CHECK(!QToolTip::isVisible());
    // Near the right edge and the bottom of the window the tool tip is moved to stay inside
    QToolTip::showText(top.mapToGlobal(QPoint(295, 195)), "At the corner of the window", &top);
    tip = tipOf(&top, {&button, &group, &off, &plain});
    CHECK(tip && QRect(QPoint(0, 0), top.size()).contains(tip->geometry()));
    QToolTip::hideText();
    // A rectangle: the tool tip goes when the mouse leaves it
    QToolTip::showText(button.mapToGlobal(QPoint(10, 10)), "In a rectangle", &button, QRect(0, 0, 30, 20));
    hover(&button, QPoint(10, 10));
    hover(&button, QPoint(12, 12)); // starts a timer, shows nothing new
    CHECK(QToolTip::isVisible());
    hover(&button, QPoint(40, 12));
    CHECK(!QToolTip::isVisible());
    QToolTip::hideText();

    // A widget that handles QEvent::ToolTip chooses the text by the position
    QWidget top2;
    top2.resize(200, 100);
    Areas areas;
    areas.setParent(&top2);
    areas.setGeometry(0, 0, 200, 100);
    top2.show();
    settle(&top2);
    hover(&areas, QPoint(20, 50));
    CHECK(waitFor([] { return QToolTip::isVisible(); }));
    CHECK(QToolTip::text() == QString("Left half") && areas.lastHelp == QPoint(20, 50));
    hover(&areas, QPoint(150, 50)); // the same widget: the tool tip stays, the text of the first position
    CHECK(QToolTip::text() == QString("Left half"));
    QToolTip::hideText();
    hover(&areas, QPoint(151, 50));
    CHECK(waitFor([] { return QToolTip::text() == QString("Right half"); }));
    QToolTip::hideText();

    // The tool tip is not found by the mouse
    QToolTip::showText(top2.mapToGlobal(QPoint(10, 10)), "Over the widget", &top2);
    CHECK(top2.childAt(QPoint(20, 30)) == &areas);

    // Window hidden: nothing remains. Setting an empty tool tip removes it
    top2.hide();
    CHECK(!QToolTip::isVisible());
    button.setToolTip(QString());
    CHECK(button.toolTip().isEmpty());
    QtPrivate::toolTipDelay() = 700;
}

static void testHideTakesFocus() {
    QWidget top;
    QWidget group(&top);
    QLineEdit edit(&group);
    QLineEdit other(&top);
    top.show();
    edit.setFocus();
    CHECK(edit.hasFocus());
    group.hide();
    CHECK(!edit.hasFocus() && top.focusWidget() == nullptr);
    group.show();
    other.setFocus();
    edit.hide();
    CHECK(other.hasFocus()); // another widget keeps the focus
}

static void testEverythingPaints() {
    for (Qt::ColorScheme scheme : {Qt::ColorScheme::Light, Qt::ColorScheme::Dark}) {
        QGuiApplication::styleHints()->setColorScheme(scheme);
        QWidget top;
        auto *layout = new QVBoxLayout(&top);
        auto *tabs = new QTabWidget(&top);
        auto *page = new QWidget;
        auto *pl = new QVBoxLayout(page);
        auto *box = new QGroupBox("Group", page);
        auto *bl = new QVBoxLayout(box);
        bl->addWidget(new QRadioButton("Radio", box));
        bl->addWidget(new QCheckBox("Check", box));
        bl->addWidget(new QLineEdit("Line edit", box));
        auto *spin = new QSpinBox(box);
        spin->setSuffix(" units");
        bl->addWidget(spin);
        auto *combo = new QComboBox(box);
        combo->addItems({"One", "Two"});
        bl->addWidget(combo);
        auto *slider = new QSlider(Qt::Horizontal, box);
        slider->setValue(30);
        bl->addWidget(slider);
        auto *bar = new QProgressBar(box);
        bar->setValue(60);
        bl->addWidget(bar);
        pl->addWidget(box);
        tabs->addTab(page, "Everything");
        tabs->addTab(new QWidget, "Empty");
        layout->addWidget(tabs);
        top.show();
        settle(&top);
        const QImage image = grab(&top);
        const QRgb window = top.palette().color(QPalette::Window).rgb();
        CHECK(!image.isNull() && inkCount(image, image.rect(), window) > 2000);
        // Every control is inside the window, and is not squeezed
        for (QWidget *w : std::initializer_list<QWidget *>{box, spin, combo, slider, bar})
            CHECK(w->isVisible() && w->height() >= 10 &&
                  w->mapTo(&top, QPoint(w->width() - 1, w->height() - 1)).y() < top.height());
    }
    QGuiApplication::styleHints()->unsetColorScheme();
}

// A widget with a size hint that it does not get from its contents.
class Box : public QWidget {
public:
    explicit Box(QSize hint, QWidget *parent = nullptr) : QWidget(parent), m_hint(hint) {}
    QSize sizeHint() const override { return m_hint; }

private:
    QSize m_hint;
};

static void testToolButton() {
    int clicked = 0, toggles = 0;
    QWidget top;
    top.resize(200, 80);
    QToolButton button(&top);
    button.setGeometry(10, 10, 60, 30);
    QObject::connect(&button, &QToolButton::clicked, [&](bool) { ++clicked; });
    QObject::connect(&button, &QToolButton::toggled, [&](bool) { ++toggles; });
    CHECK(!button.autoRaise() && button.arrowType() == Qt::NoArrow && button.focusPolicy() == Qt::TabFocus &&
          button.text().isEmpty());
    CHECK(button.sizePolicy().horizontalPolicy() == QSizePolicy::Fixed &&
          button.sizePolicy().verticalPolicy() == QSizePolicy::Fixed);
    button.setText("Open");
    const QFontMetrics fm = button.fontMetrics();
    CHECK(button.sizeHint() == QSize(fm.horizontalAdvance("Open") + 14, fm.height() + 7));
    button.setArrowType(Qt::DownArrow);
    CHECK(button.arrowType() == Qt::DownArrow && button.sizeHint() == QSize(24, 23));
    button.setArrowType(Qt::NoArrow);
    top.show();
    settle(&top);

    click(&button, QPoint(30, 15));
    CHECK(clicked == 1 && !button.isChecked());
    button.setCheckable(true);
    click(&button, QPoint(30, 15));
    CHECK(clicked == 2 && toggles == 1 && button.isChecked());
    button.setChecked(false);

    // Painting: a raised button has the color of a button; with automatic raise it is flat until the mouse is over it
    const QPalette pal = button.palette();
    const QPoint origin = inWindow(&button);
    hover(&top, QPoint(150, 70)); // the mouse is away from the button
    QImage image = grab(&top);
    const QRgb window = pal.color(QPalette::Window).rgb();
    // (the window and the button have the same color in the light palette: the frame tells them apart)
    CHECK(image.pixel(origin + QPoint(0, 15)) != window);
    button.setAutoRaise(true);
    button.clearFocus();
    CHECK(button.autoRaise());
    image = grab(&top);
    CHECK(image.pixel(origin + QPoint(0, 15)) == window);
    hover(&button, QPoint(30, 15));
    image = grab(&top);
    CHECK(image.pixel(origin + QPoint(0, 15)) != window);
    hover(&top, QPoint(150, 70));
    image = grab(&top);
    CHECK(image.pixel(origin + QPoint(0, 15)) == window);
    // Pressed
    button.setAutoRaise(false);
    const QRgb raised = image.pixel(origin + QPoint(4, 4));
    press(&button, QPoint(30, 15));
    image = grab(&top);
    CHECK(image.pixel(origin + QPoint(4, 4)) != raised);
    release(&button, QPoint(30, 15));
    // The arrows draw something in the middle, and each of them different
    button.setText(QString());
    std::vector<QImage> arrows;
    for (Qt::ArrowType arrow : {Qt::UpArrow, Qt::DownArrow, Qt::LeftArrow, Qt::RightArrow}) {
        button.setArrowType(arrow);
        button.setFixedSize(24, 23);
        arrows.push_back(grab(&top).copy());
    }
    const QRect middle(origin + QPoint(6, 6), QSize(12, 12));
    auto ink = [&](const QImage &img) {
        int n = 0;
        for (int y = middle.top(); y <= middle.bottom(); ++y)
            for (int x = middle.left(); x <= middle.right(); ++x)
                if (img.pixel(x, y) == pal.color(QPalette::ButtonText).rgb())
                    ++n;
        return n;
    };
    CHECK(ink(arrows[0]) > 5 && ink(arrows[0]) == ink(arrows[1]) && ink(arrows[2]) == ink(arrows[3]) &&
          ink(arrows[2]) > 5);
    CHECK(arrows[0].pixel(origin + QPoint(8, 10)) != arrows[1].pixel(origin + QPoint(8, 10)) &&
          arrows[2].pixel(origin + QPoint(9, 9)) != arrows[3].pixel(origin + QPoint(9, 9)));
    button.setEnabled(false);
    image = grab(&top);
    button.setEnabled(true);
    const int disabledClicks = clicked;
    button.setEnabled(false);
    click(&button, QPoint(10, 10));
    CHECK(clicked == disabledClicks);
}

static void testDial() {
    std::vector<int> values;
    int pressed = 0, released = 0, moved = 0;
    QWidget top;
    top.resize(200, 120);
    QDial dial(&top);
    dial.setGeometry(0, 0, 100, 100);
    QObject::connect(&dial, &QDial::valueChanged, [&](int v) { values.push_back(v); });
    QObject::connect(&dial, &QDial::sliderPressed, [&] { ++pressed; });
    QObject::connect(&dial, &QDial::sliderReleased, [&] { ++released; });
    QObject::connect(&dial, &QDial::sliderMoved, [&](int) { ++moved; });
    CHECK(dial.minimum() == 0 && dial.maximum() == 99 && dial.value() == 0 && dial.singleStep() == 1 &&
          dial.pageStep() == 10 && !dial.wrapping() && !dial.notchesVisible() && dial.hasTracking() &&
          !dial.isSliderDown());
    CHECK(dial.sizeHint() == QSize(100, 100) && dial.minimumSizeHint() == QSize(50, 50) &&
          dial.focusPolicy() == Qt::WheelFocus);
    top.show();
    settle(&top);

    dial.setValue(40);
    CHECK(dial.value() == 40 && dial.sliderPosition() == 40 && values == std::vector<int>{40});
    dial.setValue(500);
    CHECK(dial.value() == 99);
    dial.setRange(0, 50);
    CHECK(dial.value() == 50 && dial.maximum() == 50);
    dial.setMaximum(20);
    CHECK(dial.value() == 20);
    dial.setMinimum(30);
    CHECK(dial.minimum() == 30 && dial.maximum() == 30 && dial.value() == 30);
    dial.setRange(10, 0);
    CHECK(dial.minimum() == 10 && dial.maximum() == 10);
    dial.setSingleStep(0);
    dial.setPageStep(0);
    CHECK(dial.singleStep() == 1 && dial.pageStep() == 1);
    dial.setSingleStep(1);
    dial.setPageStep(10);
    dial.setRange(0, 99);
    dial.setValue(0);
    values.clear();

    // The mouse: a point on the circle gives the value of its angle
    const double pi = 3.14159265358979323846;
    auto at = [&](double degrees, double radius = 30) {
        return QPoint(49 + static_cast<int>(std::lround(radius * std::cos(degrees * pi / 180))),
                      49 - static_cast<int>(std::lround(radius * std::sin(degrees * pi / 180))));
    };
    press(&dial, at(225));
    CHECK(dial.isSliderDown() && pressed == 1 && dial.value() == 0 && moved == 1);
    drag(&dial, at(90));
    CHECK(dial.value() == 50 && dial.sliderPosition() == 50);
    drag(&dial, at(-45));
    CHECK(dial.value() == 99);
    drag(&dial, at(-80)); // between the ends: the nearer
    CHECK(dial.value() == 99);
    drag(&dial, at(-100));
    CHECK(dial.value() == 0);
    drag(&dial, at(180));
    CHECK(dial.value() > 10 && dial.value() < 25);
    release(&dial, at(180));
    CHECK(!dial.isSliderDown() && released == 1);
    const int after = dial.value();
    drag(&dial, at(0));
    CHECK(dial.value() == after);
    press(&dial, at(90));
    CHECK(dial.value() == 50);
    release(&dial, at(90));
    mouse(&dial, QEvent::MouseButtonPress, at(90), Qt::RightButton, Qt::RightButton);
    mouse(&dial, QEvent::MouseButtonRelease, at(90), Qt::RightButton, Qt::NoButton);
    // Without tracking, the value changes when the dial is released
    dial.setTracking(false);
    CHECK(!dial.hasTracking());
    dial.setValue(0);
    values.clear();
    press(&dial, at(90));
    drag(&dial, at(45));
    CHECK(dial.value() == 0 && dial.sliderPosition() > 50 && values.empty());
    release(&dial, at(45));
    CHECK(dial.value() > 50 && values.size() == 1);
    dial.setTracking(true);

    // Keys and wheel
    dial.setValue(10);
    dial.setFocus();
    key(&dial, Qt::Key_Right);
    CHECK(dial.value() == 11);
    key(&dial, Qt::Key_Up);
    key(&dial, Qt::Key_Down);
    key(&dial, Qt::Key_Left);
    CHECK(dial.value() == 10);
    key(&dial, Qt::Key_PageUp);
    CHECK(dial.value() == 20);
    key(&dial, Qt::Key_PageDown);
    CHECK(dial.value() == 10);
    key(&dial, Qt::Key_End);
    CHECK(dial.value() == 99);
    key(&dial, Qt::Key_Right); // the end stays
    CHECK(dial.value() == 99);
    key(&dial, Qt::Key_Home);
    CHECK(dial.value() == 0);
    key(&dial, Qt::Key_A, "a");
    wheel(&dial, QPoint(50, 50), 1);
    CHECK(dial.value() == 3);
    wheel(&dial, QPoint(50, 50), -2);
    CHECK(dial.value() == 0);

    // Wrapping: the full circle, the smallest value at the bottom, and the keys go round
    dial.setWrapping(true);
    CHECK(dial.wrapping());
    press(&dial, at(-90));
    CHECK(dial.value() == 0);
    drag(&dial, at(180)); // clockwise: the left side is a quarter
    CHECK(dial.value() == 25);
    drag(&dial, at(90));
    CHECK(dial.value() == 50);
    drag(&dial, at(0));
    CHECK(dial.value() == 74 || dial.value() == 75);
    release(&dial, at(180));
    dial.setValue(99);
    key(&dial, Qt::Key_Right);
    CHECK(dial.value() == 0);
    key(&dial, Qt::Key_Left);
    CHECK(dial.value() == 99);
    wheel(&dial, QPoint(50, 50), 1);
    CHECK(dial.value() == 2);
    key(&dial, Qt::Key_PageDown);
    CHECK(dial.value() == 92);
    dial.setWrapping(false);

    // Painting: the handle is a dot of the highlight color where the angle puts it
    dial.setValue(0);
    const QPalette pal = dial.palette();
    QImage image = grab(&top);
    CHECK(image.pixel(25, 73) == pal.color(QPalette::Highlight).rgb());
    dial.setValue(99);
    image = grab(&top);
    CHECK(image.pixel(73, 73) == pal.color(QPalette::Highlight).rgb() &&
          image.pixel(25, 73) != pal.color(QPalette::Highlight).rgb());
    dial.setNotchesVisible(true);
    CHECK(dial.notchesVisible());
    const QImage notched = grab(&top);
    CHECK(notched.pixel(49, 49 - 47) != image.pixel(49, 49 - 47) || notched.pixel(49, 3) != image.pixel(49, 3));
    dial.setNotchesVisible(false);
    dial.setEnabled(false);
    image = grab(&top);
    CHECK(image.pixel(73, 73) != pal.color(QPalette::Highlight).rgb());
    dial.setEnabled(true);
}

static void testLcdNumber() {
    int overflows = 0;
    QWidget top;
    top.resize(200, 60);
    QLCDNumber lcd(&top);
    lcd.setGeometry(10, 10, 100, 30);
    QObject::connect(&lcd, &QLCDNumber::overflow, [&] { ++overflows; });
    CHECK(lcd.digitCount() == 5 && lcd.mode() == QLCDNumber::Dec && lcd.segmentStyle() == QLCDNumber::Filled &&
          !lcd.smallDecimalPoint() && lcd.value() == 0 && lcd.intValue() == 0 && lcd.frameShape() == QFrame::Box &&
          lcd.frameShadow() == QFrame::Raised);
    CHECK(lcd.sizeHint() == QSize(10 + 9 * 6, 23) && lcd.sizePolicy().horizontalPolicy() == QSizePolicy::Minimum);
    lcd.setSmallDecimalPoint(true);
    CHECK(lcd.smallDecimalPoint() && lcd.sizeHint() == QSize(10 + 9 * 5, 23));
    lcd.setSmallDecimalPoint(false);
    QLCDNumber eight(8);
    CHECK(eight.digitCount() == 8 && eight.sizeHint().width() == 10 + 9 * 9);
    lcd.setDigitCount(7);
    CHECK(lcd.digitCount() == 7 && lcd.sizeHint().width() == 10 + 9 * 8);
    lcd.setDigitCount(500);
    CHECK(lcd.digitCount() == 99);
    lcd.setDigitCount(-4);
    CHECK(lcd.digitCount() == 0);
    lcd.setDigitCount(5);
    top.show();
    settle(&top);

    lcd.display(42);
    CHECK(lcd.value() == 42 && lcd.intValue() == 42 && overflows == 0);
    lcd.display(3.5);
    CHECK(lcd.value() == 3.5 && lcd.intValue() == 4);
    lcd.display(-3.5);
    CHECK(lcd.intValue() == -4);
    lcd.display(QString("12.5"));
    CHECK(lcd.value() == 12.5);
    lcd.display(QString("--"));
    CHECK(lcd.value() == 0);
    CHECK(!lcd.checkOverflow(12345) && lcd.checkOverflow(123456) && lcd.checkOverflow(-12345) &&
          !lcd.checkOverflow(1234.5) && lcd.checkOverflow(12345678.0));
    lcd.display(123456);
    CHECK(overflows == 1);
    lcd.display(12345);
    CHECK(overflows == 1);
    lcd.display(QString("abcdefg"));
    CHECK(overflows == 2);
    lcd.display(1234567.0);
    CHECK(overflows == 3); // a number that does not fit as a whole number overflows
    // Modes
    lcd.setHexMode();
    CHECK(lcd.mode() == QLCDNumber::Hex);
    lcd.display(255);
    CHECK(lcd.intValue() == 255 && !lcd.checkOverflow(255) && lcd.checkOverflow(0xFFFFFF));
    lcd.setBinMode();
    CHECK(lcd.mode() == QLCDNumber::Bin && lcd.checkOverflow(255) && !lcd.checkOverflow(31));
    lcd.setOctMode();
    CHECK(lcd.mode() == QLCDNumber::Oct && !lcd.checkOverflow(255) && lcd.checkOverflow(0100000));
    lcd.setDecMode();
    lcd.setMode(QLCDNumber::Dec);
    lcd.display(8);
    overflows = 0;
    lcd.setMode(QLCDNumber::Hex); // the number is shown again
    CHECK(lcd.value() == 8);
    lcd.setMode(QLCDNumber::Dec);

    // Painting: the digits are lit segments at the right, blank digits at the left
    auto inkIn = [&](const QImage &img, const QRect &area) {
        const QRgb color = lcd.palette().color(QPalette::WindowText).rgb();
        int n = 0;
        for (int y = area.top(); y <= area.bottom(); ++y)
            for (int x = area.left(); x <= area.right(); ++x)
                if (img.pixel(x, y) == color)
                    ++n;
        return n;
    };
    const QPoint origin = inWindow(&lcd);
    const int cell = (lcd.contentsRect().width() - 4) / 5;
    const QRect first(origin + QPoint(lcd.contentsRect().left() + 2, 4), QSize(cell, lcd.height() - 8));
    const QRect last(origin + QPoint(lcd.contentsRect().left() + 2 + 4 * cell, 4), QSize(cell, lcd.height() - 8));
    lcd.display(8);
    QImage image = grab(&top);
    const int eightInk = inkIn(image, last);
    CHECK(eightInk > 20 && inkIn(image, first) == 0);
    lcd.display(1);
    image = grab(&top);
    const int oneInk = inkIn(image, last);
    CHECK(oneInk > 0 && oneInk < eightInk);
    lcd.display(QString(" "));
    image = grab(&top);
    CHECK(inkIn(image, last) == 0);
    lcd.display(QString("-"));
    image = grab(&top);
    CHECK(inkIn(image, last) > 0 && inkIn(image, last) < oneInk + 10);
    lcd.display(88888);
    lcd.setSegmentStyle(QLCDNumber::Flat);
    CHECK(lcd.segmentStyle() == QLCDNumber::Flat);
    image = grab(&top);
    const int flat = inkIn(image, last);
    lcd.setSegmentStyle(QLCDNumber::Filled);
    image = grab(&top);
    const int filled = inkIn(image, last);
    lcd.setSegmentStyle(QLCDNumber::Outline);
    image = grab(&top);
    const int outline = inkIn(image, last);
    CHECK(flat > filled && outline > 10); // the segments are so thin that the outline of one is all of it
    lcd.setSegmentStyle(QLCDNumber::Filled);
    // The decimal point: its own digit, or small
    lcd.display(QString("1.2"));
    image = grab(&top);
    const int ownPoint = inkIn(
        image, QRect(origin + QPoint(lcd.contentsRect().left() + 2 + 2 * cell, 4), QSize(2 * cell, lcd.height() - 8)));
    lcd.setSmallDecimalPoint(true);
    lcd.display(QString("1.2"));
    image = grab(&top);
    CHECK(ownPoint > 0 && inkIn(image, last) > 0);
    lcd.display(QString(".5"));
    lcd.setDigitCount(0);
    lcd.display(5);
    image = grab(&top);
    lcd.setDigitCount(5);
    lcd.display(2);
    lcd.setEnabled(false);
    image = grab(&top);
    lcd.setEnabled(true);
}

static void testSplitter() {
    std::vector<std::pair<int, int>> moves;
    QWidget top;
    top.resize(300, 100);
    auto *layout = new QVBoxLayout(&top);
    layout->setContentsMargins(0, 0, 0, 0);
    QSplitter split(&top);
    layout->addWidget(&split);
    auto *a = new Box(QSize(0, 0)), *b = new Box(QSize(0, 0)), *c = new Box(QSize(0, 0));
    a->setMinimumWidth(30);
    QObject::connect(&split, &QSplitter::splitterMoved, [&](int pos, int index) { moves.emplace_back(pos, index); });
    CHECK(split.orientation() == Qt::Horizontal && split.count() == 0 && split.childrenCollapsible() &&
          split.sizes().isEmpty() && split.sizePolicy().horizontalPolicy() == QSizePolicy::Expanding);
    CHECK(split.handleWidth() == 4);
    split.addWidget(a);
    split.addWidget(b);
    split.addWidget(c);
    CHECK(split.count() == 3 && split.widget(1) == b && split.widget(3) == nullptr && split.widget(-1) == nullptr &&
          split.indexOf(c) == 2 && split.indexOf(&top) == -1 && a->parentWidget() == &split);
    top.show();
    settle(&top);
    CHECK(split.width() == 300 && split.height() == 100);
    // Equal sizes for widgets with no hints, with a handle between
    CHECK(split.sizes() == (QList<int>{97, 97, 98}));
    CHECK(a->geometry() == QRect(0, 0, 97, 100) && b->geometry() == QRect(101, 0, 97, 100) &&
          c->geometry() == QRect(202, 0, 98, 100));
    // setSizes: the sizes are scaled to the room
    split.setSizes({100, 100, 92});
    CHECK(split.sizes() == (QList<int>{100, 100, 92}) && b->x() == 104 && c->x() == 208);
    split.setSizes({1, 2, 1});
    CHECK(split.sizes() == (QList<int>{73, 146, 73}));
    split.setSizes({1});
    CHECK(split.sizes() == (QList<int>{292, 0, 0}));
    split.setSizes({100, 100, 92});

    // A handle is dragged
    const QPoint handle0(a->width() + 2, 50);
    press(&split, handle0);
    drag(&split, QPoint(handle0.x() + 30, 50));
    CHECK(split.sizes() == (QList<int>{130, 70, 92}) && moves.back() == std::make_pair(130, 1));
    CHECK(a->width() == 130 && b->x() == 134 && b->width() == 70);
    drag(&split, QPoint(handle0.x() + 30 + 40, 50));
    CHECK(split.sizes()[0] == 170);
    release(&split, QPoint(handle0.x() + 70, 50));
    const std::size_t seen = moves.size();
    drag(&split, QPoint(10, 50)); // without the button nothing moves
    CHECK(moves.size() == seen);
    // The minimum of a widget: not below it, but to nothing if dragged below half of it
    split.setSizes({100, 100, 92});
    press(&split, QPoint(102, 50));
    drag(&split, QPoint(24, 50));
    CHECK(split.sizes()[0] == 30 && split.sizes()[1] == 170);
    drag(&split, QPoint(10, 50));
    CHECK(split.sizes()[0] == 0 && split.sizes()[1] == 200 && b->x() == 4);
    drag(&split, QPoint(120, 50));
    CHECK(split.sizes()[0] > 100);
    split.setCollapsible(0, false);
    CHECK(!split.isCollapsible(0) && split.isCollapsible(1));
    drag(&split, QPoint(5, 50));
    CHECK(split.sizes()[0] == 30);
    release(&split, QPoint(5, 50));
    split.setChildrenCollapsible(false);
    CHECK(!split.childrenCollapsible() && !split.isCollapsible(1) && !split.isCollapsible(7));
    split.setCollapsible(9, true);
    split.setChildrenCollapsible(true);
    // The other end: the last widget to nothing
    split.setSizes({100, 100, 92});
    press(&split, QPoint(a->width() + split.handleWidth() + b->width() + 2, 50));
    drag(&split, QPoint(299, 50));
    CHECK(split.sizes()[2] == 0 && split.sizes()[1] == 192);
    release(&split, QPoint(299, 50));
    split.setSizes({100, 100, 92});

    // When the splitter is resized: in proportion, or by the stretch factors
    top.resize(600, 100);
    settle(&top);
    CHECK(split.width() == 600 && split.sizes()[0] + split.sizes()[1] + split.sizes()[2] == 592 &&
          split.sizes()[0] > 190 && split.sizes()[2] > 170);
    split.setSizes({100, 100, 92});
    top.resize(300, 100);
    settle(&top);
    split.setSizes({100, 100, 92});
    split.setStretchFactor(2, 1);
    split.setStretchFactor(9, 1);
    top.resize(500, 100);
    settle(&top);
    CHECK(split.sizes() == (QList<int>{100, 100, 292}));
    split.setStretchFactor(1, 1);
    top.resize(300, 100);
    settle(&top);
    CHECK(split.sizes()[0] == 100 && split.sizes()[1] + split.sizes()[2] == 192);
    split.setStretchFactor(1, 0);
    split.setStretchFactor(2, 0);
    // Too small: the minimum sizes hold
    top.resize(50, 100);
    settle(&top);
    CHECK(split.sizes()[0] >= 30);
    top.resize(300, 100);
    settle(&top);

    // Handle width
    split.setHandleWidth(10);
    CHECK(split.handleWidth() == 10);
    split.setSizes({100, 100, 80});
    CHECK(b->x() == 110 && c->x() == 220);
    split.setHandleWidth(4);
    split.setSizes({100, 100, 92});

    // A vertical splitter: the widgets are one above the other
    split.setOrientation(Qt::Vertical);
    CHECK(split.orientation() == Qt::Vertical);
    settle(&top);
    CHECK(a->width() == 300 && a->x() == 0 && a->y() == 0 && b->y() > a->height() && c->y() > b->y());
    CHECK(split.sizes()[0] + split.sizes()[1] + split.sizes()[2] == 100 - 8);
    split.setSizes({20, 30, 42});
    const int boundary = a->height();
    press(&split, QPoint(150, boundary + 2));
    drag(&split, QPoint(150, boundary + 12));
    CHECK(split.sizes()[0] == 30 && split.sizes()[1] == 20);
    release(&split, QPoint(150, boundary + 12));
    split.setOrientation(Qt::Horizontal);
    split.setOrientation(Qt::Horizontal);

    // Painting: the handle has a grip in the middle
    split.setSizes({100, 100, 92});
    QImage image = grab(&top);
    const QPoint grip = inWindow(&split, QPoint(102, 50));
    const QRgb window = split.palette().color(QPalette::Window).rgb();
    CHECK(image.pixel(inWindow(&split, QPoint(100, 48))) != window &&
          image.pixel(inWindow(&split, QPoint(101, 49))) != window);
    CHECK(image.pixel(inWindow(&split, QPoint(100, 5))) == window);
    (void)grip;

    // Moving and removing widgets
    split.insertWidget(0, c);
    CHECK(split.indexOf(c) == 0 && split.indexOf(a) == 1 && split.count() == 3);
    split.insertWidget(3, c);
    CHECK(split.indexOf(c) == 2);
    split.insertWidget(2, c);
    CHECK(split.indexOf(c) == 2);
    split.addWidget(nullptr);
    delete b;
    CHECK(split.count() == 2 && split.indexOf(c) == 1);
    settle(&top);
    CHECK(split.sizes()[0] + split.sizes()[1] == 296 && a->width() + c->width() == 296);
    // Sizes of the widgets that have hints
    QSplitter hinted;
    hinted.resize(300, 100);
    auto *h1 = new Box(QSize(100, 20)), *h2 = new Box(QSize(50, 40));
    hinted.addWidget(h1);
    hinted.addWidget(h2);
    CHECK(hinted.sizeHint() == QSize(100 + 50 + 4, 40) && hinted.minimumSizeHint() == QSize(4, 0));
    hinted.show();
    settle(&hinted);
    CHECK(hinted.sizes()[0] > hinted.sizes()[1] && hinted.sizes()[0] + hinted.sizes()[1] == 296);
    hinted.setOrientation(Qt::Vertical);
    CHECK(hinted.sizeHint() == QSize(100, 20 + 40 + 4));
    hinted.setHandleWidth(0);
    CHECK(hinted.sizeHint() == QSize(100, 60));
}

static void testStatusBar() {
    std::vector<QString> messages;
    QWidget top;
    top.resize(300, 60);
    auto *layout = new QVBoxLayout(&top);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addStretch();
    QStatusBar bar(&top);
    layout->addWidget(&bar);
    QObject::connect(&bar, &QStatusBar::messageChanged, [&](const QString &m) { messages.push_back(m); });
    top.show();
    settle(&top);
    const int lh = bar.fontMetrics().height();
    CHECK(bar.currentMessage().isEmpty() && !bar.isSizeGripEnabled() &&
          bar.sizePolicy().verticalPolicy() == QSizePolicy::Fixed);
    CHECK(bar.sizeHint().height() == lh + 5 && bar.height() == lh + 5 && bar.width() == 300 &&
          bar.minimumSizeHint().height() == lh + 5);
    bar.setSizeGripEnabled(true);
    CHECK(bar.isSizeGripEnabled());
    bar.setSizeGripEnabled(false);

    // The widgets on the left, and on the right
    QLabel left("Left", &top), second("Second", &top), perm("Right", &top), perm2("Far", &top);
    bar.addWidget(&left);
    bar.addWidget(&second, 1);
    bar.addPermanentWidget(&perm);
    bar.addPermanentWidget(&perm2);
    settle(&top);
    CHECK(left.parentWidget() == &bar && !left.isHidden() && !perm.isHidden());
    CHECK(left.x() == 6 && left.y() == 3 && left.height() == bar.height() - 5);
    // The permanent widgets from the right edge: the last added is the farthest right
    CHECK(perm2.x() + perm2.width() == 300 - 6 && perm.x() + perm.width() == perm2.x() - 4);
    CHECK(second.x() == left.x() + left.width() + 4 &&
          second.x() + second.width() == perm.x() - 4); // the stretch takes the rest
    CHECK(bar.sizeHint().width() >=
          left.sizeHint().width() + second.sizeHint().width() + perm.sizeHint().width() + perm2.sizeHint().width());

    // A message hides the widgets on the left, until it is cleared
    bar.showMessage("Ready");
    CHECK(bar.currentMessage() == "Ready" && messages == std::vector<QString>{"Ready"} && left.isHidden() &&
          second.isHidden() && !perm.isHidden());
    QImage image = grab(&top);
    const QPoint origin = inWindow(&bar);
    const QRgb window = bar.palette().color(QPalette::Window).rgb();
    int ink = 0;
    for (int x = 4; x < 60; ++x)
        for (int y = 3; y < bar.height() - 2; ++y)
            if (image.pixel(origin + QPoint(x, y)) != window)
                ++ink;
    CHECK(ink > 20);
    bar.showMessage("Ready"); // the same message is announced again
    CHECK(messages.size() == 2);
    bar.clearMessage();
    CHECK(bar.currentMessage().isEmpty() && !left.isHidden() && !second.isHidden() && messages.back().isEmpty() &&
          messages.size() == 3);
    bar.clearMessage();
    CHECK(messages.size() == 3);
    // A widget that was hidden stays hidden
    second.hide();
    bar.showMessage("Again");
    bar.clearMessage();
    CHECK(second.isHidden() && !left.isHidden());
    second.show();
    // An empty message clears
    bar.showMessage("x");
    bar.showMessage(QString());
    CHECK(bar.currentMessage().isEmpty() && !left.isHidden());

    // A message that goes away by itself
    messages.clear();
    bar.showMessage("Soon gone", 40);
    CHECK(bar.currentMessage() == "Soon gone" && left.isHidden());
    CHECK(waitFor([&] { return bar.currentMessage().isEmpty(); }));
    CHECK(!left.isHidden() && messages == (std::vector<QString>{"Soon gone", QString()}));
    // A new message replaces the old one and its timer
    bar.showMessage("First", 30);
    bar.showMessage("Second");
    CHECK(!waitFor([&] { return bar.currentMessage().isEmpty(); }, 120) && bar.currentMessage() == "Second");
    bar.clearMessage();

    // Widgets that are added during a message are hidden until it ends
    bar.showMessage("Busy");
    QLabel late("Late", &top);
    bar.addWidget(&late);
    CHECK(late.isHidden());
    bar.clearMessage();
    CHECK(!late.isHidden());
    bar.removeWidget(&late);
    CHECK(late.isHidden() && late.parentWidget() == &bar);
    bar.removeWidget(&late);
    bar.removeWidget(nullptr);
    bar.addWidget(&late);
    CHECK(!late.isHidden());
    bar.insertWidget(0, &late);
    CHECK(late.x() == 6);
    bar.insertPermanentWidget(0, &late);
    CHECK(late.x() > 200 || late.x() + late.width() <= 300);
    // A deleted widget leaves the bar
    auto *doomed = new QLabel("Doomed", &top);
    bar.addPermanentWidget(doomed);
    const int hintWidth = bar.sizeHint().width();
    delete doomed;
    CHECK(bar.sizeHint().width() < hintWidth);

    // The look: a line at the top
    image = grab(&top);
    CHECK(image.pixel(origin + QPoint(150, 0)) != window && image.pixel(origin + QPoint(150, 1)) == window);
    bar.setEnabled(false);
    bar.showMessage("Disabled");
    image = grab(&top);
    bar.setEnabled(true);
    bar.clearMessage();
}

int main(int argc, char **argv) {
    ::setenv("QT_QPA_PLATFORM", "offscreen", 1);
    ::setenv("QT_SCALE_FACTOR", "1", 1);
    QApplication app(argc, argv);
    // The tests look for the colors of the light palette, whatever the desktop uses.
    app.styleHints()->setColorScheme(Qt::ColorScheme::Light);
    testRadioButtons();
    testLineEdit();
    testSlider();
    testProgressBar();
    testSpinBox();
    testComboBox();
    testGroupBox();
    testTabWidget();
    testToolTips();
    testToolButton();
    testDial();
    testLcdNumber();
    testSplitter();
    testStatusBar();
    testHideTakesFocus();
    testEverythingPaints();
    if (g_failures == 0)
        std::puts("all control tests passed");
    return g_failures == 0 ? 0 : 1;
}
