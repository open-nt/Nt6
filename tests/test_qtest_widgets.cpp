// MIT License - see LICENSE
//
// The mouse, keyboard and wheel functions of Nt Test (QTest::mouseClick() and friends) driving widgets on the
// offscreen platform, through the public API only: <QtTest> brings in qtest_widgets.h for a program that links
// Nt Widgets, and QTEST_MAIN then makes a QApplication.
#include <QtTest>

#include <QtGui/qevent.h>
#include <QtGui/qkeysequence.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qcheckbox.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qspinbox.h>
#include <QtWidgets/qwidget.h>

// Records the input events it gets.
class Probe : public QWidget {
public:
    explicit Probe(QWidget *parent = nullptr) : QWidget(parent) {
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
    }

    QStringList log;
    QPoint lastPos;
    QPoint lastGlobal;
    Qt::MouseButtons lastButtons;
    Qt::KeyboardModifiers lastModifiers;
    QPoint lastWheel;

protected:
    void mousePressEvent(QMouseEvent *e) override { mouse("press", e); }
    void mouseReleaseEvent(QMouseEvent *e) override { mouse("release", e); }
    void mouseDoubleClickEvent(QMouseEvent *e) override { mouse("dblclick", e); }
    void mouseMoveEvent(QMouseEvent *e) override { mouse("move", e); }
    void wheelEvent(QWheelEvent *e) override {
        lastWheel = e->angleDelta();
        lastModifiers = e->modifiers();
        log.append(QString::fromLatin1("wheel"));
    }
    void keyPressEvent(QKeyEvent *e) override { key("down", e); }
    void keyReleaseEvent(QKeyEvent *e) override { key("up", e); }

private:
    void mouse(const char *what, QMouseEvent *e) {
        lastPos = e->position().toPoint();
        lastGlobal = e->globalPosition().toPoint();
        lastButtons = e->buttons();
        lastModifiers = e->modifiers();
        log.append(QString::fromLatin1(what));
    }
    void key(const char *what, QKeyEvent *e) {
        lastModifiers = e->modifiers();
        log.append(
            QString::fromLatin1(what) + QString::fromLatin1(" ") + QString::number(e->key(), 16) +
            (e->text().isEmpty() ? QString() : QString::fromLatin1(" '") + e->text() + QString::fromLatin1("'")));
    }
};

class TestQtestWidgets : public QObject {
    Q_OBJECT

private slots:
    void waitForWindow();
    void clickButton();
    void mouseAtPosition();
    void mouseButtonsAndMove();
    void doubleClick();
    void typeInLineEdit();
    void keyWithModifiers();
    void keySequenceClicks();
    void wheel();
    void defaults();
    void nullTargets();
};

void TestQtestWidgets::waitForWindow() {
    QWidget w;
    w.resize(200, 100);
    QVERIFY(!w.windowHandle());
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(w.windowHandle()));
    QVERIFY(QTest::qWaitForWindowActive(w.windowHandle()));
    QVERIFY(w.windowHandle()->isExposed());

    QWidget hidden;
    hidden.show();
    hidden.hide();
    QVERIFY(!QTest::qWaitForWindowExposed(hidden.windowHandle(), 20));
    QVERIFY(!QTest::qWaitForWindowExposed(nullptr, 20));
    QVERIFY(!QTest::qWaitForWindowActive(nullptr, 20));
}

void TestQtestWidgets::clickButton() {
    QWidget top;
    top.resize(300, 200);
    QPushButton button(QString::fromLatin1("Go"), &top);
    button.setGeometry(50, 50, 100, 40);
    top.show();
    QVERIFY(QTest::qWaitForWindowExposed(top.windowHandle()));

    QSignalSpy clicked(&button, &QPushButton::clicked);
    QTest::mouseClick(&button, Qt::LeftButton);
    QCOMPARE(clicked.size(), 1);

    // The same through the window, at the button's center.
    QTest::mouseClick(top.windowHandle(), Qt::LeftButton, Qt::NoModifier, QPoint(100, 70));
    QCOMPARE(clicked.size(), 2);

    // A click next to the button does nothing.
    QTest::mouseClick(&top, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
    QCOMPARE(clicked.size(), 2);
}

void TestQtestWidgets::mouseAtPosition() {
    QWidget top;
    top.resize(300, 200);
    Probe probe(&top);
    probe.setGeometry(100, 50, 120, 80);
    top.show();
    QVERIFY(QTest::qWaitForWindowExposed(top.windowHandle()));

    // A position in the widget, mapped to the window; the default is the center.
    QTest::mousePress(&probe, Qt::LeftButton, Qt::ControlModifier, QPoint(7, 9));
    QCOMPARE(probe.log, QStringList{QString::fromLatin1("press")});
    QCOMPARE(probe.lastPos, QPoint(7, 9));
    QCOMPARE(probe.lastModifiers, Qt::KeyboardModifiers(Qt::ControlModifier));
    QCOMPARE(probe.lastButtons, Qt::MouseButtons(Qt::LeftButton));
    QTest::mouseRelease(&probe, Qt::LeftButton, Qt::NoModifier, QPoint(7, 9));
    QCOMPARE(probe.lastButtons, Qt::MouseButtons());

    QTest::mouseClick(&probe, Qt::RightButton);
    QCOMPARE(probe.lastPos, QPoint(59, 39)); // the center, as QRect::center() has it

    // QPoint(0, 0) is a null point and so means the center, as in Qt; the corner is reached from (1, 0).
    QTest::mouseClick(&probe, Qt::LeftButton, Qt::NoModifier, QPoint(0, 0));
    QCOMPARE(probe.lastPos, QPoint(59, 39));
    QTest::mouseClick(&probe, Qt::LeftButton, Qt::NoModifier, QPoint(1, 0));
    QCOMPARE(probe.lastPos, QPoint(1, 0));

    // Through the window: the position is in the window, and the widget sees it in its own coordinates.
    QTest::mouseClick(top.windowHandle(), Qt::LeftButton, Qt::NoModifier, QPoint(110, 60));
    QCOMPARE(probe.lastPos, QPoint(10, 10));
    QCOMPARE(probe.lastGlobal, QPoint(110, 60) + top.pos());
}

void TestQtestWidgets::mouseButtonsAndMove() {
    Probe probe;
    probe.resize(200, 100);
    probe.show();
    QVERIFY(QTest::qWaitForWindowExposed(probe.windowHandle()));

    QTest::mousePress(&probe, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
    QTest::mouseMove(&probe, QPoint(50, 60));
    QCOMPARE(probe.log.last(), QString::fromLatin1("move"));
    QCOMPARE(probe.lastPos, QPoint(50, 60));
    QCOMPARE(probe.lastButtons, Qt::MouseButtons(Qt::LeftButton)); // still held from the press
    QTest::mouseRelease(&probe, Qt::LeftButton, Qt::NoModifier, QPoint(50, 60));
    QCOMPARE(probe.lastButtons, Qt::MouseButtons());
    QTest::mouseMove(&probe);
    QCOMPARE(probe.lastPos, QPoint(99, 49));
    QCOMPARE(probe.lastButtons, Qt::MouseButtons());
}

void TestQtestWidgets::doubleClick() {
    Probe probe;
    probe.resize(200, 100);
    probe.show();
    QVERIFY(QTest::qWaitForWindowExposed(probe.windowHandle()));

    QTest::mouseDClick(&probe, Qt::LeftButton, Qt::NoModifier, QPoint(20, 20));
    const QStringList expected{QString::fromLatin1("press"), QString::fromLatin1("release"),
                               QString::fromLatin1("dblclick"), QString::fromLatin1("release")};
    QCOMPARE(probe.log, expected);
    QCOMPARE(probe.lastPos, QPoint(20, 20));
}

void TestQtestWidgets::typeInLineEdit() {
    QWidget top;
    top.resize(300, 100);
    QLineEdit edit(&top);
    edit.setGeometry(10, 10, 200, 30);
    QCheckBox box(QString::fromLatin1("Check"), &top);
    box.setGeometry(10, 50, 200, 30);
    top.show();
    QVERIFY(QTest::qWaitForWindowActive(top.windowHandle()));

    edit.setFocus();
    QVERIFY(edit.hasFocus());
    QTest::keyClicks(&edit, QString::fromLatin1("Hello, Nt 6!"));
    QCOMPARE(edit.text(), QString::fromLatin1("Hello, Nt 6!"));
    QTest::keyClick(&edit, Qt::Key_Backspace);
    QCOMPARE(edit.text(), QString::fromLatin1("Hello, Nt 6"));
    QTest::keyClick(&edit, '?');
    QCOMPARE(edit.text(), QString::fromLatin1("Hello, Nt 6?"));

    // Keys go to the widget with the focus, whichever widget of the window is named.
    QTest::keyClick(&top, 'x');
    QCOMPARE(edit.text(), QString::fromLatin1("Hello, Nt 6?x"));

    box.setFocus();
    QVERIFY(box.hasFocus());
    QVERIFY(!box.isChecked());
    QTest::keyClick(&box, Qt::Key_Space);
    QVERIFY(box.isChecked());
    QTest::keyClick(box.window()->windowHandle(), ' ');
    QVERIFY(!box.isChecked());
}

void TestQtestWidgets::keyWithModifiers() {
    Probe probe;
    probe.resize(200, 100);
    probe.show();
    QVERIFY(QTest::qWaitForWindowActive(probe.windowHandle()));
    probe.setFocus();
    QVERIFY(probe.hasFocus());

    // The modifier keys go down before the key and come up after it, and a letter with Shift types upper case.
    QTest::keyClick(&probe, Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier);
    const QStringList expected{
        QString::fromLatin1("down 1000020"), QString::fromLatin1("down 1000021"), QString::fromLatin1("down 41 'A'"),
        QString::fromLatin1("up 41 'A'"),    QString::fromLatin1("up 1000021"),   QString::fromLatin1("up 1000020"),
    };
    QCOMPARE(probe.log, expected);
    QCOMPARE(probe.lastModifiers, Qt::KeyboardModifiers());

    probe.log.clear();
    QTest::keyPress(&probe, Qt::Key_B);
    QCOMPARE(probe.log, QStringList{QString::fromLatin1("down 42 'b'")});
    QTest::keyRelease(&probe, Qt::Key_B);
    QCOMPARE(probe.log.last(), QString::fromLatin1("up 42 'b'"));

    // An upper-case character adds Shift; Return and Tab type their control characters.
    probe.log.clear();
    QTest::keyPress(&probe, 'Q');
    QCOMPARE(probe.log, (QStringList{QString::fromLatin1("down 1000020"), QString::fromLatin1("down 51 'Q'")}));
    QCOMPARE(probe.lastModifiers, Qt::KeyboardModifiers(Qt::ShiftModifier));
    QTest::keyRelease(&probe, 'Q');
    probe.log.clear();
    QTest::keyClick(&probe, Qt::Key_Return);
    QCOMPARE(probe.log.first(), QString::fromLatin1("down 1000004 '\r'"));
    QTest::keyClick(&probe, '\t');
    QCOMPARE(probe.log.last(), QString::fromLatin1("up 1000001 '\t'"));
    QTest::keyClick(&probe, Qt::Key_Escape);
    QCOMPARE(probe.log.last(), QString::fromLatin1("up 1000000 '\x1b'"));
}

void TestQtestWidgets::keySequenceClicks() {
    Probe probe;
    probe.resize(200, 100);
    probe.show();
    QVERIFY(QTest::qWaitForWindowActive(probe.windowHandle()));
    probe.setFocus();

    QTest::keySequence(&probe, QKeySequence(QString::fromLatin1("Ctrl+S")));
    QCOMPARE(probe.log.size(), 4);
    QCOMPARE(probe.log.at(1), QString::fromLatin1("down 53 's'"));
    probe.log.clear();
    QTest::keySequence(probe.windowHandle(), QKeySequence(Qt::Key_F5));
    QCOMPARE(probe.log, (QStringList{QString::fromLatin1("down 1000034"), QString::fromLatin1("up 1000034")}));
}

void TestQtestWidgets::wheel() {
    QWidget top;
    top.resize(300, 200);
    QSpinBox spin(&top);
    spin.setGeometry(10, 10, 100, 30);
    spin.setRange(0, 10);
    spin.setValue(5);
    Probe probe(&top);
    probe.setGeometry(10, 60, 100, 100);
    top.show();
    QVERIFY(QTest::qWaitForWindowExposed(top.windowHandle()));

    QTest::wheelEvent(&spin, QPoint(), QPoint(0, 120));
    QCOMPARE(spin.value(), 6);
    QTest::wheelEvent(top.windowHandle(), QPointF(50, 20), QPoint(0, -240));
    QCOMPARE(spin.value(), 4);

    QTest::wheelEvent(&probe, QPoint(5, 5), QPoint(0, 120), QPoint(0, 30), Qt::ShiftModifier);
    QCOMPARE(probe.log, QStringList{QString::fromLatin1("wheel")});
    QCOMPARE(probe.lastWheel, QPoint(0, 120));
    QCOMPARE(probe.lastModifiers, Qt::KeyboardModifiers(Qt::ShiftModifier));
}

void TestQtestWidgets::defaults() {
    QCOMPARE(QTest::defaultMouseDelay(), 0);
    QCOMPARE(QTest::defaultKeyDelay(), 0);
    QVERIFY(qApp != nullptr); // QTEST_MAIN made a QApplication, as QT_WIDGETS_LIB is defined
    QVERIFY(QApplication::instance() != nullptr);
}

void TestQtestWidgets::nullTargets() {
    QTest::ignoreMessage(QtWarningMsg, "QTest::mouseEvent(): the widget is null");
    QTest::mouseClick(static_cast<QWidget *>(nullptr), Qt::LeftButton);
    QTest::ignoreMessage(QtWarningMsg, "QTest::mouseEvent(): the window is null");
    QTest::mouseClick(static_cast<QWindow *>(nullptr), Qt::LeftButton);
    QTest::ignoreMessage(QtWarningMsg, "QTest::keyClicks(): the widget is null");
    QTest::keyClicks(static_cast<QWidget *>(nullptr), QString::fromLatin1("x"));
    QTest::ignoreMessage(QtWarningMsg, "QTest::keyEvent(): the window is null");
    QTest::keyClick(static_cast<QWindow *>(nullptr), Qt::Key_A);
    QTest::ignoreMessage(QtWarningMsg, "QTest::wheelEvent(): the widget is null");
    QTest::wheelEvent(static_cast<QWidget *>(nullptr), QPoint(), QPoint(0, 120));

    QWidget unshown;
    QTest::ignoreMessage(QtWarningMsg, "QTest::mouseEvent(): the widget has no window; show it first");
    QTest::mouseClick(&unshown, Qt::LeftButton);
}

QTEST_MAIN(TestQtestWidgets)

#include "test_qtest_widgets.moc"
