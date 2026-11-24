// MIT License - see LICENSE
//
// Child windows and QWidget::createWindowContainer() on the offscreen platform: the parent of a window, its geometry
// relative to the parent, what counts as a top-level window, and a container that keeps its window at its place in
// the top-level window, shows and hides it with itself and deletes it.
#include <QtCore>
#include <QtGui/qguiapplication.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qwidget.h>

#include "platforms/offscreen/qoffscreenintegration_p.h"

#include <cstdio>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace {

const QPlatformWindow *platformParent(const QWindow *window) {
    const auto *platform = static_cast<const QOffscreenWindow *>(window->handle());
    return platform ? platform->parent() : nullptr;
}

void testChildWindows() {
    auto *top = new QWindow;
    top->setGeometry(100, 50, 400, 300);
    QPointer<QWindow> child = new QWindow(top);
    child->setGeometry(10, 20, 30, 40);
    CHECK(child->parent() == top);
    CHECK(static_cast<QObject *>(child.data())->parent() == top);
    CHECK(!child->isTopLevel() && top->isTopLevel());
    CHECK(top->isAncestorOf(child) && !child->isAncestorOf(top));
    CHECK(QGuiApplication::allWindows().contains(child.data()));
    CHECK(!QGuiApplication::topLevelWindows().contains(child.data()));
    CHECK(QGuiApplication::topLevelWindows().contains(top));

    // Positions add up through the parents.
    CHECK(child->mapToGlobal(QPoint(1, 2)) == QPoint(111, 72));
    CHECK(child->mapFromGlobal(QPoint(111, 72)) == QPoint(1, 2));
    QWindow grandchild(child);
    grandchild.setGeometry(5, 5, 10, 10);
    CHECK(grandchild.mapToGlobal(QPoint(0, 0)) == QPoint(115, 75));
    CHECK(top->isAncestorOf(&grandchild));

    // Creating a child creates its parent and puts the native window inside it.
    child->create();
    CHECK(top->handle() && child->handle());
    CHECK(platformParent(child) == top->handle());
    CHECK(platformParent(top) == nullptr);

    // A transient parent is not a parent, unless asked for.
    QWindow dialog;
    dialog.setTransientParent(top);
    CHECK(dialog.parent() == nullptr && dialog.isTopLevel());
    CHECK(dialog.parent(QWindow::IncludeTransients) == top);
    CHECK(top->isAncestorOf(&dialog) && !top->isAncestorOf(&dialog, QWindow::ExcludeTransients));

    // Hiding a child does not quit; closing the last top-level window does.
    int lastClosed = 0;
    const QMetaObject::Connection closedConnection =
        QObject::connect(qApp, &QGuiApplication::lastWindowClosed, [&] { ++lastClosed; });
    top->show();
    child->show();
    CHECK(child->isVisible());
    child->hide();
    CHECK(lastClosed == 0);
    QObject::disconnect(closedConnection); // the lambda holds a local, and the signal comes again after this function
    child->show();

    // Destroying the parent destroys the child's native window first; showing it again makes both anew.
    top->destroy();
    CHECK(!child->handle() && !top->handle());
    child->show();
    CHECK(child->handle() && top->handle() && platformParent(child) == top->handle());

    // A child made top-level again leaves its parent.
    grandchild.setParent(nullptr);
    CHECK(grandchild.isTopLevel() && !child->isAncestorOf(&grandchild));
    grandchild.create();
    CHECK(platformParent(&grandchild) == nullptr);

    // The parent deletes its children.
    delete top;
    CHECK(child.isNull());
}

void testContainer() {
    QWidget top;
    auto *layout = new QVBoxLayout(&top);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *label = new QLabel(QStringLiteral("above"));
    label->setFixedHeight(30);
    layout->addWidget(label);
    auto *frame = new QWidget;
    layout->addWidget(frame);
    auto *inner = new QVBoxLayout(frame);
    inner->setContentsMargins(7, 9, 7, 9);
    QPointer<QWindow> window = new QWindow;
    QWidget *container = QWidget::createWindowContainer(window, frame);
    inner->addWidget(container);
    CHECK(container->focusPolicy() == Qt::StrongFocus);
    CHECK(!window->isVisible());

    top.resize(320, 240);
    top.show();
    QCoreApplication::processEvents();
    const auto expected = [&] { return QRect(container->mapTo(&top, QPoint(0, 0)), container->size()); };
    CHECK(window->parent() == top.windowHandle());
    CHECK(platformParent(window) == top.windowHandle()->handle());
    CHECK(window->isVisible());
    CHECK(window->geometry() == expected());
    CHECK(window->geometry().topLeft() == QPoint(7, 30 + 9));

    // It follows the layout when the top-level window is resized.
    top.resize(400, 300);
    QCoreApplication::processEvents();
    CHECK(window->geometry() == expected());
    CHECK(window->width() == 400 - 14);

    // It follows a parent that moves without the container moving within it.
    layout->removeWidget(frame);
    frame->move(50, 60);
    QCoreApplication::processEvents();
    CHECK(window->geometry().topLeft() == QPoint(57, 69));

    // Hidden with the container, or with one of its parents, and shown again with it.
    container->hide();
    CHECK(!window->isVisible());
    container->show();
    CHECK(window->isVisible());
    frame->hide();
    CHECK(!window->isVisible());
    frame->show();
    CHECK(window->isVisible());

    // Into another top-level window.
    QWidget other;
    other.resize(200, 200);
    other.show();
    frame->setParent(&other);
    frame->move(3, 4);
    frame->show();
    QCoreApplication::processEvents();
    CHECK(window->parent() == other.windowHandle());
    CHECK(window->geometry().topLeft() == QPoint(10, 13));
    CHECK(window->isVisible());

    // The container deletes its window.
    delete container;
    CHECK(window.isNull());
}

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    testChildWindows();
    testContainer();
    if (g_failures == 0)
        std::puts("all window container tests passed");
    return g_failures == 0 ? 0 : 1;
}
