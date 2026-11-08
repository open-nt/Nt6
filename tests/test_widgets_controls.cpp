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
#include <QtWidgets/qgroupbox.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qprogressbar.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qradiobutton.h>
#include <QtWidgets/qslider.h>
#include <QtWidgets/qspinbox.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qtabwidget.h>

#include "platforms/offscreen/qoffscreenintegration_p.h"
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
    CHECK(inkCount(withHint, text, edit.palette().color(QPalette::Base).rgb()) > 20);
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
    CHECK(slider.sizeHint().height() == 20 && slider.sizeHint().width() >= 80 && slider.minimumSizeHint().width() < 80);
    CHECK(slider.width() == 200 - 22 && slider.height() == 20);

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
    CHECK(image.pixel(origin + QPoint(slider.width() / 2, 10)) != highlight);
    slider.setValue(100);
    image = grab(&top);
    CHECK(image.pixel(origin + QPoint(slider.width() / 2, 10)) == highlight);

    // A vertical slider has its smallest value at the bottom
    QSlider vertical(Qt::Vertical, &top);
    vertical.setGeometry(150, 0, 20, 120);
    vertical.show();
    vertical.setRange(0, 10);
    CHECK(vertical.orientation() == Qt::Vertical && vertical.sizePolicy().verticalPolicy() == QSizePolicy::Expanding);
    CHECK(vertical.sizeHint().width() == 20 && vertical.sizeHint().height() >= 80);
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
    CHECK(withText > 5 && inkCount(image, textArea, image.pixel(textArea.topLeft())) == 0);
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
    CHECK(inkCount(image, titleArea, window) > 30);
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
    testHideTakesFocus();
    testEverythingPaints();
    if (g_failures == 0)
        std::puts("all control tests passed");
    return g_failures == 0 ? 0 : 1;
}
