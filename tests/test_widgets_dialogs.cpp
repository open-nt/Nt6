// MIT License - see LICENSE
//
// Tests for windows with a parent, modality and the dialogs of Nt Widgets on the offscreen platform: QDialog,
// QDialogButtonBox, QMessageBox, QInputDialog, QProgressDialog and QErrorMessage.
#include <QtCore>
#include <QtGui/qevent.h>
#include <QtGui/qstylehints.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qcheckbox.h>
#include <QtWidgets/qcolordialog.h>
#include <QtWidgets/qfiledialog.h>
#include <QtWidgets/qfontdialog.h>
#include <QtWidgets/qlistwidget.h>
#include <QtWidgets/qwizard.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qdialog.h>
#include <QtWidgets/qdialogbuttonbox.h>
#include <QtWidgets/qerrormessage.h>
#include <QtWidgets/qinputdialog.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qplaintextedit.h>
#include <QtWidgets/qprogressdialog.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qspinbox.h>

#include "qpa/qwindowsysteminterface_p.h"

#include <cstdio>
#include <cstdlib>
#include <functional>

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

void click(QWidget *w, QPoint local) {
    QWindow *win = w->window()->windowHandle();
    const QPoint inWindow = w->mapTo(w->window(), local);
    const QPointF global(inWindow + w->window()->pos());
    QWindowSystemInterface::handleMouseEvent(win, QPointF(inWindow), global, Qt::LeftButton, Qt::LeftButton,
                                             QEvent::MouseButtonPress, Qt::NoModifier);
    QWindowSystemInterface::handleMouseEvent(win, QPointF(inWindow), global, Qt::NoButton, Qt::LeftButton,
                                             QEvent::MouseButtonRelease, Qt::NoModifier);
}

void key(QWidget *w, int k, const QString &text = QString()) {
    QWindow *win = w->window()->windowHandle();
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyPress, k, Qt::NoModifier, text);
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyRelease, k, Qt::NoModifier);
}

void type(QWidget *w, const QString &text) {
    for (QChar c : text)
        key(w, c.unicode() >= 'a' && c.unicode() <= 'z' ? c.unicode() - 32 : c.unicode(), QString(c));
}

// Runs `action` once the event loop of a dialog is running.
void later(std::function<void()> action) {
    QTimer::singleShot(0, [action] { action(); });
}

// The modal window that was shown last, as the type T.
template <typename T> T *activeModal() { return dynamic_cast<T *>(QApplication::activeModalWidget()); }

void testWindowsWithParents() {
    QWidget main;
    main.setGeometry(100, 100, 400, 300);
    auto *button = new QPushButton("Main", &main);
    button->setGeometry(10, 10, 80, 30);
    int clicks = 0;
    QObject::connect(button, &QPushButton::clicked, [&] { ++clicks; });
    main.show();
    settle(&main);

    // A widget with Qt::Window and a parent is a window of its own
    auto *tool = new QWidget(&main, Qt::Window);
    CHECK(tool->isWindow() && tool->parentWidget() == &main && tool->window() == tool);
    CHECK(tool->windowType() == Qt::Window && tool->windowFlags() == Qt::WindowFlags(Qt::Window));
    CHECK(!tool->isVisible()); // not shown with its parent
    tool->resize(120, 80);
    tool->show();
    settle(tool);
    CHECK(tool->isVisible() && tool->windowHandle() != nullptr && tool->windowHandle() != main.windowHandle());
    CHECK(tool->windowHandle()->transientParent() == main.windowHandle());
    CHECK(!QApplication::topLevelWidgets().isEmpty() && QApplication::topLevelWidgets().contains(tool));

    // A child of the window is in the window, not in the parent of the window
    auto *inner = new QLabel("inner", tool);
    inner->move(5, 6);
    CHECK(inner->window() == tool && inner->mapTo(tool, QPoint(0, 0)) == QPoint(5, 6));
    CHECK(main.childAt(QPoint(6, 7)) != inner);

    // The window keeps its own palette, font and enabled state
    main.setEnabled(false);
    CHECK(!button->isEnabled() && tool->isEnabled() && inner->isEnabled());
    main.setEnabled(true);

    // Hiding the parent leaves the window
    main.hide();
    CHECK(tool->isVisible());
    main.show();
    settle(&main);

    // setParent() without flags drops the kind of window; with flags it keeps it
    tool->setParent(&main);
    CHECK(!tool->isWindow() && tool->windowType() == Qt::Widget);
    tool->setParent(&main, Qt::Tool);
    CHECK(tool->isWindow() && tool->windowType() == Qt::Tool);
    tool->setWindowFlags(Qt::Widget);
    CHECK(!tool->isWindow());
    tool->setWindowFlags(Qt::Window);
    CHECK(tool->isWindow());

    // The window goes with its parent
    QPointer<QWidget> guard(tool);
    {
        auto *other = new QWidget;
        auto *owned = new QWidget(other, Qt::Window);
        QPointer<QWidget> ownedGuard(owned);
        owned->show();
        delete other;
        CHECK(ownedGuard.isNull());
    }

    // Modality: an application-modal window blocks the input of the others
    auto *modal = new QWidget(&main, Qt::Dialog);
    modal->setWindowModality(Qt::ApplicationModal);
    CHECK(modal->isModal() && modal->windowModality() == Qt::ApplicationModal);
    CHECK(QApplication::activeModalWidget() == nullptr);
    modal->show();
    settle(modal);
    CHECK(QApplication::activeModalWidget() == modal);
    click(button, QPoint(5, 5));
    CHECK(clicks == 0);
    modal->hide();
    CHECK(QApplication::activeModalWidget() == nullptr);
    click(button, QPoint(5, 5));
    CHECK(clicks == 1);

    // A window-modal window blocks only the windows it belongs to
    QWidget unrelated;
    auto *other = new QPushButton("Other", &unrelated);
    other->setGeometry(0, 0, 60, 30);
    int otherClicks = 0;
    QObject::connect(other, &QPushButton::clicked, [&] { ++otherClicks; });
    unrelated.show();
    settle(&unrelated);
    modal->setWindowModality(Qt::WindowModal);
    modal->show();
    settle(modal);
    click(button, QPoint(5, 5));
    click(other, QPoint(5, 5));
    CHECK(clicks == 1 && otherClicks == 1);
    // ... and a window of its own is not blocked
    auto *ownButton = new QPushButton("Own", modal);
    ownButton->setGeometry(0, 0, 60, 30);
    ownButton->show();
    int ownClicks = 0;
    QObject::connect(ownButton, &QPushButton::clicked, [&] { ++ownClicks; });
    click(ownButton, QPoint(5, 5));
    CHECK(ownClicks == 1);
    // A blocked window cannot be closed
    CHECK(!main.windowHandle()->close() && main.isVisible());
    delete modal; // deleting a modal window unblocks
    CHECK(QApplication::activeModalWidget() == nullptr);
    click(button, QPoint(5, 5));
    CHECK(clicks == 2);
}

void testDialog() {
    QWidget main;
    main.setGeometry(200, 150, 400, 300);
    main.show();
    settle(&main);

    QDialog dialog(&main);
    CHECK(dialog.isWindow() && dialog.windowType() == Qt::Dialog && !dialog.isModal() && dialog.result() == 0);
    auto *layout = new QVBoxLayout(&dialog);
    auto *edit = new QLineEdit(&dialog);
    auto *ok = new QPushButton("OK", &dialog);
    auto *cancel = new QPushButton("Cancel", &dialog);
    layout->addWidget(edit);
    layout->addWidget(ok);
    layout->addWidget(cancel);
    QObject::connect(ok, &QPushButton::clicked, &dialog, &QDialog::accept);
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    CHECK(ok->autoDefault() && !ok->isDefault());

    QStringList log;
    QObject::connect(&dialog, &QDialog::accepted, [&] { log << "accepted"; });
    QObject::connect(&dialog, &QDialog::rejected, [&] { log << "rejected"; });
    QObject::connect(&dialog, &QDialog::finished, [&](int r) { log << QString("finished %1").arg(r); });

    // exec() is modal and returns the result; the dialog is centered over its parent
    later([&] {
        CHECK(dialog.isVisible() && dialog.windowModality() == Qt::ApplicationModal);
        CHECK(QApplication::activeModalWidget() == &dialog);
        const QPoint center = dialog.geometry().center();
        CHECK(std::abs(center.x() - main.geometry().center().x()) <= 1 &&
              std::abs(center.y() - main.geometry().center().y()) <= 1);
        click(ok, QPoint(5, 5));
    });
    CHECK(dialog.exec() == QDialog::Accepted);
    CHECK(!dialog.isVisible() && dialog.result() == QDialog::Accepted && !dialog.isModal());
    CHECK(log == QStringList({"accepted", "finished 1"}));

    // Escape rejects; Return clicks the default button, even from a line edit
    log.clear();
    ok->setDefault(true);
    CHECK(ok->isDefault());
    cancel->setDefault(true);
    CHECK(!ok->isDefault() && cancel->isDefault());
    ok->setDefault(true);
    later([&] {
        edit->setFocus();
        type(edit, "x");
        key(edit, Qt::Key_Return);
    });
    CHECK(dialog.exec() == QDialog::Accepted && edit->text() == QString("x"));
    later([&] { key(&dialog, Qt::Key_Escape); });
    CHECK(dialog.exec() == QDialog::Rejected && log.contains("rejected"));

    // Closing the window rejects
    later([&] { dialog.windowHandle()->close(); });
    CHECK(dialog.exec() == QDialog::Rejected);

    // done() with any code
    later([&] { dialog.done(7); });
    CHECK(dialog.exec() == 7 && dialog.result() == 7);

    // open() is window-modal, returns at once, and resets the modality when done
    log.clear();
    dialog.open();
    CHECK(dialog.isVisible() && dialog.windowModality() == Qt::WindowModal);
    dialog.reject();
    CHECK(!dialog.isVisible() && dialog.windowModality() == Qt::NonModal && log.last() == "finished 0");

    // setModal() and show()
    dialog.setModal(true);
    dialog.show();
    CHECK(QApplication::activeModalWidget() == &dialog);
    dialog.accept();
    dialog.setModal(false);

    // A dialog deleted while exec() runs makes it return Rejected
    auto *doomed = new QDialog(&main);
    later([doomed] { delete doomed; });
    CHECK(doomed->exec() == QDialog::Rejected);
}

void testButtonBox() {
    QDialogButtonBox box(QDialogButtonBox::Cancel | QDialogButtonBox::Ok | QDialogButtonBox::Help |
                         QDialogButtonBox::Apply);
    box.resize(500, 40);
    CHECK(box.orientation() == Qt::Horizontal && box.buttons().size() == 4 && !box.centerButtons());
    CHECK(box.standardButtons() ==
          (QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Help | QDialogButtonBox::Apply));
    QPushButton *ok = box.button(QDialogButtonBox::Ok);
    QPushButton *cancel = box.button(QDialogButtonBox::Cancel);
    QPushButton *help = box.button(QDialogButtonBox::Help);
    QPushButton *apply = box.button(QDialogButtonBox::Apply);
    CHECK(ok && ok->text() == QString("OK") && cancel->text() == QString("Cancel"));
    CHECK(box.buttonRole(ok) == QDialogButtonBox::AcceptRole &&
          box.buttonRole(cancel) == QDialogButtonBox::RejectRole &&
          box.buttonRole(help) == QDialogButtonBox::HelpRole && box.buttonRole(apply) == QDialogButtonBox::ApplyRole);
    CHECK(box.standardButton(ok) == QDialogButtonBox::Ok && box.button(QDialogButtonBox::Save) == nullptr);

    // The order of the platform: help at the start, then OK, Apply and Cancel at the end
    box.show();
    settle(&box);
    CHECK(help->x() < ok->x() && ok->x() < apply->x() && apply->x() < cancel->x());
    CHECK(ok->x() - help->x() > 100); // the stretch
    box.setCenterButtons(true);
    settle(&box);
    CHECK(box.centerButtons() && help->x() > 50);
    box.setCenterButtons(false);

    // Signals by role
    QStringList log;
    QObject::connect(&box, &QDialogButtonBox::clicked, [&](QAbstractButton *b) { log << "clicked " + b->text(); });
    QObject::connect(&box, &QDialogButtonBox::accepted, [&] { log << "accepted"; });
    QObject::connect(&box, &QDialogButtonBox::rejected, [&] { log << "rejected"; });
    QObject::connect(&box, &QDialogButtonBox::helpRequested, [&] { log << "help"; });
    ok->click();
    cancel->click();
    help->click();
    apply->click();
    CHECK(log == QStringList({"clicked OK", "accepted", "clicked Cancel", "rejected", "clicked Help", "help",
                              "clicked Apply"}));

    // Custom buttons
    QPushButton *more = box.addButton("More", QDialogButtonBox::ActionRole);
    CHECK(more && more->parentWidget() == &box && box.buttonRole(more) == QDialogButtonBox::ActionRole);
    CHECK(box.addButton("Bad", QDialogButtonBox::InvalidRole) == nullptr);
    auto *mine = new QPushButton("Mine");
    box.addButton(mine, QDialogButtonBox::YesRole);
    CHECK(mine->parentWidget() == &box && box.standardButton(mine) == QDialogButtonBox::NoButton);
    box.removeButton(mine);
    CHECK(mine->parentWidget() == nullptr && box.buttonRole(mine) == QDialogButtonBox::InvalidRole);
    delete mine;
    delete more; // a deleted button leaves the box
    CHECK(box.buttons().size() == 4);
    CHECK(box.addButton(QDialogButtonBox::Ok) == ok);

    // setStandardButtons() replaces the standard buttons
    box.setStandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::No);
    CHECK(box.buttons().size() == 2 && box.button(QDialogButtonBox::Ok) == nullptr &&
          box.buttonRole(box.button(QDialogButtonBox::No)) == QDialogButtonBox::NoRole);
    box.clear();
    CHECK(box.buttons().isEmpty() && box.standardButtons() == QDialogButtonBox::StandardButtons());

    // Vertical
    box.setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    box.setOrientation(Qt::Vertical);
    box.resize(80, 200);
    settle(&box);
    CHECK(box.orientation() == Qt::Vertical &&
          box.button(QDialogButtonBox::Ok)->y() < box.button(QDialogButtonBox::Cancel)->y());

    // In a dialog the first accepting button becomes the default when shown
    QDialog dialog;
    auto *layout = new QVBoxLayout(&dialog);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    later([&] {
        CHECK(buttons->button(QDialogButtonBox::Save)->isDefault());
        key(&dialog, Qt::Key_Return);
    });
    CHECK(dialog.exec() == QDialog::Accepted);
}

void testMessageBox() {
    QWidget main;
    main.setGeometry(0, 0, 400, 300);
    main.show();
    settle(&main);

    // The static functions return the button that was clicked
    later([] {
        auto *box = activeModal<QMessageBox>();
        CHECK(box && box->icon() == QMessageBox::Question && box->text() == QString("Quit?") &&
              box->windowTitle() == QString("Title"));
        CHECK(box && box->standardButtons() == (QMessageBox::Yes | QMessageBox::No));
        if (box)
            box->button(QMessageBox::Yes)->click();
    });
    CHECK(QMessageBox::question(&main, "Title", "Quit?") == QMessageBox::Yes);
    later([] {
        if (auto *box = activeModal<QMessageBox>())
            key(box, Qt::Key_Escape);
    });
    CHECK(QMessageBox::warning(&main, "W", "Careful", QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel);
    later([] {
        if (auto *box = activeModal<QMessageBox>()) {
            CHECK(box->icon() == QMessageBox::Critical && box->defaultButton() == box->button(QMessageBox::Retry));
            key(box, Qt::Key_Return);
        }
    });
    CHECK(QMessageBox::critical(&main, "C", "Broken", QMessageBox::Retry | QMessageBox::Abort, QMessageBox::Retry) ==
          QMessageBox::Retry);
    later([] {
        if (auto *box = activeModal<QMessageBox>()) {
            CHECK(box->icon() == QMessageBox::Information);
            box->windowHandle()->close(); // the only button is the escape button
        }
    });
    CHECK(QMessageBox::information(&main, "I", "Done") == QMessageBox::Ok);
    later([] {
        if (auto *box = activeModal<QMessageBox>())
            box->button(QMessageBox::Ok)->click();
    });
    QMessageBox::about(&main, "About", "Nt");

    // A message box set up by hand
    QMessageBox box(QMessageBox::Warning, "Save", "Modified.",
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, &main);
    box.setInformativeText("Save the changes?");
    box.setDefaultButton(QMessageBox::Save);
    CHECK(box.informativeText() == QString("Save the changes?") && box.text() == QString("Modified."));
    CHECK(box.buttonRole(box.button(QMessageBox::Discard)) == QMessageBox::DestructiveRole);
    QStringList log;
    QObject::connect(&box, &QMessageBox::buttonClicked, [&](QAbstractButton *b) { log << b->text(); });
    QObject::connect(&box, &QDialog::rejected, [&] { log << "rejected"; });
    later([&] { box.button(QMessageBox::Discard)->click(); });
    CHECK(box.exec() == QMessageBox::Discard && box.clickedButton() == box.button(QMessageBox::Discard));
    CHECK(log == QStringList({"Discard"}));
    later([&] { key(&box, Qt::Key_Escape); }); // Cancel has the RejectRole
    CHECK(box.exec() == QMessageBox::Cancel && log.last() == "rejected");

    // Custom buttons, the escape button, the check box and the details
    QMessageBox custom(&main);
    custom.setText("Custom");
    QPushButton *first = custom.addButton("First", QMessageBox::AcceptRole);
    QPushButton *second = custom.addButton("Second", QMessageBox::ActionRole);
    custom.setEscapeButton(second);
    CHECK(custom.escapeButton() == second && custom.buttons().size() == 2);
    auto *again = new QCheckBox("Do not ask again");
    custom.setCheckBox(again);
    CHECK(custom.checkBox() == again && again->parentWidget() == &custom);
    custom.setDetailedText("Line 1\nLine 2");
    CHECK(custom.detailedText() == QString("Line 1\nLine 2") && custom.buttons().size() == 2);
    later([&] { key(&custom, Qt::Key_Escape); });
    CHECK(custom.exec() == 1 && custom.clickedButton() == second); // the index of a custom button
    later([&] { first->click(); });
    CHECK(custom.exec() == 0);
    custom.setDetailedText(QString());
    CHECK(custom.detailedText().isEmpty());
    custom.removeButton(second);
    CHECK(custom.escapeButton() == nullptr && custom.buttons().size() == 1);
    delete second;

    // Without an escape button the window cannot be closed
    QMessageBox noEscape(&main);
    noEscape.addButton("A", QMessageBox::AcceptRole);
    noEscape.addButton("B", QMessageBox::ActionRole);
    noEscape.show();
    settle(&noEscape);
    CHECK(!noEscape.windowHandle()->close() && noEscape.isVisible());
    key(&noEscape, Qt::Key_Escape);
    CHECK(noEscape.isVisible());
    noEscape.hide();

    // A box without buttons gets OK
    QMessageBox empty(&main);
    empty.show();
    settle(&empty);
    CHECK(empty.buttons().size() == 1 && empty.standardButton(empty.buttons().first()) == QMessageBox::Ok);
    empty.hide();
}

void testInputDialog() {
    QWidget main;
    main.show();
    settle(&main);

    // getText
    later([] {
        auto *dialog = activeModal<QInputDialog>();
        CHECK(dialog && dialog->labelText() == QString("Name:") && dialog->textValue() == QString("old") &&
              dialog->textEchoMode() == QLineEdit::Normal);
        if (!dialog)
            return;
        QLineEdit *edit = dialog->findChild<QLineEdit *>();
        edit->setFocus();
        edit->selectAll();
        type(edit, "new");
        key(edit, Qt::Key_Return);
    });
    bool ok = false;
    CHECK(QInputDialog::getText(&main, "T", "Name:", QLineEdit::Normal, "old", &ok) == QString("new") && ok);
    later([] {
        if (auto *dialog = activeModal<QInputDialog>())
            key(dialog, Qt::Key_Escape);
    });
    CHECK(QInputDialog::getText(&main, "T", "Name:", QLineEdit::Password, "old", &ok).isEmpty() && !ok);

    // getInt and getDouble
    later([] {
        if (auto *dialog = activeModal<QInputDialog>()) {
            CHECK(dialog->inputMode() == QInputDialog::IntInput && dialog->intMinimum() == 0 &&
                  dialog->intMaximum() == 10 && dialog->intStep() == 2 && dialog->intValue() == 4);
            dialog->setIntValue(8);
            dialog->accept();
        }
    });
    CHECK(QInputDialog::getInt(&main, "T", "Count:", 4, 0, 10, 2, &ok) == 8 && ok);
    later([] {
        if (auto *dialog = activeModal<QInputDialog>()) {
            CHECK(dialog->inputMode() == QInputDialog::DoubleInput && dialog->doubleDecimals() == 3 &&
                  dialog->doubleValue() == 1.5);
            dialog->setDoubleValue(2.25);
            dialog->accept();
        }
    });
    CHECK(QInputDialog::getDouble(&main, "T", "Gain:", 1.5, 0, 5, 3, &ok) == 2.25 && ok);
    later([] {
        if (auto *dialog = activeModal<QInputDialog>())
            dialog->reject();
    });
    CHECK(QInputDialog::getInt(&main, "T", "Count:", 4, 0, 10, 1, &ok) == 4 && !ok);

    // getItem and getMultiLineText
    later([] {
        if (auto *dialog = activeModal<QInputDialog>()) {
            CHECK(dialog->comboBoxItems() == QStringList({"a", "b", "c"}) && dialog->textValue() == QString("b"));
            dialog->findChild<QComboBox *>()->setCurrentIndex(2);
            dialog->accept();
        }
    });
    CHECK(QInputDialog::getItem(&main, "T", "Pick:", {"a", "b", "c"}, 1, false, &ok) == QString("c") && ok);
    later([] {
        if (auto *dialog = activeModal<QInputDialog>()) {
            CHECK(dialog->testOption(QInputDialog::UsePlainTextEditForTextInput));
            dialog->findChild<QPlainTextEdit *>()->setPlainText("one\ntwo");
            dialog->accept();
        }
    });
    CHECK(QInputDialog::getMultiLineText(&main, "T", "Text:", "x", &ok) == QString("one\ntwo") && ok);

    // Signals and options of a dialog set up by hand
    QInputDialog dialog(&main);
    QList<int> ints;
    QList<int> selected;
    QObject::connect(&dialog, &QInputDialog::intValueChanged, [&](int v) { ints << v; });
    QObject::connect(&dialog, &QInputDialog::intValueSelected, [&](int v) { selected << v; });
    dialog.setInputMode(QInputDialog::IntInput);
    dialog.setIntRange(0, 100);
    dialog.setIntValue(42);
    CHECK(ints == QList<int>({42}));
    dialog.setOkButtonText("Go");
    dialog.setCancelButtonText("Stop");
    CHECK(dialog.okButtonText() == QString("Go") && dialog.cancelButtonText() == QString("Stop"));
    dialog.setOption(QInputDialog::NoButtons);
    CHECK(dialog.testOption(QInputDialog::NoButtons) && dialog.options() == QInputDialog::NoButtons);
    dialog.open();
    dialog.accept();
    CHECK(selected == QList<int>({42}));
    QStringList texts;
    QObject::connect(&dialog, &QInputDialog::textValueChanged, [&](const QString &t) { texts << t; });
    dialog.setInputMode(QInputDialog::TextInput);
    dialog.setTextValue("abc");
    CHECK(texts == QStringList({"abc"}) && dialog.textValue() == QString("abc"));
}

void testProgressDialog() {
    QWidget main;
    main.show();
    settle(&main);

    QProgressDialog progress("Working", "Abort", 0, 10, &main);
    CHECK(progress.minimum() == 0 && progress.maximum() == 10 && progress.value() == -1 &&
          progress.minimumDuration() == 4000 && progress.autoReset() && progress.autoClose() &&
          !progress.wasCanceled() && progress.labelText() == QString("Working"));
    // Short tasks do not show the dialog
    progress.setValue(0);
    progress.setValue(5);
    CHECK(!progress.isVisible() && progress.value() == 5);
    progress.setValue(10); // the end resets
    CHECK(progress.value() == -1 && !progress.isVisible());

    // With no minimum duration the dialog shows at once
    progress.setMinimumDuration(0);
    progress.setValue(0);
    progress.setValue(1);
    CHECK(progress.isVisible());
    int canceled = 0;
    QObject::connect(&progress, &QProgressDialog::canceled, [&] { ++canceled; });
    progress.findChild<QPushButton *>()->click();
    CHECK(progress.wasCanceled() && canceled == 1 && !progress.isVisible());
    progress.reset();
    CHECK(!progress.wasCanceled());

    // Escape cancels; without auto reset the end keeps the value
    progress.setValue(0);
    progress.setValue(1);
    key(&progress, Qt::Key_Escape);
    CHECK(progress.wasCanceled() && canceled == 2);
    progress.reset();
    progress.setAutoReset(false);
    progress.setValue(0);
    progress.setValue(10);
    CHECK(progress.value() == 10 && progress.isVisible());
    progress.setLabelText("Done");
    progress.setCancelButtonText(QString());
    CHECK(progress.labelText() == QString("Done") && progress.findChild<QPushButton *>()->isHidden());
    progress.setRange(5, 20);
    CHECK(progress.minimum() == 5 && progress.maximum() == 20);
    progress.cancel();
    CHECK(progress.wasCanceled() && !progress.isVisible());
}

void testErrorMessage() {
    QWidget main;
    main.show();
    settle(&main);
    QErrorMessage errors(&main);
    QLabel *label = nullptr;
    for (QLabel *l : errors.findChildren<QLabel *>())
        label = l;
    QCheckBox *again = errors.findChild<QCheckBox *>();
    CHECK(label && again);

    errors.showMessage("First");
    CHECK(errors.isVisible() && label->text() == QString("First") && again->isChecked());
    errors.showMessage("Second");
    errors.showMessage("Second"); // not queued twice
    errors.accept();
    CHECK(errors.isVisible() && label->text() == QString("Second"));
    again->setChecked(false);
    errors.accept();
    CHECK(!errors.isVisible());
    errors.showMessage("Second"); // the user asked not to see it again
    CHECK(!errors.isVisible());
    errors.showMessage("First");
    CHECK(errors.isVisible());
    errors.accept();

    // A type covers all its messages
    errors.showMessage("Disk A full", "disk");
    again->setChecked(false);
    errors.accept();
    errors.showMessage("Disk B full", "disk");
    CHECK(!errors.isVisible());
}

void testColorDialog() {
    QWidget main;
    main.show();
    settle(&main);

    QColorDialog dialog(QColor(255, 0, 0), &main);
    CHECK(dialog.currentColor() == QColor(255, 0, 0) && !dialog.selectedColor().isValid());
    QList<QColor> changes;
    QObject::connect(&dialog, &QColorDialog::currentColorChanged, [&](const QColor &c) { changes << c; });
    dialog.setCurrentColor(QColor(0, 128, 255));
    CHECK(changes.size() == 1 && dialog.currentColor() == QColor(0, 128, 255));
    // The spin boxes and the HTML text follow the color, and change it
    QLineEdit *html = nullptr;
    for (QLineEdit *e : dialog.findChildren<QLineEdit *>())
        if (!dynamic_cast<QAbstractSpinBox *>(e->parentWidget()))
            html = e;
    CHECK(html && html->text() == QString("#0080ff"));
    html->setFocus();
    html->selectAll();
    dialog.show();
    settle(&dialog);
    type(html, "#102030");
    CHECK(dialog.currentColor() == QColor(0x10, 0x20, 0x30));
    QList<QSpinBox *> spins = dialog.findChildren<QSpinBox *>();
    CHECK(spins.size() == 7);
    // Without the alpha channel the color is opaque
    dialog.setCurrentColor(QColor(1, 2, 3, 100));
    CHECK(dialog.currentColor().alpha() == 255);
    dialog.setOption(QColorDialog::ShowAlphaChannel);
    dialog.setCurrentColor(QColor(1, 2, 3, 100));
    CHECK(dialog.testOption(QColorDialog::ShowAlphaChannel) && dialog.currentColor().alpha() == 100);
    QList<QColor> selected;
    QObject::connect(&dialog, &QColorDialog::colorSelected, [&](const QColor &c) { selected << c; });
    dialog.accept();
    CHECK(dialog.selectedColor() == QColor(1, 2, 3, 100) && selected.size() == 1);

    // Standard and custom colors
    CHECK(QColorDialog::customCount() == 16 && QColorDialog::customColor(0) == QColor(Qt::white));
    QColorDialog::setCustomColor(3, QColor(9, 9, 9));
    CHECK(QColorDialog::customColor(3) == QColor(9, 9, 9) && !QColorDialog::customColor(16).isValid());
    CHECK(QColorDialog::standardColor(0) == QColor(Qt::black) && QColorDialog::standardColor(7) == QColor(Qt::white));
    CHECK(!QColorDialog::standardColor(48).isValid());

    // getColor()
    later([] {
        if (auto *d = activeModal<QColorDialog>()) {
            CHECK(d->windowTitle() == QString("Pick") && d->currentColor() == QColor(Qt::green));
            d->setCurrentColor(QColor(Qt::blue));
            d->accept();
        }
    });
    CHECK(QColorDialog::getColor(Qt::green, &main, "Pick") == QColor(Qt::blue));
    later([] {
        if (auto *d = activeModal<QColorDialog>())
            d->reject();
    });
    CHECK(!QColorDialog::getColor(Qt::green, &main).isValid());
}

void testFontDialog() {
    QWidget main;
    main.show();
    settle(&main);

    QFont initial("Serif", 14);
    initial.setBold(true);
    QFontDialog dialog(initial, &main);
    CHECK(dialog.currentFont() == initial);
    QList<QListWidget *> lists = dialog.findChildren<QListWidget *>();
    CHECK(lists.size() == 3);
    QListWidget *families = lists.value(0);
    QListWidget *styles = lists.value(1);
    CHECK(families && families->currentItem() && families->currentItem()->text() == QString("Serif"));
    CHECK(styles && styles->currentRow() == 2); // Bold
    QList<QFont> changes;
    QObject::connect(&dialog, &QFontDialog::currentFontChanged, [&](const QFont &f) { changes << f; });
    styles->setCurrentRow(3);
    CHECK(dialog.currentFont().bold() && dialog.currentFont().italic() && changes.size() == 1);
    families->setCurrentRow(2);
    CHECK(dialog.currentFont().family() == QString("Monospace"));
    QSpinBox *size = dialog.findChild<QSpinBox *>();
    size->setValue(20);
    CHECK(dialog.currentFont().pointSize() == 20);
    dialog.accept();
    CHECK(dialog.selectedFont() == dialog.currentFont());

    bool ok = false;
    later([] {
        if (auto *d = activeModal<QFontDialog>()) {
            QFont f = d->currentFont();
            f.setPointSize(30);
            d->setCurrentFont(f);
            d->accept();
        }
    });
    CHECK(QFontDialog::getFont(&ok, initial, &main).pointSize() == 30 && ok);
    later([] {
        if (auto *d = activeModal<QFontDialog>())
            d->reject();
    });
    CHECK(QFontDialog::getFont(&ok, initial, &main) == initial && !ok);
    dialog.setOption(QFontDialog::NoButtons);
    CHECK(dialog.testOption(QFontDialog::NoButtons));
}

void testFileDialog() {
    QWidget main;
    main.show();
    settle(&main);
    QTemporaryDir temp;
    const QString root = temp.path();
    QDir(root).mkdir("sub");
    for (const char *name : {"a.txt", "b.png", "c.txt", "sub/d.txt"}) {
        QFile file(root + "/" + name);
        file.open(QIODevice::WriteOnly);
        file.write("x");
    }

    QFileDialog dialog(&main, "Open", root, "Text (*.txt);;Images (*.png);;All (*)");
    QListWidget *list = dialog.findChild<QListWidget *>();
    const auto names = [list] {
        QStringList out;
        for (int i = 0; i < list->count(); ++i)
            out << list->item(i)->text();
        return out;
    };
    CHECK(dialog.directory().absolutePath() == QDir(root).absolutePath() && dialog.windowTitle() == QString("Open"));
    CHECK(dialog.nameFilters().size() == 3 && dialog.selectedNameFilter() == QString("Text (*.txt)"));
    CHECK(names() == QStringList({"sub/", "a.txt", "c.txt"}));
    QStringList filters;
    QObject::connect(&dialog, &QFileDialog::filterSelected, [&](const QString &f) { filters << f; });
    dialog.selectNameFilter("Images (*.png)");
    CHECK(names() == QStringList({"sub/", "b.png"}) && filters == QStringList({"Images (*.png)"}));
    dialog.selectNameFilter("All (*)");

    // Entering directories
    QStringList entered;
    QObject::connect(&dialog, &QFileDialog::directoryEntered, [&](const QString &d) { entered << d; });
    dialog.setDirectory(root + "/sub");
    CHECK(names() == QStringList({"d.txt"}) && entered.size() == 1);
    dialog.setDirectory(root);
    list->setCurrentRow(0);
    emit list->itemActivated(list->item(0)); // the "sub/" item
    CHECK(dialog.directory().dirName() == QString("sub"));
    dialog.setDirectory(root);

    // An existing file
    dialog.setFileMode(QFileDialog::ExistingFile);
    QStringList selected;
    QObject::connect(&dialog, &QFileDialog::fileSelected, [&](const QString &f) { selected << f; });
    dialog.open();
    dialog.selectFile("missing.txt");
    dialog.accept(); // refused
    CHECK(dialog.isVisible() && selected.isEmpty());
    dialog.selectFile("b.png");
    dialog.accept();
    CHECK(!dialog.isVisible() && dialog.result() == QDialog::Accepted &&
          dialog.selectedFiles() == QStringList({QDir(root).absoluteFilePath("b.png")}) && selected.size() == 1);

    // getOpenFileName and getOpenFileNames
    later([] {
        if (auto *d = activeModal<QFileDialog>()) {
            CHECK(d->fileMode() == QFileDialog::ExistingFile && d->acceptMode() == QFileDialog::AcceptOpen);
            d->selectFile("c.txt");
            d->accept();
        }
    });
    QString chosenFilter;
    CHECK(QFileDialog::getOpenFileName(&main, "T", root, "Text (*.txt)", &chosenFilter) ==
          QDir(root).absoluteFilePath("c.txt"));
    CHECK(chosenFilter == QString("Text (*.txt)"));
    later([] {
        if (auto *d = activeModal<QFileDialog>()) {
            QListWidget *l = d->findChild<QListWidget *>();
            CHECK(l->selectionMode() == QAbstractItemView::ExtendedSelection);
            for (int i = 0; i < l->count(); ++i)
                l->item(i)->setSelected(l->item(i)->text().endsWith(".txt"));
            d->accept();
        }
    });
    CHECK(QFileDialog::getOpenFileNames(&main, "T", root, "Text (*.txt)").size() == 2);
    later([] {
        if (auto *d = activeModal<QFileDialog>())
            d->reject();
    });
    CHECK(QFileDialog::getOpenFileName(&main, "T", root).isEmpty());

    // Saving: a default suffix, and replacing a file is confirmed
    later([] {
        if (auto *d = activeModal<QFileDialog>()) {
            CHECK(d->acceptMode() == QFileDialog::AcceptSave);
            d->setDefaultSuffix("txt");
            d->selectFile("new");
            d->accept();
        }
    });
    CHECK(QFileDialog::getSaveFileName(&main, "S", root) == QDir(root).absoluteFilePath("new.txt"));
    later([] {
        if (auto *d = activeModal<QFileDialog>()) {
            later([] {
                if (auto *box = activeModal<QMessageBox>())
                    box->button(QMessageBox::Yes)->click();
            });
            d->selectFile("a.txt");
            d->accept();
        }
    });
    CHECK(QFileDialog::getSaveFileName(&main, "S", root) == QDir(root).absoluteFilePath("a.txt"));

    // A directory
    later([&] {
        if (auto *d = activeModal<QFileDialog>()) {
            CHECK(d->fileMode() == QFileDialog::Directory && d->testOption(QFileDialog::ShowDirsOnly));
            QListWidget *l = d->findChild<QListWidget *>();
            CHECK(l->count() == 1 && l->item(0)->text() == QString("sub/"));
            d->selectFile("sub");
            d->accept();
        }
    });
    CHECK(QFileDialog::getExistingDirectory(&main, "D", root) == QDir(root).absoluteFilePath("sub"));

    // Labels and filter details
    dialog.setLabelText(QFileDialog::Accept, "Load");
    CHECK(dialog.labelText(QFileDialog::Accept) == QString("Load"));
    dialog.setLabelText(QFileDialog::LookIn, "Folder:");
    CHECK(dialog.labelText(QFileDialog::LookIn) == QString("Folder:"));
    dialog.setOption(QFileDialog::HideNameFilterDetails);
    CHECK(dialog.findChild<QComboBox *>()->itemText(0) == QString("Text"));
}

class NamePage : public QWizardPage {
public:
    QLineEdit *edit = new QLineEdit(this);
    int initialized = 0;
    int cleaned = 0;
    bool valid = true;
    NamePage() {
        setTitle("Name");
        setSubTitle("Enter a name");
        registerField("name*", edit);
    }
    void initializePage() override { ++initialized; }
    void cleanupPage() override {
        ++cleaned;
        QWizardPage::cleanupPage();
    }
    bool validatePage() override { return valid; }
};

void testWizard() {
    QWidget main;
    main.show();
    settle(&main);

    QWizard wizard(&main);
    auto *intro = new QWizardPage;
    intro->setTitle("Intro");
    auto *named = new NamePage;
    auto *last = new QWizardPage;
    auto *check = new QCheckBox("Agree", last);
    CHECK(wizard.addPage(intro) == 0 && wizard.addPage(named) == 1);
    wizard.setPage(5, last);
    CHECK(wizard.pageIds() == QList<int>({0, 1, 5}) && wizard.page(5) == last && wizard.startId() == 0);
    CHECK(wizard.currentId() == -1 && wizard.currentPage() == nullptr);
    QList<int> ids;
    QObject::connect(&wizard, &QWizard::currentIdChanged, [&](int id) { ids << id; });

    wizard.show();
    settle(&wizard);
    QAbstractButton *back = wizard.button(QWizard::BackButton);
    QAbstractButton *next = wizard.button(QWizard::NextButton);
    QAbstractButton *finish = wizard.button(QWizard::FinishButton);
    CHECK(wizard.currentId() == 0 && ids == QList<int>({0}) && wizard.visitedIds() == QList<int>({0}));
    CHECK(back->text() == QString("< Back") && next->text() == QString("Next >"));
    CHECK(!back->isEnabled() && next->isEnabled() && finish->isHidden());

    // A mandatory field keeps Next disabled until it is filled
    next->click();
    CHECK(wizard.currentId() == 1 && named->initialized == 1 && back->isEnabled() && !next->isEnabled());
    named->edit->setText("Nt");
    CHECK(next->isEnabled() && wizard.field("name") == QVariant(QString("Nt")));
    // validatePage() can refuse
    named->valid = false;
    next->click();
    CHECK(wizard.currentId() == 1);
    named->valid = true;
    next->click();
    CHECK(wizard.currentId() == 5 && wizard.hasVisitedPage(1) && !finish->isHidden() && next->isHidden());

    // Back cleans up the page that is left; the fields keep their values on the earlier page
    back->click();
    CHECK(wizard.currentId() == 1 && named->edit->text() == QString("Nt"));
    back->click();
    CHECK(wizard.currentId() == 0 && named->cleaned == 1 && named->edit->text().isEmpty());

    // setField() and the commit page
    wizard.setField("name", QString("set"));
    CHECK(named->edit->text() == QString("set"));
    named->setCommitPage(true);
    next->click();
    QAbstractButton *commit = wizard.button(QWizard::CommitButton);
    CHECK(wizard.currentId() == 1 && !commit->isHidden() && next->isHidden() && commit->text() == QString("Commit"));
    commit->click();
    CHECK(wizard.currentId() == 5 && !back->isEnabled()); // no going back past a commit page

    // Button texts and options
    wizard.setButtonText(QWizard::FinishButton, "Done");
    CHECK(finish->text() == QString("Done") && wizard.buttonText(QWizard::FinishButton) == QString("Done"));
    last->setButtonText(QWizard::FinishButton, "Create");
    CHECK(wizard.buttonText(QWizard::FinishButton) == QString("Create"));
    int helps = 0;
    QObject::connect(&wizard, &QWizard::helpRequested, [&] { ++helps; });
    wizard.setOption(QWizard::HaveHelpButton);
    CHECK(!wizard.button(QWizard::HelpButton)->isHidden());
    wizard.button(QWizard::HelpButton)->click();
    CHECK(helps == 1);
    (void)check;

    // Finish accepts
    finish->click();
    CHECK(!wizard.isVisible() && wizard.result() == QDialog::Accepted);

    // restart() goes back to the start; a rejected wizard starts again next time
    named->setCommitPage(false);
    wizard.restart();
    CHECK(wizard.currentId() == 0 && wizard.visitedIds() == QList<int>({0}));
    wizard.setCurrentId(5);
    CHECK(wizard.currentId() == 5 || wizard.currentId() == 1); // the mandatory field may stop it on the way
    later([&] { wizard.reject(); });
    CHECK(wizard.exec() == QDialog::Rejected);
    CHECK(wizard.currentId() == -1);

    // removePage()
    QList<int> removed;
    QObject::connect(&wizard, &QWizard::pageRemoved, [&](int id) { removed << id; });
    wizard.removePage(5);
    CHECK(removed == QList<int>({5}) && wizard.page(5) == nullptr && last->parentWidget() == nullptr);
    delete last;
}
} // namespace

int main(int argc, char **argv) {
    ::setenv("QT_QPA_PLATFORM", "offscreen", 1);
    ::setenv("QT_SCALE_FACTOR", "1", 1);
    QApplication app(argc, argv);
    app.styleHints()->setColorScheme(Qt::ColorScheme::Light);
    app.setQuitOnLastWindowClosed(false);
    testWindowsWithParents();
    testDialog();
    testButtonBox();
    testMessageBox();
    testInputDialog();
    testProgressDialog();
    testErrorMessage();
    testColorDialog();
    testFontDialog();
    testFileDialog();
    testWizard();
    if (g_failures == 0)
        std::puts("all dialog tests passed");
    return g_failures == 0 ? 0 : 1;
}
