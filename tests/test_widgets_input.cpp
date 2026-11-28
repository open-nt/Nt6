// MIT License - see LICENSE
//
// Tests for the base classes of the input widgets of Nt Widgets on the offscreen platform: QAbstractSlider,
// QAbstractSpinBox with QSpinBox and QDoubleSpinBox, and QButtonGroup.
#include <QtCore>
#include <QtGui/qevent.h>
#include <QtGui/qstylehints.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qabstractslider.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qbuttongroup.h>
#include <QtWidgets/qcalendarwidget.h>
#include <QtWidgets/qcheckbox.h>
#include <QtWidgets/qcommandlinkbutton.h>
#include <QtWidgets/qcompleter.h>
#include <QtWidgets/qdatetimeedit.h>
#include <QtWidgets/qfontcombobox.h>
#include <QtWidgets/qkeysequenceedit.h>
#include <QtWidgets/qlistwidget.h>
#include <QtWidgets/qtabbar.h>
#include <QtWidgets/qtabwidget.h>
#include <QtWidgets/qtoolbox.h>
#include <QtWidgets/qdial.h>
#include <QtWidgets/qcolormap.h>
#include <QtWidgets/qfiledialog.h>
#include <QtWidgets/qgesture.h>
#include <QtWidgets/qgraphicsanchorlayout.h>
#include <QtGui/qstandarditemmodel.h>
#include <QtWidgets/qgraphicsscene.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qstyleditemdelegate.h>
#include <QtWidgets/qplaintextdocumentlayout.h>
#include <QtWidgets/qgraphicswidget.h>
#include <QtWidgets/qmdiarea.h>
#include <QtWidgets/qtreewidget.h>
#include <QtWidgets/qwizard.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qdialog.h>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qradiobutton.h>
#include <QtWidgets/qscrollarea.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qslider.h>
#include <QtWidgets/qspinbox.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>
#include <QtWidgets/qlabel.h>

#include "platforms/offscreen/qoffscreenintegration_p.h"
#include "qpa/qwindowsysteminterface_p.h"
#include "qwindow_p.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

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

void key(QWidget *w, int k, const QString &text = QString(), Qt::KeyboardModifiers mods = Qt::NoModifier) {
    QWindow *win = w->window()->windowHandle();
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyPress, k, mods, text);
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyRelease, k, mods);
}

void drag(QWidget *w, QPoint local) { mouse(w, QEvent::MouseMove, local, Qt::NoButton, Qt::LeftButton); }

void type(QWidget *w, const QString &text) {
    for (QChar c : text)
        key(w, c.unicode() >= 'a' && c.unicode() <= 'z' ? c.unicode() - 32 : c.unicode(), QString(c));
}

void wheel(QWidget *w, QPoint local, int notches) {
    const QPoint inWindow = w->mapTo(w->window(), local);
    QWindowSystemInterface::handleWheelEvent(w->window()->windowHandle(), QPointF(inWindow), QPointF(inWindow),
                                             QPoint(), QPoint(0, 120 * notches), Qt::NoModifier);
}

QLineEdit *editOf(QWidget *spin) {
    for (QObject *child : spin->children())
        if (auto *e = dynamic_cast<QLineEdit *>(child))
            return e;
    return nullptr;
}

// A slider that records what sliderChange() is told.
class RecordingSlider : public QAbstractSlider {
public:
    using QAbstractSlider::QAbstractSlider;
    static constexpr int Range = SliderRangeChange;
    static constexpr int Orientation = SliderOrientationChange;
    static constexpr int Steps = SliderStepsChange;
    static constexpr int Value = SliderValueChange;
    QList<int> changes;

protected:
    void sliderChange(SliderChange change) override {
        changes << static_cast<int>(change);
        QAbstractSlider::sliderChange(change);
    }
};

void testAbstractSlider() {
    QWidget top;
    RecordingSlider slider(&top);
    top.resize(200, 60);
    top.show();
    settle(&top);

    CHECK(slider.minimum() == 0 && slider.maximum() == 99 && slider.value() == 0 && slider.sliderPosition() == 0);
    CHECK(slider.orientation() == Qt::Horizontal && slider.hasTracking() && !slider.invertedControls());
    CHECK(slider.focusPolicy() == Qt::WheelFocus);

    QStringList log;
    QObject::connect(&slider, &QAbstractSlider::rangeChanged,
                     [&](int min, int max) { log << QString("range %1 %2").arg(min).arg(max); });
    QObject::connect(&slider, &QAbstractSlider::valueChanged, [&](int v) { log << QString("value %1").arg(v); });
    QObject::connect(&slider, &QAbstractSlider::sliderMoved, [&](int v) { log << QString("moved %1").arg(v); });
    QObject::connect(&slider, &QAbstractSlider::actionTriggered, [&](int a) {
        log << QString("action %1 at %2 value %3").arg(a).arg(slider.sliderPosition()).arg(slider.value());
    });
    QObject::connect(&slider, &QAbstractSlider::sliderPressed, [&] { log << "pressed"; });
    QObject::connect(&slider, &QAbstractSlider::sliderReleased, [&] { log << "released"; });

    // The range comes before the value it clamps, as in Qt
    slider.setValue(80);
    log.clear();
    slider.setRange(0, 50);
    CHECK(log == QStringList({"range 0 50", "value 50"}));
    CHECK(slider.changes.contains(RecordingSlider::Range) && slider.changes.last() == RecordingSlider::Value);

    // triggerAction() moves the position, reports the action, then sets the value
    slider.setRange(0, 100);
    slider.setValue(10);
    log.clear();
    slider.triggerAction(QAbstractSlider::SliderPageStepAdd);
    CHECK(slider.value() == 20 && log == QStringList({"action 3 at 20 value 10", "value 20"}));
    slider.triggerAction(QAbstractSlider::SliderToMaximum);
    CHECK(slider.value() == 100);
    slider.triggerAction(QAbstractSlider::SliderSingleStepAdd); // stays at the end
    CHECK(slider.value() == 100);
    slider.triggerAction(QAbstractSlider::SliderToMinimum);
    slider.triggerAction(QAbstractSlider::SliderSingleStepSub);
    CHECK(slider.value() == 0);

    // Dragging: sliderMoved while down; without tracking the value waits for the release
    slider.setTracking(false);
    log.clear();
    slider.setSliderDown(true);
    slider.setSliderPosition(30);
    CHECK(slider.isSliderDown() && slider.sliderPosition() == 30 && slider.value() == 0);
    CHECK(log == QStringList({"pressed", "moved 30"}));
    slider.setSliderDown(false);
    CHECK(slider.value() == 30 && log.mid(2) == QStringList({"released", "action 7 at 30 value 0", "value 30"}));
    slider.setTracking(true);
    log.clear();
    slider.setSliderPosition(40); // not down: no sliderMoved, but tracking sets the value
    CHECK(slider.value() == 40 && !log.contains("moved 40"));
    slider.setSliderPosition(500);
    CHECK(slider.sliderPosition() == 100 && slider.value() == 100);

    // Steps are at least one
    slider.changes.clear();
    slider.setSingleStep(0);
    slider.setPageStep(-4);
    CHECK(slider.singleStep() == 1 && slider.pageStep() == 1);
    CHECK(slider.changes.contains(RecordingSlider::Steps));
    slider.setPageStep(10);

    // Keys and the wheel, and inverted controls turn them round
    slider.setFocus();
    slider.setValue(50);
    key(&slider, Qt::Key_Right);
    CHECK(slider.value() == 51);
    slider.setInvertedControls(true);
    CHECK(slider.invertedControls());
    key(&slider, Qt::Key_Right);
    key(&slider, Qt::Key_Right);
    CHECK(slider.value() == 49);
    key(&slider, Qt::Key_PageUp);
    CHECK(slider.value() == 39);
    key(&slider, Qt::Key_Home);
    CHECK(slider.value() == 0); // the ends are not inverted
    slider.setValue(50);
    wheel(&slider, QPoint(10, 5), 1);
    CHECK(slider.value() == 47);
    slider.setInvertedControls(false);
    wheel(&slider, QPoint(10, 5), 1);
    CHECK(slider.value() == 50);

    // The orientation transposes the size policy
    slider.changes.clear();
    slider.setOrientation(Qt::Vertical);
    CHECK(slider.orientation() == Qt::Vertical && slider.sizePolicy().verticalPolicy() == QSizePolicy::Expanding &&
          slider.sizePolicy().horizontalPolicy() == QSizePolicy::Fixed);
    CHECK(slider.changes == QList<int>({RecordingSlider::Orientation}));

    // The concrete sliders share the base
    QSlider qslider(Qt::Vertical);
    QScrollBar bar(Qt::Horizontal);
    QDial dial;
    QAbstractSlider *all[] = {&qslider, &bar, &dial};
    for (QAbstractSlider *s : all) {
        s->setRange(0, 10);
        s->triggerAction(QAbstractSlider::SliderPageStepAdd);
        CHECK(s->value() == 10);
    }
    CHECK(qslider.orientation() == Qt::Vertical && bar.orientation() == Qt::Horizontal);
    CHECK(bar.focusPolicy() == Qt::NoFocus && qslider.focusPolicy() == Qt::StrongFocus);

    // A dial that wraps steps round the ends
    dial.setRange(0, 9);
    dial.setValue(9);
    dial.setWrapping(true);
    dial.triggerAction(QAbstractSlider::SliderSingleStepAdd);
    CHECK(dial.value() == 0);
    dial.triggerAction(QAbstractSlider::SliderSingleStepSub);
    CHECK(dial.value() == 9);
    dial.setValue(42); // setValue() stays in the range
    CHECK(dial.value() == 9);
}

void testSpinBoxBase() {
    QWidget top;
    auto *layout = new QVBoxLayout(&top);
    QSpinBox spin;
    layout->addWidget(&spin);
    top.resize(200, 80);
    top.show();
    settle(&top);
    QLineEdit *edit = editOf(&spin);
    CHECK(edit != nullptr);

    // The step buttons are enabled as far as the value can go
    spin.setRange(0, 10);
    spin.setValue(0);
    QAbstractSpinBox *base = &spin;
    CHECK(!base->wrapping() && !base->isReadOnly() && base->keyboardTracking());

    // The special value text stands for the smallest value
    QStringList texts;
    QObject::connect(&spin, &QSpinBox::textChanged, [&](const QString &t) { texts << t; });
    spin.setSpecialValueText("Off");
    CHECK(spin.specialValueText() == QString("Off") && spin.text() == QString("Off") &&
          spin.cleanText() == QString("0"));
    spin.setSuffix(" s");
    spin.stepUp();
    CHECK(spin.text() == QString("1 s") && texts == QStringList({"1 s"}));
    edit->setFocus();
    edit->selectAll();
    type(edit, "Off");
    key(edit, Qt::Key_Return);
    CHECK(spin.value() == 0 && spin.text() == QString("Off"));
    spin.setSpecialValueText(QString());
    CHECK(spin.text() == QString("0 s"));
    spin.setSuffix(QString());

    // Read-only: the text cannot be edited and nothing steps
    spin.setReadOnly(true);
    CHECK(spin.isReadOnly() && edit->isReadOnly());
    key(edit, Qt::Key_Up);
    wheel(&spin, QPoint(10, 5), 1);
    CHECK(spin.value() == 0);
    spin.setReadOnly(false);
    key(edit, Qt::Key_Up);
    CHECK(spin.value() == 1);

    // Without keyboard tracking the value waits for Return; interpretText() takes it too
    QList<int> values;
    QObject::connect(&spin, &QSpinBox::valueChanged, [&](int v) { values << v; });
    spin.setKeyboardTracking(false);
    edit->selectAll();
    type(edit, "7");
    CHECK(spin.value() == 1 && values.isEmpty());
    key(edit, Qt::Key_Return);
    CHECK(spin.value() == 7 && values == QList<int>({7}));
    edit->selectAll();
    type(edit, "4");
    spin.interpretText();
    CHECK(spin.value() == 4);
    spin.setKeyboardTracking(true);

    // clear() leaves the prefix and the suffix and keeps the value
    spin.setPrefix("[");
    spin.setSuffix("]");
    spin.clear();
    CHECK(spin.text() == QString("[]") && spin.value() == 4 && edit->cursorPosition() == 1);
    spin.setValue(5);
    spin.selectAll();
    CHECK(edit->selectedText() == QString("5"));
    spin.setPrefix(QString());
    spin.setSuffix(QString());

    // A text of a reimplemented textFromValue() is read back
    class HexSpinBox : public QSpinBox {
    protected:
        QString textFromValue(int value) const override { return QString::number(value, 16); }
        int valueFromText(const QString &text) const override { return text.toInt(nullptr, 16); }
    };
    HexSpinBox hex;
    hex.setRange(0, 255);
    hex.setValue(255);
    CHECK(hex.text() == QString("ff") && hex.cleanText() == QString("ff"));
}

void testDoubleSpinBox() {
    QWidget top;
    auto *layout = new QVBoxLayout(&top);
    QDoubleSpinBox spin;
    layout->addWidget(&spin);
    top.resize(200, 80);
    top.show();
    settle(&top);
    QLineEdit *edit = editOf(&spin);

    CHECK(spin.minimum() == 0.0 && spin.maximum() == 99.99 && spin.value() == 0.0 && spin.singleStep() == 1.0 &&
          spin.decimals() == 2);
    CHECK(spin.text() == QString("0.00") && spin.cleanText() == QString("0.00"));
    CHECK(spin.sizeHint().width() > spin.fontMetrics().horizontalAdvance("99.99"));

    QList<double> values;
    QObject::connect(&spin, &QDoubleSpinBox::valueChanged, [&](double v) { values << v; });
    spin.setValue(1.234);
    CHECK(spin.value() == 1.23 && spin.text() == QString("1.23") && values == QList<double>({1.23}));
    spin.setValue(1.231); // the same once rounded
    CHECK(values.size() == 1);
    spin.setValue(1000);
    CHECK(spin.value() == 99.99);
    spin.setRange(-1.0, 1.0);
    CHECK(spin.value() == 1.0 && spin.minimum() == -1.0);
    spin.setSingleStep(0.25);
    spin.setValue(0.0);
    spin.stepBy(3);
    CHECK(spin.value() == 0.75);
    spin.stepUp();
    spin.stepUp(); // stops at the end
    CHECK(spin.value() == 1.0);
    spin.setWrapping(true);
    spin.stepUp();
    CHECK(spin.value() == -1.0);
    spin.setWrapping(false);
    spin.setSingleStep(-3); // ignored
    CHECK(spin.singleStep() == 0.25);

    // Decimals round the range and the value
    spin.setDecimals(1);
    spin.setValue(0.46);
    CHECK(spin.value() == 0.5 && spin.text() == QString("0.5"));
    spin.setDecimals(0);
    CHECK(spin.text() == QString("1") || spin.text() == QString("0"));
    spin.setDecimals(2);

    // Prefix, suffix and typing with a point
    spin.setPrefix("x");
    spin.setSuffix(" V");
    spin.setValue(0.5);
    CHECK(spin.text() == QString("x0.50 V") && spin.prefix() == QString("x") && spin.suffix() == QString(" V"));
    edit->setFocus();
    spin.selectAll();
    type(edit, "0.7");
    CHECK(spin.value() == 0.7);
    type(edit, "q"); // letters are refused
    CHECK(spin.value() == 0.7);
    key(edit, Qt::Key_Return);
    CHECK(spin.text() == QString("x0.70 V"));
    key(edit, Qt::Key_Down);
    CHECK(spin.value() == 0.45);
}

void testButtonGroup() {
    QWidget top;
    auto *layout = new QVBoxLayout(&top);
    // Buttons with different parents, which auto-exclusivity alone would not tie together
    QWidget left, right;
    layout->addWidget(&left);
    layout->addWidget(&right);
    auto *a = new QPushButton("A", &left);
    auto *b = new QPushButton("B", &right);
    auto *c = new QPushButton("C", &right);
    c->move(0, 40);
    for (QPushButton *button : {a, b, c})
        button->setCheckable(true);
    top.resize(200, 200);
    top.show();
    settle(&top);

    QButtonGroup group;
    CHECK(group.exclusive() && group.buttons().isEmpty() && group.checkedButton() == nullptr &&
          group.checkedId() == -1);
    group.addButton(a, 1);
    group.addButton(b); // automatic ids are negative from -2
    group.addButton(c);
    CHECK(group.buttons() == QList<QAbstractButton *>({a, b, c}));
    CHECK(group.id(a) == 1 && group.id(b) == -2 && group.id(c) == -3 && group.button(-3) == c);
    CHECK(a->group() == &group && group.button(7) == nullptr);
    group.setId(c, 3);
    group.setId(c, -1); // refused
    CHECK(group.id(c) == 3);

    QStringList log;
    QObject::connect(&group, &QButtonGroup::idClicked, [&](int id) { log << QString("idClicked %1").arg(id); });
    QObject::connect(&group, &QButtonGroup::buttonClicked,
                     [&](QAbstractButton *button) { log << "buttonClicked " + button->text(); });
    QObject::connect(&group, &QButtonGroup::idPressed, [&](int id) { log << QString("idPressed %1").arg(id); });
    QObject::connect(&group, &QButtonGroup::buttonPressed,
                     [&](QAbstractButton *button) { log << "buttonPressed " + button->text(); });
    QObject::connect(&group, &QButtonGroup::idReleased, [&](int id) { log << QString("idReleased %1").arg(id); });
    QObject::connect(&group, &QButtonGroup::buttonReleased,
                     [&](QAbstractButton *button) { log << "buttonReleased " + button->text(); });
    QObject::connect(&group, &QButtonGroup::idToggled,
                     [&](int id, bool on) { log << QString("idToggled %1 %2").arg(id).arg(on); });
    QObject::connect(&group, &QButtonGroup::buttonToggled, [&](QAbstractButton *button, bool on) {
        log << QString("buttonToggled %1 %2").arg(button->text()).arg(on);
    });

    // An exclusive group checks one button at a time, across parents
    a->click();
    CHECK(a->isChecked() && group.checkedButton() == a && group.checkedId() == 1);
    CHECK(log == QStringList({"idPressed 1", "buttonPressed A", "idReleased 1", "buttonReleased A", "idToggled 1 1",
                              "buttonToggled A 1", "idClicked 1", "buttonClicked A"}));
    log.clear();
    click(b, QPoint(5, 5));
    CHECK(b->isChecked() && !a->isChecked() && group.checkedId() == -2);
    CHECK(log.contains("idToggled 1 0") && log.contains("idToggled -2 1") && log.last() == "buttonClicked B");
    b->click(); // the checked button stays checked
    CHECK(b->isChecked());
    b->setChecked(false);
    CHECK(b->isChecked());

    // Adding a checked button to an exclusive group unchecks the others
    auto *d = new QPushButton("D", &right);
    d->setCheckable(true);
    d->setChecked(true);
    group.addButton(d, 4);
    CHECK(d->isChecked() && !b->isChecked() && group.checkedId() == 4);

    // A group that is not exclusive lets buttons be checked and unchecked freely
    group.setExclusive(false);
    a->click();
    CHECK(a->isChecked() && d->isChecked());
    a->click();
    CHECK(!a->isChecked());
    group.setExclusive(true);

    // Removing and deleting buttons
    group.removeButton(d);
    CHECK(d->group() == nullptr && group.id(d) == -1 && group.buttons().size() == 3);
    group.addButton(d, 4);
    delete d;
    CHECK(group.buttons() == QList<QAbstractButton *>({a, b, c}));

    // A button joins one group at most
    QButtonGroup other;
    other.addButton(c);
    CHECK(c->group() == &other && !group.buttons().contains(c));

    // Radio buttons in a group: one choice, and the arrow keys move within the group
    QWidget box;
    auto *boxLayout = new QVBoxLayout(&box);
    auto *r1 = new QRadioButton("One");
    auto *r2 = new QRadioButton("Two");
    auto *r3 = new QRadioButton("Three"); // a sibling not in the group
    boxLayout->addWidget(r1);
    boxLayout->addWidget(r2);
    boxLayout->addWidget(r3);
    box.show();
    settle(&box);
    QButtonGroup radios;
    radios.addButton(r1, 1);
    radios.addButton(r2, 2);
    r1->setChecked(true);
    r3->setChecked(true); // auto-exclusive with its siblings outside the group only
    CHECK(r1->isChecked() && r3->isChecked());
    r1->setFocus();
    key(r1, Qt::Key_Down);
    CHECK(r2->isChecked() && !r1->isChecked() && r3->isChecked() && r2->hasFocus());
    key(r2, Qt::Key_Down);
    CHECK(r1->isChecked() && r1->hasFocus());

    // Deleting the group leaves the buttons
    {
        auto *temporary = new QButtonGroup;
        QCheckBox check;
        temporary->addButton(&check);
        delete temporary;
        CHECK(check.group() == nullptr);
    }
}

void testTabBar() {
    QWidget top;
    QTabBar bar(&top);
    top.resize(400, 60);
    QList<int> current;
    QObject::connect(&bar, &QTabBar::currentChanged, [&](int i) { current << i; });
    CHECK(bar.count() == 0 && bar.currentIndex() == -1);
    CHECK(bar.addTab("One") == 0 && bar.addTab("Two") == 1 && bar.insertTab(1, "Middle") == 1);
    CHECK(bar.count() == 3 && bar.currentIndex() == 0 && current == QList<int>({0}));
    CHECK(bar.tabText(1) == QString("Middle") && bar.tabText(9).isEmpty());
    bar.resize(bar.sizeHint());
    top.show();
    settle(&top);
    CHECK(bar.tabRect(1).left() == bar.tabRect(0).right() + 1 && bar.tabAt(bar.tabRect(2).center()) == 2);

    // Clicking, keys and the wheel skip disabled tabs
    QList<int> clicked;
    QObject::connect(&bar, &QTabBar::tabBarClicked, [&](int i) { clicked << i; });
    click(&bar, bar.tabRect(2).center());
    CHECK(bar.currentIndex() == 2 && clicked == QList<int>({2}));
    bar.setTabEnabled(1, false);
    CHECK(!bar.isTabEnabled(1));
    bar.setFocus();
    key(&bar, Qt::Key_Left);
    CHECK(bar.currentIndex() == 0);
    key(&bar, Qt::Key_End);
    CHECK(bar.currentIndex() == 2);
    wheel(&bar, QPoint(5, 5), 1);
    CHECK(bar.currentIndex() == 0);
    click(&bar, bar.tabRect(1).center());
    CHECK(bar.currentIndex() == 0);
    bar.setTabEnabled(1, true);

    // Data and tool tips
    bar.setTabData(1, 42);
    bar.setTabToolTip(1, "tip");
    CHECK(bar.tabData(1).toInt() == 42 && bar.tabToolTip(1) == QString("tip"));

    // Moving: by moveTab() and by dragging a movable bar
    std::vector<std::pair<int, int>> moves;
    QObject::connect(&bar, &QTabBar::tabMoved, [&](int a, int b) { moves.emplace_back(a, b); });
    bar.moveTab(0, 2);
    CHECK(bar.tabText(2) == QString("One") && bar.currentIndex() == 2 && moves.size() == 1);
    bar.setMovable(true);
    CHECK(bar.isMovable());
    const QPoint start = bar.tabRect(0).center();
    mouse(&bar, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    drag(&bar, start + QPoint(bar.tabRect(1).width(), 0));
    mouse(&bar, QEvent::MouseButtonRelease, start + QPoint(bar.tabRect(1).width(), 0), Qt::LeftButton, Qt::NoButton);
    CHECK(moves.size() == 2 && bar.tabText(1) == QString("Middle"));

    // Close buttons
    bar.setTabsClosable(true);
    bar.resize(bar.sizeHint());
    QList<int> closes;
    QObject::connect(&bar, &QTabBar::tabCloseRequested, [&](int i) { closes << i; });
    const QRect last = bar.tabRect(2);
    click(&bar, QPoint(last.right() - 10, last.center().y()));
    CHECK(closes == QList<int>({2}) && bar.count() == 3);

    // Removing the current tab selects as the behavior says
    bar.setCurrentIndex(1);
    current.clear();
    bar.removeTab(1);
    CHECK(bar.currentIndex() == 1 && current == QList<int>({1}));
    bar.setSelectionBehaviorOnRemove(QTabBar::SelectLeftTab);
    bar.removeTab(1);
    CHECK(bar.currentIndex() == 0 && bar.count() == 1);
    bar.removeTab(0);
    CHECK(bar.currentIndex() == -1 && current.last() == -1);

    // A tab widget uses a tab bar
    QTabWidget tabs;
    auto *a = new QLabel("a");
    auto *b = new QLabel("b");
    tabs.addTab(a, "A");
    tabs.addTab(b, "B");
    CHECK(tabs.tabBar() && tabs.tabBar()->count() == 2 && tabs.currentWidget() == a);
    tabs.setTabToolTip(1, "second");
    CHECK(tabs.tabToolTip(1) == QString("second"));
    tabs.setMovable(true);
    tabs.tabBar()->moveTab(0, 1);
    CHECK(tabs.widget(0) == b && tabs.widget(1) == a && tabs.currentWidget() == a && tabs.currentIndex() == 1);
    QList<int> tabCloses;
    QObject::connect(&tabs, &QTabWidget::tabCloseRequested, [&](int i) { tabCloses << i; });
    tabs.setTabsClosable(true);
    CHECK(tabs.tabsClosable() && tabs.isMovable());
    emit tabs.tabBar()->tabCloseRequested(0);
    CHECK(tabCloses == QList<int>({0}));
}

void testToolBox() {
    QToolBox box;
    box.resize(200, 300);
    auto *first = new QLabel("first");
    auto *second = new QLabel("second");
    QList<int> current;
    QObject::connect(&box, &QToolBox::currentChanged, [&](int i) { current << i; });
    CHECK(box.addItem(first, "First") == 0 && box.addItem(second, "Second") == 1);
    CHECK(box.count() == 2 && box.currentIndex() == 0 && box.currentWidget() == first && current == QList<int>({0}));
    CHECK(box.itemText(1) == QString("Second") && box.indexOf(second) == 1 && box.widget(1) == second);
    box.show();
    settle(&box);
    CHECK(!first->isHidden() && second->isHidden());
    QList<QAbstractButton *> bars = box.findChildren<QAbstractButton *>();
    CHECK(bars.size() == 2);
    bars.at(1)->click();
    settle(&box);
    CHECK(box.currentIndex() == 1 && first->isHidden() && !second->isHidden());
    CHECK(second->geometry().top() > bars.at(1)->geometry().bottom() && second->height() > 150);
    box.setItemText(0, "Renamed");
    box.setItemToolTip(0, "tip");
    CHECK(box.itemText(0) == QString("Renamed") && box.itemToolTip(0) == QString("tip"));
    box.setItemEnabled(1, false); // the current item moves to an enabled one
    CHECK(!box.isItemEnabled(1) && box.currentIndex() == 0);
    box.setCurrentWidget(second);
    CHECK(box.currentIndex() == 1);
    box.removeItem(1);
    CHECK(box.count() == 1 && box.currentIndex() == 0 && second->parentWidget() == &box);
    delete first;
    CHECK(box.count() == 0 && box.currentIndex() == -1 && current.last() == -1);
}

void testSmallWidgets() {
    QCommandLinkButton link("Save", "Keep the changes.");
    CHECK(link.text() == QString("Save") && link.description() == QString("Keep the changes.") && link.isFlat());
    const int oneLine = link.sizeHint().height();
    link.setDescription("A much longer description that has to wrap onto more than one line in the button.");
    CHECK(link.sizeHint().height() > oneLine && link.sizeHint().width() >= 200);
    int clicks = 0;
    QObject::connect(&link, &QPushButton::clicked, [&] { ++clicks; });
    link.click();
    CHECK(clicks == 1);

    QFontComboBox fonts;
    CHECK(fonts.count() >= 3 && fonts.itemText(0) == QString("Sans Serif"));
    QList<QString> families;
    QObject::connect(&fonts, &QFontComboBox::currentFontChanged, [&](const QFont &f) { families << f.family(); });
    fonts.setCurrentFont(QFont("Monospace"));
    CHECK(fonts.currentFont().family() == QString("Monospace") && families.last() == QString("Monospace"));
    fonts.setCurrentFont(QFont("No Such Family"));
    CHECK(fonts.currentText() == QString("No Such Family"));
    fonts.setFontFilters(QFontComboBox::MonospacedFonts);
    CHECK(fonts.fontFilters() == QFontComboBox::MonospacedFonts);

    QWidget top;
    QKeySequenceEdit edit(&top);
    top.resize(200, 40);
    edit.resize(150, 30);
    top.show();
    settle(&top);
    QList<QKeySequence> changes;
    int finished = 0;
    QObject::connect(&edit, &QKeySequenceEdit::keySequenceChanged, [&](const QKeySequence &k) { changes << k; });
    QObject::connect(&edit, &QKeySequenceEdit::editingFinished, [&] { ++finished; });
    edit.setFocus();
    key(&edit, Qt::Key_Control, QString(), Qt::ControlModifier); // a modifier alone records nothing
    CHECK(edit.keySequence().isEmpty());
    key(&edit, Qt::Key_S, "s", Qt::ControlModifier);
    CHECK(edit.keySequence() == QKeySequence(Qt::Key_S | static_cast<int>(Qt::ControlModifier)) && changes.size() == 1);
    edit.clearFocus(); // losing the focus finishes
    CHECK(finished == 1);
    edit.clear();
    CHECK(edit.keySequence().isEmpty());
    edit.setKeySequence(QKeySequence(Qt::Key_F5));
    CHECK(edit.keySequence() == QKeySequence(Qt::Key_F5));
}

namespace {
struct HookScene : QGraphicsScene {
    using QGraphicsScene::focusNextPrevChild;
    using QGraphicsScene::helpEvent;
};
struct HookWidget : QGraphicsWidget {
    int grabs = 0, ungrabs = 0;
    QRectF lastGeometry;
    using QGraphicsWidget::focusNextPrevChild;
    void grabMouseEvent(QEvent *) override { ++grabs; }
    void ungrabMouseEvent(QEvent *) override { ++ungrabs; }
    QVariant propertyChange(const QString &name, const QVariant &value) override {
        if (name == "geometry") {
            lastGeometry = value.toRectF();
            return QVariant(lastGeometry.adjusted(0, 0, 10, 0)); // grows by 10
        }
        return value;
    }
};
struct HookList : QListView {
    using QListView::rectForIndex;
    using QListView::setPositionForIndex;
};
} // namespace

void testWidgetGaps() {
    QCalendarWidget calendar;
    QTextCharFormat red;
    red.setForeground(QBrush(Qt::red));
    calendar.setDateTextFormat(QDate(2024, 2, 14), red);
    CHECK(calendar.dateTextFormat(QDate(2024, 2, 14)).foreground().color() == QColor(Qt::red));
    CHECK(calendar.dateTextFormat().size() == 1);
    calendar.setDateTextFormat(QDate(), QTextCharFormat());
    CHECK(calendar.dateTextFormat().isEmpty());
    calendar.setHorizontalHeaderFormat(QCalendarWidget::SingleLetterDayNames);
    CHECK(calendar.horizontalHeaderFormat() == QCalendarWidget::SingleLetterDayNames);
    calendar.setMaximumDate(QDate(2030, 1, 1));
    calendar.clearMaximumDate();
    CHECK(calendar.maximumDate() == QDate(9999, 12, 31));

    QFontComboBox fonts;
    fonts.setSampleTextForFont("Serif", "abc");
    CHECK(fonts.sampleTextForFont("Serif") == "abc" && fonts.displayFont("Serif").family() == "Serif");

    QWizard wizard;
    wizard.setButtonLayout({QWizard::Stretch, QWizard::NextButton});
    wizard.setTitleFormat(Qt::PlainText);
    CHECK(wizard.titleFormat() == Qt::PlainText);

    CHECK(QApplication::style()->layoutSpacing(QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Horizontal) >= 0);
    CHECK(QApplication::style()->combinedLayoutSpacing(QSizePolicy::PushButton | QSizePolicy::Label,
                                                       QSizePolicy::LineEdit, Qt::Vertical) >= 0);
    QPixmap pixmap(4, 4);
    pixmap.fill(Qt::blue);
    CHECK(!QApplication::style()->generatedIconPixmap(QIcon::Disabled, pixmap, nullptr).isNull());

    QGraphicsScene scene;
    scene.setMinimumRenderSize(2);
    CHECK(scene.minimumRenderSize() == 2 && scene.style() == QApplication::style());

    auto *gw = new QGraphicsWidget;
    scene.addItem(gw);
    gw->setWindowFrameMargins(2, 10, 2, 2);
    gw->resize(100, 50);
    CHECK(gw->windowFrameRect() == QRectF(-2, -10, 104, 62));
    CHECK(gw->windowFrameSectionAt(QPointF(50, -5)) == Qt::TitleBarArea &&
          gw->windowFrameSectionAt(QPointF(50, 20)) == Qt::NoSection);
    QAction action;
    gw->addAction(&action);
    CHECK(gw->actions().size() == 1);
    gw->removeAction(&action);
    gw->setAttribute(Qt::WA_Hover);
    CHECK(gw->testAttribute(Qt::WA_Hover) && gw->style() == scene.style());
    CHECK(gw->close() && !gw->isVisible());

    QTreeWidget tree;
    tree.setColumnCount(2);
    auto *header = new QTreeWidgetItem;
    header->setText(0, "A");
    tree.setHeaderItem(header);
    CHECK(tree.headerItem() == header && tree.columnCount() == 2);

    QMdiArea area;
    area.setTabPosition(QTabWidget::South);
    area.setDocumentMode(true);
    CHECK(area.tabPosition() == QTabWidget::South && area.documentMode());
    auto *sub = area.addSubWindow(new QWidget);
    sub->resize(200, 100);
    sub->showShaded();
    CHECK(sub->isShaded() && sub->height() < 100);
    sub->showShaded();
    CHECK(!sub->isShaded() && sub->height() == 100);

    QPanGesture pan;
    pan.setLastOffset(QPointF(1, 1));
    pan.setOffset(QPointF(4, 5));
    CHECK(pan.gestureType() == Qt::PanGesture && pan.delta() == QPointF(3, 4));
    QSwipeGesture swipe;
    swipe.setSwipeAngle(90);
    CHECK(swipe.verticalDirection() == QSwipeGesture::Up && swipe.horizontalDirection() == QSwipeGesture::NoDirection);
    QGestureEvent gestureEvent({&pan});
    CHECK(gestureEvent.gesture(Qt::PanGesture) == &pan && !gestureEvent.isAccepted(Qt::PanGesture));
    gestureEvent.accept(&pan);
    CHECK(gestureEvent.isAccepted(&pan));
    CHECK(QColormap::instance().pixel(QColor(255, 0, 0)) == 0xffff0000u);
    QStyleOptionHeaderV2 headerOption;
    CHECK(headerOption.version == 2 && headerOption.type == QStyleOption::SO_Header);

    // Anchors: a fills the layout's width; b sits below a with the default spacing
    {
        QGraphicsWidget form;
        auto *anchors = new QGraphicsAnchorLayout;
        auto *a = new QGraphicsWidget;
        auto *b = new QGraphicsWidget;
        a->setPreferredSize(50, 20);
        b->setPreferredSize(50, 30);
        anchors->addAnchors(anchors, a, Qt::Horizontal);
        anchors->addAnchor(anchors, Qt::AnchorTop, a, Qt::AnchorTop);
        anchors->addAnchor(a, Qt::AnchorBottom, b, Qt::AnchorTop);
        anchors->addAnchor(b, Qt::AnchorLeft, a, Qt::AnchorLeft)->setSpacing(0);
        anchors->setContentsMargins(0, 0, 0, 0);
        form.setLayout(anchors);
        form.resize(200, 100);
        anchors->activate();
        CHECK(a->geometry().width() == 200 && a->geometry().top() == 0);
        CHECK(b->geometry().top() == a->geometry().bottom() + 6 && b->geometry().left() == 0);
        CHECK(anchors->anchor(a, Qt::AnchorBottom, b, Qt::AnchorTop) != nullptr);
        anchors->anchor(a, Qt::AnchorBottom, b, Qt::AnchorTop)->setSpacing(10);
        anchors->activate();
        CHECK(b->geometry().top() == a->geometry().bottom() + 10);
        CHECK(anchors->addAnchor(a, Qt::AnchorLeft, b, Qt::AnchorTop) == nullptr);
    }

    QTextDocument textDocument;
    textDocument.setPlainText("one\ntwo");
    QPlainTextDocumentLayout textLayout(&textDocument);
    CHECK(textLayout.pageCount() == 1 && textLayout.documentSize().height() > 0 && textLayout.cursorWidth() == 1);

    // Scene: active window, focus chain; widget hooks
    {
        HookScene hookScene;
        auto *first = new QGraphicsRectItem(0, 0, 10, 10);
        auto *second = new QGraphicsRectItem(20, 0, 10, 10);
        first->setFlag(QGraphicsItem::ItemIsFocusable);
        second->setFlag(QGraphicsItem::ItemIsFocusable);
        hookScene.addItem(first);
        hookScene.addItem(second);
        hookScene.setActivePanel(first);
        CHECK(hookScene.focusItem() == first);
        CHECK(hookScene.focusNextPrevChild(true) && hookScene.focusItem() == second);
        CHECK(hookScene.focusNextPrevChild(true) && hookScene.focusItem() == first); // wraps
        CHECK(hookScene.focusNextPrevChild(false) && hookScene.focusItem() == second);

        auto *window = new HookWidget;
        hookScene.addItem(window);
        hookScene.setActiveWindow(window);
        CHECK(hookScene.activeWindow() == window);
        window->setGeometry(0, 0, 50, 20);
        CHECK(window->lastGeometry == QRectF(0, 0, 50, 20) && window->geometry().width() == 60);
        window->setFlag(QGraphicsItem::ItemIsFocusable);
        window->grabMouse();
        window->ungrabMouse();
        CHECK(window->grabs == 1 && window->ungrabs == 1);
        window->setFocus();
        CHECK(!window->focusNextPrevChild(true)); // only itself in its chain
        delete window;
        CHECK(hookScene.activeWindow() == nullptr);
    }

    {
        QStandardItemModel model;
        for (int i = 0; i < 3; ++i)
            model.appendRow(new QStandardItem(QString::number(i)));
        HookList list;
        list.setModel(&model);
        list.resize(200, 200);
        list.show();
        list.setPositionForIndex(QPoint(120, 90), model.index(1, 0));
        CHECK(list.rectForIndex(model.index(1, 0)).topLeft() == QPoint(120, 90));
        QStyledItemDelegate delegate;
        CHECK(delegate.paintingRoles().isEmpty());
    }

    QFileDialog dialog;
    dialog.setDirectoryUrl(QUrl::fromLocalFile("/tmp"));
    CHECK(dialog.directoryUrl() == QUrl::fromLocalFile("/tmp") && dialog.history().contains("/tmp"));
    QFileDialog other;
    CHECK(other.restoreState(dialog.saveState()) && other.directory().absolutePath() == "/tmp");
    CHECK(!other.restoreState(QByteArray("junk")));
}

void testCalendar() {
    QCalendarWidget calendar;
    calendar.setSelectedDate(QDate(2024, 2, 14));
    CHECK(calendar.selectedDate() == QDate(2024, 2, 14) && calendar.monthShown() == 2 && calendar.yearShown() == 2024);
    CHECK(calendar.firstDayOfWeek() == Qt::Monday && calendar.minimumDate() == QDate(1752, 9, 14));
    calendar.resize(calendar.sizeHint());
    calendar.show();
    settle(&calendar);

    QList<QDate> clicked;
    int selections = 0;
    std::vector<std::pair<int, int>> pages;
    QObject::connect(&calendar, &QCalendarWidget::clicked, [&](QDate d) { clicked << d; });
    QObject::connect(&calendar, &QCalendarWidget::selectionChanged, [&] { ++selections; });
    QObject::connect(&calendar, &QCalendarWidget::currentPageChanged, [&](int y, int m) { pages.emplace_back(y, m); });

    // Keys move the selection
    calendar.setFocus();
    key(&calendar, Qt::Key_Right);
    CHECK(calendar.selectedDate() == QDate(2024, 2, 15) && selections == 1);
    key(&calendar, Qt::Key_Down);
    CHECK(calendar.selectedDate() == QDate(2024, 2, 22));
    key(&calendar, Qt::Key_End);
    CHECK(calendar.selectedDate() == QDate(2024, 2, 29));
    key(&calendar, Qt::Key_Right); // into March
    CHECK(calendar.selectedDate() == QDate(2024, 3, 1) && pages.size() == 1 && pages[0] == std::make_pair(2024, 3));
    key(&calendar, Qt::Key_PageUp);
    CHECK(calendar.selectedDate() == QDate(2024, 2, 1));

    // Navigation does not change the selection
    calendar.showNextMonth();
    CHECK(calendar.monthShown() == 3 && calendar.selectedDate() == QDate(2024, 2, 1));
    calendar.showPreviousYear();
    CHECK(calendar.yearShown() == 2023);
    calendar.showSelectedDate();
    CHECK(calendar.yearShown() == 2024 && calendar.monthShown() == 2);

    // The range
    calendar.setDateRange(QDate(2024, 2, 10), QDate(2024, 2, 20));
    CHECK(calendar.selectedDate() == QDate(2024, 2, 10));
    calendar.setSelectedDate(QDate(2024, 3, 1)); // out of range
    CHECK(calendar.selectedDate() == QDate(2024, 2, 10));
    key(&calendar, Qt::Key_End);
    CHECK(calendar.selectedDate() == QDate(2024, 2, 20));

    // Clicking a day: find the cell of the 15th
    QPoint at;
    for (int y = 0; y < calendar.height() && at.isNull(); y += 2)
        for (int x = 0; x < calendar.width(); x += 2) {
            calendar.setSelectedDate(QDate(2024, 2, 20));
            click(&calendar, QPoint(x, y));
            if (!clicked.isEmpty() && clicked.last() == QDate(2024, 2, 15)) {
                at = QPoint(x, y);
                break;
            }
        }
    CHECK(!at.isNull() && calendar.selectedDate() == QDate(2024, 2, 15));
    calendar.setSelectionMode(QCalendarWidget::NoSelection);
    key(&calendar, Qt::Key_Left);
    CHECK(calendar.selectedDate() == QDate(2024, 2, 15));
    calendar.setFirstDayOfWeek(Qt::Sunday);
    calendar.setGridVisible(true);
    calendar.setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    calendar.setNavigationBarVisible(false);
    CHECK(calendar.firstDayOfWeek() == Qt::Sunday && calendar.isGridVisible() && !calendar.isNavigationBarVisible());
}

void testDateTimeEdit() {
    QWidget top;
    auto *layout = new QVBoxLayout(&top);
    QDateTimeEdit edit(QDateTime(QDate(2024, 1, 31), QTime(13, 45)));
    layout->addWidget(&edit);
    top.resize(300, 80);
    top.show();
    settle(&top);
    QLineEdit *line = editOf(&edit);
    CHECK(edit.displayFormat() == QString("yyyy-MM-dd HH:mm") && edit.text() == QString("2024-01-31 13:45"));
    CHECK(edit.sectionCount() == 5 &&
          edit.displayedSections() ==
              (QDateTimeEdit::YearSection | QDateTimeEdit::MonthSection | QDateTimeEdit::DaySection |
               QDateTimeEdit::HourSection | QDateTimeEdit::MinuteSection));
    CHECK(edit.sectionText(QDateTimeEdit::MonthSection) == QString("01"));

    QList<QDate> dates;
    QList<QTime> times;
    QObject::connect(&edit, &QDateTimeEdit::dateChanged, [&](QDate d) { dates << d; });
    QObject::connect(&edit, &QDateTimeEdit::timeChanged, [&](QTime t) { times << t; });

    // Stepping the section of the cursor: the month keeps the day in the month
    line->setFocus();
    edit.setCurrentSection(QDateTimeEdit::MonthSection);
    CHECK(edit.currentSection() == QDateTimeEdit::MonthSection);
    key(line, Qt::Key_Up);
    CHECK(edit.date() == QDate(2024, 2, 29) && line->selectedText() == QString("02") && dates.size() == 1);
    edit.setCurrentSection(QDateTimeEdit::DaySection);
    key(line, Qt::Key_Up); // the end of the month: it stops
    CHECK(edit.date() == QDate(2024, 2, 29));
    edit.setWrapping(true);
    edit.setCurrentSection(QDateTimeEdit::DaySection);
    key(line, Qt::Key_Up);
    CHECK(edit.date() == QDate(2024, 2, 1));
    edit.setWrapping(false);
    edit.setCurrentSection(QDateTimeEdit::HourSection);
    key(line, Qt::Key_Down);
    CHECK(edit.time() == QTime(12, 45) && times.size() == 1);
    edit.setCurrentSection(QDateTimeEdit::YearSection);
    edit.stepBy(2);
    CHECK(edit.date().year() == 2026);

    // Typing a date and time
    line->selectAll();
    type(line, "2025-12-24 08:30");
    CHECK(edit.dateTime() == QDateTime(QDate(2025, 12, 24), QTime(8, 30)));
    line->selectAll();
    type(line, "2025-13-01 00:00"); // no thirteenth month: the value stays
    CHECK(edit.date() == QDate(2025, 12, 24));
    key(line, Qt::Key_Return);
    CHECK(edit.text() == QString("2025-12-24 08:30"));
    // A number that fills its section moves on to the next one and selects it, as in Qt
    edit.setSelectedSection(QDateTimeEdit::HourSection);
    type(line, "21");
    CHECK(edit.time() == QTime(21, 30) && edit.currentSection() == QDateTimeEdit::MinuteSection &&
          line->selectedText() == QString("30"));
    type(line, "45"); // the last section: the cursor stays
    CHECK(edit.time() == QTime(21, 45) && line->cursorPosition() == 16 && !line->hasSelectedText());
    edit.setSelectedSection(QDateTimeEdit::MonthSection);
    type(line, "3"); // no month 30 or above: one digit fills it, which gets its zero
    CHECK(edit.text() == QString("2025-03-24 21:45") && line->selectedText() == QString("24"));
    type(line, "1"); // a day may be 10 to 19: it waits for a second digit
    CHECK(line->text() == QString("2025-03-1 21:45") && line->cursorPosition() == 9);
    key(line, Qt::Key_Return);
    CHECK(edit.date() == QDate(2025, 3, 1));
    // The separator that follows a section, typed in it, moves on to the next one and writes the section with its zero
    edit.setSelectedSection(QDateTimeEdit::HourSection);
    type(line, "1");
    CHECK(line->text() == QString("2025-03-01 1:45"));
    type(line, ":");
    CHECK(line->text() == QString("2025-03-01 01:45") && edit.time() == QTime(1, 45) &&
          edit.currentSection() == QDateTimeEdit::MinuteSection && line->selectedText() == QString("45"));
    edit.setSelectedSection(QDateTimeEdit::MonthSection);
    type(line, "1-");
    CHECK(line->text() == QString("2025-01-01 01:45") && line->selectedText() == QString("01") &&
          edit.currentSection() == QDateTimeEdit::DaySection);
    type(line, "2 ");
    CHECK(edit.date() == QDate(2025, 1, 2) && edit.currentSection() == QDateTimeEdit::HourSection &&
          line->selectedText() == QString("01"));
    type(line, "/"); // not the separator that follows: nothing
    CHECK(line->text() == QString("2025-01-02 01:45"));
    edit.setDisplayFormat("dd.MM.yyyy");
    edit.setSelectedSection(QDateTimeEdit::DaySection);
    type(line, "3.");
    CHECK(line->text() == QString("03.01.2025") && edit.currentSection() == QDateTimeEdit::MonthSection);
    edit.setDisplayFormat("yyyy-MM-dd HH:mm");
    edit.setDateTime(QDateTime(QDate(2025, 12, 24), QTime(8, 30)));

    // The range
    edit.setDateTimeRange(QDateTime(QDate(2025, 1, 1), QTime(0, 0)), QDateTime(QDate(2025, 6, 30), QTime(12, 0)));
    CHECK(edit.dateTime() == QDateTime(QDate(2025, 6, 30), QTime(12, 0)) && edit.maximumDate() == QDate(2025, 6, 30));
    edit.setCurrentSection(QDateTimeEdit::DaySection);
    key(line, Qt::Key_Up); // at the maximum
    CHECK(edit.date() == QDate(2025, 6, 30));
    edit.setDate(QDate(2024, 5, 5));
    CHECK(edit.date() == QDate(2025, 1, 1));
    edit.clearMinimumDateTime();
    edit.clearMaximumDateTime();

    // Names, a 12-hour clock and literals
    edit.setDisplayFormat("dddd d MMMM yyyy 'at' h:mm AP");
    edit.setDateTime(QDateTime(QDate(2024, 3, 1), QTime(15, 5)));
    CHECK(edit.text() == QString("Friday 1 March 2024 at 3:05 PM"));
    line->selectAll();
    type(line, "Monday 4 March 2024 at 9:15 AM");
    CHECK(edit.dateTime() == QDateTime(QDate(2024, 3, 4), QTime(9, 15)));
    line->selectAll();
    type(line, "Friday 1 Smarch 2024 at 3:05 PM"); // no such month: the value stays
    CHECK(edit.dateTime() == QDateTime(QDate(2024, 3, 4), QTime(9, 15)));
    key(line, Qt::Key_Return);
    edit.setDateTime(QDateTime(QDate(2024, 3, 1), QTime(15, 5)));
    edit.setCurrentSection(QDateTimeEdit::AmPmSection);
    edit.stepBy(1);
    CHECK(edit.time() == QTime(3, 5));
    // In the AM/PM section, a and p choose; the last section stays selected
    edit.setSelectedSection(QDateTimeEdit::AmPmSection);
    type(line, "p");
    CHECK(edit.time() == QTime(15, 5) && line->selectedText() == QString("PM"));
    type(line, "A");
    CHECK(edit.time() == QTime(3, 5) && line->selectedText() == QString("AM"));
    type(line, "x"); // neither: nothing
    CHECK(line->text() == QString("Friday 1 March 2024 at 3:05 AM"));
    edit.setDisplayFormat("AP h:mm");
    edit.setSelectedSection(QDateTimeEdit::AmPmSection);
    type(line, "p"); // and move on to the section after it
    CHECK(edit.time() == QTime(15, 5) && edit.currentSection() == QDateTimeEdit::HourSection &&
          line->selectedText() == QString("3"));

    // The calendar popup
    edit.setDisplayFormat("yyyy-MM-dd");
    edit.setCalendarPopup(true);
    CHECK(edit.calendarPopup() && edit.calendarWidget());
    QStyleOptionSpinBox option;
    option.initFrom(&edit);
    const QRect button = edit.style()->subElementRect(QStyle::SE_SpinBoxUp, &option, &edit);
    click(&edit, button.center());
    QCalendarWidget *popup = edit.calendarWidget();
    CHECK(popup->isVisible() && popup->window() == &top && popup->selectedDate() == edit.date());
    emit popup->clicked(QDate(2024, 3, 9));
    CHECK(edit.date() == QDate(2024, 3, 9) && !popup->isVisible());

    // QDateEdit and QTimeEdit
    QDateEdit dateEdit(QDate(2020, 2, 2));
    QTimeEdit timeEdit(QTime(7, 30));
    CHECK(dateEdit.text() == QString("2020-02-02") && dateEdit.date() == QDate(2020, 2, 2));
    CHECK(timeEdit.text() == QString("07:30") && timeEdit.time() == QTime(7, 30));
    timeEdit.setTime(QTime(23, 59));
    timeEdit.setCurrentSectionIndex(1);
    timeEdit.stepBy(1);
    CHECK(timeEdit.time() == QTime(23, 59));
}

void testCompleter() {
    QWidget top;
    auto *layout = new QVBoxLayout(&top);
    QLineEdit edit;
    layout->addWidget(&edit);
    layout->addStretch();
    top.resize(300, 300);
    top.show();
    settle(&top);
    QCompleter completer(QStringList{"apple", "apricot", "Avocado", "banana"});
    edit.setCompleter(&completer);
    CHECK(edit.completer() == &completer && completer.widget() == &edit);
    QStringList activated;
    QObject::connect(&completer, &QCompleter::activated, [&](const QString &t) { activated << t; });

    // Typing shows the words that start with the text
    edit.setFocus();
    type(&edit, "ap");
    auto *popup = dynamic_cast<QListWidget *>(completer.popup());
    CHECK(popup && popup->isVisible() && popup->count() == 2 && completer.completionCount() == 2);
    CHECK(completer.completionPrefix() == QString("ap") && completer.currentCompletion() == QString("apple"));
    CHECK(edit.hasFocus()); // the popup does not take the focus
    key(&edit, Qt::Key_Down);
    CHECK(completer.currentRow() == 1 && completer.currentCompletion() == QString("apricot"));
    key(&edit, Qt::Key_Return);
    CHECK(edit.text() == QString("apricot") && !popup->isVisible() && activated == QStringList({"apricot"}));

    // Case, filter modes and Escape
    edit.clear();
    completer.setCaseSensitivity(Qt::CaseInsensitive);
    type(&edit, "a");
    CHECK(completer.completionCount() == 3);
    key(&edit, Qt::Key_Escape);
    CHECK(!popup->isVisible());
    edit.clear();
    completer.setFilterMode(Qt::MatchContains);
    type(&edit, "an");
    CHECK(completer.completionCount() == 1 && completer.currentCompletion() == QString("banana"));
    click(popup, popup->visualItemRect(popup->item(0)).center());
    CHECK(edit.text() == QString("banana") && activated.last() == QString("banana"));
    completer.setFilterMode(Qt::MatchWildcard); // refused
    CHECK((completer.filterMode() & Qt::MatchTypeMask) == Qt::MatchContains);

    // Inline completion puts the rest after the text, selected
    completer.setFilterMode(Qt::MatchStartsWith);
    completer.setCompletionMode(QCompleter::InlineCompletion);
    edit.clear();
    type(&edit, "ban");
    CHECK(edit.text() == QString("banana") && edit.selectedText() == QString("ana"));
    key(&edit, Qt::Key_Backspace); // deleting does not complete again
    CHECK(edit.text() == QString("ban"));

    // The popup of all the words
    completer.setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    edit.clear();
    type(&edit, "b");
    CHECK(completer.completionCount() == 4 && completer.currentCompletion() == QString("banana"));
    CHECK(completer.setCurrentRow(0) && !completer.setCurrentRow(9));
    completer.setMaxVisibleItems(2);
    completer.setWrapAround(false);
    CHECK(completer.maxVisibleItems() == 2 && !completer.wrapAround());
    edit.setCompleter(nullptr);
    CHECK(completer.widget() == nullptr);
}
} // namespace

// What the widgets that take text tell an input method, for the keys that an on-screen keyboard shows.
static void testInputHints() {
    // A line edit says it as the focus widget: of a spin box, that is the line edit inside it.
    const auto hintsOf = [](const QWidget *widget) { return widget->inputMethodQuery(Qt::ImHints).toInt(); };
    QSpinBox count;
    auto *digits = count.findChild<QLineEdit *>();
    CHECK(digits && hintsOf(digits) == static_cast<int>(Qt::ImhDigitsOnly));
    count.setRange(-10, 10); // a sign can be typed
    CHECK(digits && hintsOf(digits) == static_cast<int>(Qt::ImhFormattedNumbersOnly));
    count.setMinimum(0);
    CHECK(digits && hintsOf(digits) == static_cast<int>(Qt::ImhDigitsOnly));
    QDoubleSpinBox weight;
    CHECK(hintsOf(weight.findChild<QLineEdit *>()) == static_cast<int>(Qt::ImhFormattedNumbersOnly));
    QTimeEdit time;
    time.setDisplayFormat(QStringLiteral("HH:mm"));
    CHECK(hintsOf(time.findChild<QLineEdit *>()) == static_cast<int>(Qt::ImhPreferNumbers));
    time.setDisplayFormat(QStringLiteral("h:mm AP"));
    CHECK(hintsOf(time.findChild<QLineEdit *>()) == 0);
    QDateEdit date;
    date.setDisplayFormat(QStringLiteral("d MMM yyyy"));
    CHECK(hintsOf(date.findChild<QLineEdit *>()) == 0);

    QLineEdit field;
    CHECK(hintsOf(&field) == 0);
    field.setInputMethodHints(Qt::ImhEmailCharactersOnly);
    CHECK(hintsOf(&field) == static_cast<int>(Qt::ImhEmailCharactersOnly));
    // A password says that it is hidden, besides what it was given, until it is shown.
    field.setInputMethodHints(Qt::ImhDigitsOnly);
    field.setEchoMode(QLineEdit::Password);
    const int hidden = static_cast<int>(Qt::ImhHiddenText);
    CHECK((hintsOf(&field) & hidden) && (hintsOf(&field) & static_cast<int>(Qt::ImhSensitiveData)));
    CHECK(hintsOf(&field) & static_cast<int>(Qt::ImhDigitsOnly));
    CHECK(field.inputMethodHints() == Qt::InputMethodHints(Qt::ImhDigitsOnly)); // what it was given is as it was
    field.setEchoMode(QLineEdit::Normal);
    CHECK(!(hintsOf(&field) & hidden));
}

// On a platform that narrows its dialogs to fit the screen, a dialog narrows itself and is as high as its wrapped
// text then needs.
static void testNarrowDialogs() {
    auto *platform = dynamic_cast<QOffscreenIntegration *>(QtPrivate::GuiAccess::integration());
    CHECK(platform != nullptr);
    if (!platform)
        return;
    const QString text = QStringLiteral("A message that is much too long for one line of a dialog on a telephone, so "
                                        "that it has to be broken into several of them to be read at all.");
    const auto needed = [](const QLabel *label) {
        return label->fontMetrics()
            .boundingRect(QRect(0, 0, label->width(), 0), Qt::TextWordWrap, label->text())
            .height();
    };
    for (const int limit : {312, 0}) {
        platform->dialogWidthLimit = limit;
        QMessageBox box(QMessageBox::Information, QStringLiteral("About"), text, QMessageBox::Ok);
        box.show();
        QCoreApplication::processEvents();
        QLabel *message = nullptr;
        const QList<QLabel *> labels = box.findChildren<QLabel *>();
        for (QLabel *label : labels) {
            if (label->text() == text)
                message = label;
        }
        CHECK(message != nullptr);
        if (!message)
            continue;
        if (limit > 0) {
            CHECK(box.width() == limit);
            CHECK(message->height() >= needed(message));
            // All of the text, and the button under it, are inside the dialog.
            const QRect shown(message->mapTo(&box, QPoint(0, 0)), message->size());
            CHECK(box.rect().contains(shown));
            const QList<QPushButton *> buttons = box.findChildren<QPushButton *>();
            CHECK(!buttons.isEmpty() && buttons.first()->mapTo(&box, QPoint(0, 0)).y() >= shown.y() + shown.height());
        } else {
            CHECK(message->minimumHeight() == 0); // a platform with room leaves the dialog as it was
        }
    }
    // A dialog that is narrow enough, and one with a size of its own, are left alone.
    platform->dialogWidthLimit = 312;
    QDialog sized;
    sized.resize(500, 200);
    sized.show();
    QCoreApplication::processEvents();
    CHECK(sized.width() == 500);
    platform->dialogWidthLimit = 0;
}

// What is typed in a spin box goes into its number, never into the prefix or the suffix, wherever the text was
// clicked.
static void testSpinBoxAffixes() {
    QWidget window;
    auto *spin = new QSpinBox(&window);
    spin->setRange(0, 999);
    spin->setPrefix(QStringLiteral("~ "));
    spin->setSuffix(QStringLiteral(" nuts"));
    spin->setGeometry(0, 0, 220, 40);
    window.resize(300, 100);
    window.show();
    window.activateWindow();
    auto *line = spin->findChild<QLineEdit *>();
    CHECK(line != nullptr);
    if (!line)
        return;
    line->setFocus();
    QCoreApplication::processEvents();
    const auto key = [&](int code, const QString &text = QString(), Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QWindowSystemInterface::handleKeyEvent(window.windowHandle(), QEvent::KeyPress, code, modifiers, text);
        QWindowSystemInterface::handleKeyEvent(window.windowHandle(), QEvent::KeyRelease, code, modifiers, text);
        QCoreApplication::processEvents();
    };
    // A click past the end of the text leaves the cursor after the number.
    const QPointF past(line->width() - 3, line->height() / 2);
    QMouseEvent press(QEvent::MouseButtonPress, past, past, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(line, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, past, past, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(line, &release);
    CHECK(line->text() == QStringLiteral("~ 0 nuts") && line->cursorPosition() == 3);
    key(Qt::Key_5, QStringLiteral("5"));
    CHECK(line->text() == QStringLiteral("~ 05 nuts") && spin->value() == 5);
    // A key that an on-screen keyboard sends has no key code, only its text.
    key(Qt::Key_unknown, QStringLiteral("7"));
    CHECK(spin->value() == 57);
    // Nor does the cursor go there by the keys, or by the program.
    key(Qt::Key_End);
    CHECK(line->cursorPosition() == static_cast<int>(line->text().size()) - 5);
    key(Qt::Key_Delete); // nothing of the suffix goes
    CHECK(line->text().endsWith(QStringLiteral(" nuts")));
    key(Qt::Key_Home);
    CHECK(line->cursorPosition() == 2);
    key(Qt::Key_Backspace); // nor of the prefix
    CHECK(line->text().startsWith(QStringLiteral("~ ")));
    key(Qt::Key_Left);
    CHECK(line->cursorPosition() == 2);
    line->setCursorPosition(0);
    CHECK(line->cursorPosition() == 2);
    // Select All takes the number alone, so that typing over it replaces no more.
    key(Qt::Key_A, QString(), Qt::ControlModifier);
    CHECK(line->selectedText() == line->text().mid(2, line->text().size() - 7));
    key(Qt::Key_8, QStringLiteral("8"));
    CHECK(line->text() == QStringLiteral("~ 8 nuts") && spin->value() == 8);
    key(Qt::Key_Backspace);
    key(Qt::Key_3, QStringLiteral("3"));
    CHECK(line->text() == QStringLiteral("~ 3 nuts") && spin->value() == 3);
    // After the program or a step has changed the value, the cursor is still in the number.
    spin->setValue(120);
    CHECK(line->cursorPosition() <= 5);
    spin->setSuffix(QStringLiteral(" nuts left"));
    CHECK(line->cursorPosition() <= 5);
    // The special value text is shown whole, and the cursor may be anywhere in it.
    spin->setSpecialValueText(QStringLiteral("None at all"));
    spin->setValue(0);
    line->setCursorPosition(11);
    CHECK(line->text() == QStringLiteral("None at all") && line->cursorPosition() == 11);

    QDoubleSpinBox weight;
    weight.setSuffix(QStringLiteral(" kg"));
    auto *grams = weight.findChild<QLineEdit *>();
    grams->setCursorPosition(100);
    CHECK(grams->cursorPosition() == static_cast<int>(grams->text().size()) - 3);
}

// What a platform with an on-screen keyboard is told: how short the window can be, where the field is, and to show
// the keyboard again when the field that has it is tapped.
static void testInputPanel() {
    auto *platform = dynamic_cast<QOffscreenIntegration *>(QtPrivate::GuiAccess::integration());
    CHECK(platform != nullptr);
    if (!platform)
        return;
    platform->inputPanel = {};
    platform->inputPanel.present = true;

    // Two pages of a tab widget, one much taller than the other.
    QWidget window;
    auto *rows = new QVBoxLayout(&window);
    rows->setContentsMargins(0, 0, 0, 0);
    auto *tabs = new QTabWidget;
    rows->addWidget(tabs);
    auto *shortPage = new QWidget;
    auto *shortRows = new QVBoxLayout(shortPage);
    auto *first = new QLineEdit;
    auto *second = new QLineEdit;
    shortRows->addWidget(first);
    shortRows->addStretch(1);
    shortRows->addWidget(second);
    auto *tall = new QWidget;
    auto *tallRows = new QVBoxLayout(tall);
    auto *block = new QWidget;
    block->setMinimumHeight(900);
    tallRows->addWidget(block);
    tabs->addTab(shortPage, QStringLiteral("Short"));
    tabs->addTab(tall, QStringLiteral("Tall"));
    window.resize(300, 400);
    window.show();
    window.activateWindow();
    second->setFocus();
    QCoreApplication::processEvents();
    CHECK(platform->inputPanel.visible && platform->inputPanel.window == window.windowHandle());
    CHECK(platform->inputPanel.item == QRect(second->mapTo(&window, QPoint(0, 0)), second->size()));
    // The page that is hidden does not count, though the window itself asks for its room.
    CHECK(window.minimumSizeHint().height() > 900);
    const int least = platform->inputPanel.minimumHeight;
    CHECK(least >= shortPage->minimumSizeHint().height() && least < 200);

    // A window that is resized tells where the field has gone.
    window.resize(300, 300);
    QCoreApplication::processEvents();
    CHECK(platform->inputPanel.item == QRect(second->mapTo(&window, QPoint(0, 0)), second->size()));

    // The user put the keyboard away, without Nt hearing of it: a tap on the field asks for it again.
    const auto click = [&](QWidget *on, const QPoint &from, const QPoint &to) {
        const QPoint down = on->mapTo(&window, from), up = on->mapTo(&window, to);
        QWindowSystemInterface::handleMouseEvent(window.windowHandle(), down, down, Qt::LeftButton, Qt::LeftButton,
                                                 QEvent::MouseButtonPress, Qt::NoModifier);
        QWindowSystemInterface::handleMouseEvent(window.windowHandle(), up, up, Qt::NoButton, Qt::LeftButton,
                                                 QEvent::MouseButtonRelease, Qt::NoModifier);
        QCoreApplication::processEvents();
    };
    int before = platform->inputPanel.requests;
    click(second, QPoint(10, 5), QPoint(10, 5));
    CHECK(platform->inputPanel.requests == before + 1);
    // A press that drags, as one that scrolls does, asks for nothing.
    before = platform->inputPanel.requests;
    click(second, QPoint(10, 5), QPoint(120, 5));
    CHECK(platform->inputPanel.requests == before);
    // Another field gets it too, though Nt believes it is shown already.
    click(first, QPoint(10, 5), QPoint(10, 5));
    CHECK(first->hasFocus() && platform->inputPanel.requests > before);
    CHECK(platform->inputPanel.item.top() == first->mapTo(&window, QPoint(0, 0)).y());

    // A spin box's buttons step without asking for the keys; its text asks for them, and then a button leaves them be.
    auto *spin = new QSpinBox(shortPage);
    shortRows->insertWidget(1, spin);
    spin->setRange(0, 9);
    spin->show();
    QCoreApplication::processEvents();
    auto *digits = spin->findChild<QLineEdit *>();
    QStyleOptionSpinBox option;
    option.initFrom(spin);
    const QRect plus = spin->style()->subElementRect(QStyle::SE_SpinBoxUp, &option, spin);
    second->setFocus();
    before = platform->inputPanel.requests;
    click(spin, plus.center(), plus.center());
    CHECK(spin->value() == 1 && second->hasFocus() && platform->inputPanel.requests == before);
    click(digits, QPoint(5, 5), QPoint(5, 5));
    CHECK(digits->hasFocus() && platform->inputPanel.requests > before);
    before = platform->inputPanel.requests;
    click(spin, plus.center(), plus.center());
    CHECK(spin->value() == 2 && platform->inputPanel.requests == before);
    // Without a keyboard on the screen, a button gives the field the focus, for the arrow keys.
    second->setFocus();
    platform->inputPanel.present = false;
    click(spin, plus.center(), plus.center());
    CHECK(spin->value() == 3 && digits->hasFocus());
    platform->inputPanel.present = true;

    // In a scroll area, the field with the focus is scrolled into view as the window shrinks.
    QWidget other;
    auto *column = new QVBoxLayout(&other);
    column->setContentsMargins(0, 0, 0, 0);
    auto *area = new QScrollArea;
    auto *content = new QWidget;
    auto *inside = new QVBoxLayout(content);
    auto *spacer = new QWidget;
    spacer->setMinimumHeight(500);
    inside->addWidget(spacer);
    auto *low = new QLineEdit;
    inside->addWidget(low);
    area->setWidget(content);
    area->setWidgetResizable(true);
    column->addWidget(area);
    other.resize(300, 700);
    other.show();
    other.activateWindow();
    low->setFocus();
    QCoreApplication::processEvents();
    other.resize(300, 250);
    QCoreApplication::processEvents();
    const QRect seen(low->mapTo(area->viewport(), QPoint(0, 0)), low->size());
    CHECK(area->viewport()->rect().contains(seen));
    platform->inputPanel = {};
}

int main(int argc, char **argv) {
    ::setenv("QT_QPA_PLATFORM", "offscreen", 1);
    ::setenv("QT_SCALE_FACTOR", "1", 1);
    QApplication app(argc, argv);
    app.styleHints()->setColorScheme(Qt::ColorScheme::Light);
    testAbstractSlider();
    testSpinBoxBase();
    testDoubleSpinBox();
    testButtonGroup();
    testTabBar();
    testToolBox();
    testSmallWidgets();
    testCalendar();
    testWidgetGaps();
    testDateTimeEdit();
    testCompleter();
    testInputHints();
    testInputPanel();
    testSpinBoxAffixes();
    testNarrowDialogs();
    if (g_failures == 0)
        std::puts("all input tests passed");
    return g_failures == 0 ? 0 : 1;
}
