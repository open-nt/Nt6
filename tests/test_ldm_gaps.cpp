// MIT License - see LICENSE
//
// Tests for the Qt APIs that the ldm programs needed of Nt: widget and painter additions, item views, editable combo
// boxes, context menus, shortcut overrides, icons, key combinations, network subnets and the other gaps. The
// widgets are on the offscreen platform.
#include <QtCore/QtCore>
#include <QtGui/QtGui>
#include <QtNetwork/QtNetwork>
#include <QtSvg/QtSvg>
#include <QtTest/qsignalspy.h>
#include <QtWidgets/QtWidgets>
#include <QtWebSockets/QtWebSockets>

#include "qpa/qwindowsysteminterface_p.h"

#include <cstdio>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static void settle() {
    for (int i = 0; i < 5; ++i)
        QCoreApplication::processEvents();
}

static void sendMouse(QWidget *window, QEvent::Type type, const QPoint &pos, Qt::MouseButton button) {
    QWindow *handle = window->windowHandle();
    const Qt::MouseButtons buttons = type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::MouseButtons(button);
    QMouseEvent event(type, QPointF(pos), QPointF(window->mapToGlobal(pos)), button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(handle, &event);
}

static void sendKey(QWidget *window, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QWindowSystemInterface::handleKeyEvent(window->windowHandle(), QEvent::KeyPress, key, modifiers);
    QWindowSystemInterface::handleKeyEvent(window->windowHandle(), QEvent::KeyRelease, key, modifiers);
}

// ---- Core and GUI values ----

static void testKeys() {
    const QKeySequence copy(Qt::CTRL | Qt::Key_C);
    const QKeyCombination c = copy[0];
    CHECK(c.key() == Qt::Key_C && c.keyboardModifiers() == Qt::ControlModifier);
    CHECK(c == (Qt::CTRL | Qt::Key_C) && c.toCombined() == (Qt::CTRL | Qt::Key_C));
    CHECK(QKeyCombination::fromCombined(c.toCombined()) == c);
    CHECK(QKeySequence(QKeyCombination(Qt::ShiftModifier, Qt::Key_F13)).toString().contains("F13"));
    QSet<QKeySequence> set{copy, QKeySequence(Qt::Key_F35)};
    CHECK(set.contains(QKeySequence(Qt::CTRL | Qt::Key_C)) && set.size() == 2);
    const QKeyEvent event(QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier);
    CHECK(event.matches(QKeySequence::Copy) && !event.matches(QKeySequence::Paste));
    CHECK(event.keyCombination() == c);
    QAction action;
    action.setShortcuts({QKeySequence(Qt::Key_F5), QKeySequence(Qt::CTRL | Qt::Key_R)});
    CHECK(action.shortcut() == QKeySequence(Qt::Key_F5) && action.shortcuts().size() == 2);
    action.setShortcuts(QKeySequence::Copy);
    CHECK(action.shortcuts().contains(QKeySequence(Qt::CTRL | Qt::Key_C)));
}

static void testValues() {
    QList<int> a{1, 2}, b{3, 4, 5};
    a.append(b.cbegin() + 1, b.cend());
    CHECK(a == (QList<int>{1, 2, 4, 5}));
    QString text;
    QDebug(&text) << QVector3D(1, 2, 3) << QHostAddress(QStringLiteral("10.0.0.1"));
    CHECK(text.contains("QVector3D(1, 2, 3)") && text.contains("QHostAddress(\"10.0.0.1\")"));
    if (!text.contains("QVector3D(1, 2, 3)") || !text.contains("QHostAddress(\"10.0.0.1\")"))
        std::printf("  debug text: %s\n", qPrintable(text));
    // QDataStream comes with the meta-type system, as in Qt.
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out << QList<int>{7, 8};
    QDataStream in(bytes);
    QList<int> back;
    in >> back;
    CHECK(back == (QList<int>{7, 8}) && in.atEnd());
    QGuiApplication::setDesktopFileName(QStringLiteral("org.example.App.desktop"));
    CHECK(QGuiApplication::desktopFileName() == QLatin1String("org.example.App"));
    if (QScreen *screen = QGuiApplication::primaryScreen())
        CHECK(QGuiApplication::screenAt(screen->geometry().center()) == screen);
    CHECK(QGuiApplication::screenAt(QPoint(-100000, -100000)) == nullptr);
}

static void testNetwork() {
    const QHostAddress a(QStringLiteral("192.168.1.42"));
    CHECK(a.isInSubnet(QHostAddress(QStringLiteral("192.168.1.0")), 24));
    CHECK(!a.isInSubnet(QHostAddress(QStringLiteral("192.168.2.0")), 24));
    CHECK(a.isInSubnet(QHostAddress::parseSubnet(QStringLiteral("192.168.0.0/16"))));
    CHECK(a.isInSubnet(QHostAddress::parseSubnet(QStringLiteral("192.168.1.0/255.255.255.0"))));
    const auto subnet = QHostAddress::parseSubnet(QStringLiteral("10.1.2.3/8"));
    CHECK(subnet.first == QHostAddress(QStringLiteral("10.0.0.0")) && subnet.second == 8);
    CHECK(QHostAddress::parseSubnet(QStringLiteral("10.0.0.0/33")).second == -1);
    CHECK(QHostAddress::parseSubnet(QStringLiteral("10.0.0.0/255.0.255.0")).second == -1);
    CHECK(QHostAddress(QStringLiteral("fe80::1")).isInSubnet(QHostAddress::parseSubnet(QStringLiteral("fe80::/10"))));
    CHECK(QHostAddress(QStringLiteral("::ffff:192.168.1.42")).isInSubnet(QHostAddress(QStringLiteral("192.168.1.0")), 24));
    QUdpSocket socket;
    CHECK(socket.bind(QHostAddress::AnyIPv4, 0));
    CHECK(!socket.joinMulticastGroup(QHostAddress(QStringLiteral("239.1.2.3")), QNetworkInterface()));
}

// ---- Painting ----

static void testImages() {
    QImage gray(5, 3, QImage::Format_Grayscale8);
    CHECK(gray.depth() == 8 && gray.bytesPerLine() == 8);
    gray.fill(0);
    gray.scanLine(1)[2] = 200;
    CHECK(gray.pixel(2, 1) == qRgb(200, 200, 200) && gray.pixel(0, 0) == qRgb(0, 0, 0));
    gray.setPixel(4, 2, qRgb(255, 255, 255));
    CHECK(gray.constScanLine(2)[4] == 255);
    const QImage color = gray.convertToFormat(QImage::Format_RGB32);
    CHECK(color.pixel(2, 1) == qRgb(200, 200, 200));
    CHECK(color.convertToFormat(QImage::Format_Grayscale8) == gray);
    CHECK(gray.copy(2, 1, 2, 2).pixel(0, 0) == qRgb(200, 200, 200));
    CHECK(gray.scaled(10, 6).format() == QImage::Format_Grayscale8);
    QImage canvas(5, 3, QImage::Format_RGB32);
    canvas.fill(Qt::red);
    {
        QPainter p(&canvas);
        p.drawImage(QPoint(0, 0), gray);
    }
    CHECK(canvas.pixel(2, 1) == qRgb(200, 200, 200));
    CHECK(QPainter(&gray).isActive() == false); // a painter cannot paint on 8 bits
    QPixmap pixmap(QSize(20, 10));
    pixmap.setDevicePixelRatio(2.0);
    CHECK(pixmap.devicePixelRatio() == 2.0 && pixmap.deviceIndependentSize() == QSizeF(10, 5));
    QFont font;
    font.setHintingPreference(QFont::PreferNoHinting);
    CHECK(font.hintingPreference() == QFont::PreferNoHinting && font != QFont());
}

static void testPainterWindow() {
    QImage image(100, 100, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter p(&image);
    CHECK(p.viewport() == QRect(0, 0, 100, 100) && p.window() == QRect(0, 0, 100, 100));
    CHECK(!p.viewTransformEnabled());
    p.setWindow(0, 0, 10, 10); // 10 by 10 units on 100 by 100 pixels
    CHECK(p.viewTransformEnabled() && p.combinedTransform().m11() == 10.0);
    p.fillRect(QRectF(1, 1, 2, 2), Qt::black);
    p.setViewTransformEnabled(false);
    p.end();
    CHECK(image.pixel(15, 15) == qRgb(0, 0, 0) && image.pixel(35, 35) == qRgb(255, 255, 255));
    // Text grows with the transformation: an upright glyph twice as high under a scale of 2.
    const auto inkHeight = [](qreal scale) {
        QImage text(200, 120, QImage::Format_RGB32);
        text.fill(Qt::white);
        QPainter tp(&text);
        tp.scale(scale, scale);
        tp.setPen(Qt::black);
        tp.setFont(QFont(QStringLiteral("sans-serif"), 12));
        tp.drawText(QPointF(5, 30), QStringLiteral("H"));
        tp.end();
        int top = -1, bottom = -1;
        for (int y = 0; y < text.height(); ++y)
            for (int x = 0; x < text.width(); ++x)
                if (qGray(text.pixel(x, y)) < 128) {
                    if (top < 0)
                        top = y;
                    bottom = y;
                }
        return top < 0 ? 0 : bottom - top + 1;
    };
    // Without an installed font no glyph has ink, so there is nothing to measure.
    if (QFontMetrics(QFont(QStringLiteral("sans-serif"), 10)).inFont(QChar('A'))) {
        const int one = inkHeight(1.0), two = inkHeight(2.0);
        CHECK(one > 0 && two >= one * 2 - 2 && two <= one * 2 + 2);
    } else {
        std::puts("SKIP: no fonts for the text of the transformed painter");
    }
}

// ---- Icons ----

static void testIcons(const QString &dir) {
    CHECK(QIcon().isNull() && QIcon().pixmap(16).isNull());
    QPixmap red(QSize(32, 32));
    red.fill(Qt::red);
    QIcon icon(red);
    CHECK(!icon.isNull() && icon.availableSizes() == (QList<QSize>{QSize(32, 32)}));
    CHECK(icon.pixmap(16).size() == QSize(16, 16) && icon.pixmap(64).size() == QSize(32, 32)); // never scaled up
    CHECK(icon.actualSize(QSize(20, 20)) == QSize(20, 20));
    const QImage disabled = icon.pixmap(16, QIcon::Disabled).toImage();
    CHECK(qAlpha(disabled.pixel(8, 8)) < 255 && qRed(disabled.pixel(8, 8)) == qGreen(disabled.pixel(8, 8)));
    QPixmap blue(QSize(16, 16));
    blue.fill(Qt::blue);
    icon.addPixmap(blue);
    CHECK(icon.pixmap(16).toImage().pixel(8, 8) == qRgb(0, 0, 255)); // the size that fits
    CHECK(icon.pixmap(QSize(16, 16), 2.0).devicePixelRatio() == 2.0);
    QIcon copy = icon;
    CHECK(copy.cacheKey() == icon.cacheKey());
    copy.addPixmap(red, QIcon::Selected);
    CHECK(copy.cacheKey() != icon.cacheKey());
    QImage canvas(40, 40, QImage::Format_RGB32);
    canvas.fill(Qt::white);
    {
        QPainter p(&canvas);
        icon.paint(&p, QRect(0, 0, 40, 40));
    }
    CHECK(canvas.pixel(20, 20) == qRgb(255, 0, 0) && canvas.pixel(2, 2) == qRgb(255, 255, 255));

    // A theme of our own: an index.theme that inherits hicolor, and a PNG and an SVG icon.
    const QString theme = dir + QStringLiteral("/icons/testtheme");
    QDir().mkpath(theme + QStringLiteral("/16x16/actions"));
    QDir().mkpath(theme + QStringLiteral("/scalable/actions"));
    QFile index(theme + QStringLiteral("/index.theme"));
    CHECK(index.open(QIODevice::WriteOnly));
    index.write("[Icon Theme]\nName=Test\nInherits=hicolor\nDirectories=16x16/actions,scalable/actions\n");
    index.close();
    QImage png(16, 16, QImage::Format_ARGB32);
    png.fill(Qt::green);
    CHECK(png.save(theme + QStringLiteral("/16x16/actions/test-green.png")));
    QFile svg(theme + QStringLiteral("/scalable/actions/test-square.svg"));
    CHECK(svg.open(QIODevice::WriteOnly));
    svg.write("<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10'>"
              "<rect width='10' height='10' fill='#0000ff'/></svg>");
    svg.close();
    QIcon::setThemeSearchPaths({dir + QStringLiteral("/icons")});
    QIcon::setThemeName(QStringLiteral("testtheme"));
    CHECK(QIcon::themeName() == QLatin1String("testtheme"));
    CHECK(QIcon::hasThemeIcon(QStringLiteral("test-green")) && !QIcon::hasThemeIcon(QStringLiteral("no-such-icon")));
    const QIcon green = QIcon::fromTheme(QStringLiteral("test-green"));
    CHECK(green.name() == QLatin1String("test-green") && green.pixmap(16).toImage().pixel(8, 8) == qRgb(0, 255, 0));
    const QIcon square = QIcon::fromTheme(QStringLiteral("test-square"));
    const QPixmap big = square.pixmap(48);
    CHECK(big.size() == QSize(48, 48) && big.toImage().pixel(24, 24) == qRgb(0, 0, 255)); // SVG drawn at the size
    CHECK(QIcon::fromTheme(QStringLiteral("no-such-icon"), icon).cacheKey() == icon.cacheKey());
    const QIcon fromFile(theme + QStringLiteral("/16x16/actions/test-green.png"));
    CHECK(fromFile.pixmap(16).toImage().pixel(1, 1) == qRgb(0, 255, 0));

    // Widgets that show icons.
    QAction action(icon, QStringLiteral("Act"));
    CHECK(action.icon().cacheKey() == icon.cacheKey());
    QPushButton button(QStringLiteral("A button with a longer label"));
    const int plain = button.sizeHint().width();
    button.setIcon(icon);
    button.setIconSize(QSize(24, 24));
    CHECK(button.iconSize() == QSize(24, 24) && button.sizeHint().width() > plain);
    QToolButton tool;
    tool.setDefaultAction(&action);
    CHECK(tool.icon().cacheKey() == icon.cacheKey());
    QMenu menu;
    QAction *withIcon = menu.addAction(icon, QStringLiteral("Open"));
    CHECK(!withIcon->icon().isNull());
    QToolBar bar;
    bar.setIconSize(QSize(20, 20));
    QAction *barAction = bar.addAction(icon, QStringLiteral("Bar"));
    CHECK(!barAction->icon().isNull() && bar.iconSize() == QSize(20, 20));
    QMainWindow main;
    main.setIconSize(QSize(18, 18));
    CHECK(main.iconSize() == QSize(18, 18));
    QListWidgetItem item(icon, QStringLiteral("Item"));
    CHECK(item.data(Qt::DecorationRole).value<QIcon>().cacheKey() == icon.cacheKey());
    QComboBox combo;
    combo.addItem(icon, QStringLiteral("One"));
    combo.setIconSize(QSize(12, 12));
    CHECK(!combo.itemIcon(0).isNull() && combo.iconSize() == QSize(12, 12));
    QTreeWidgetItem treeItem;
    treeItem.setIcon(0, icon);
    CHECK(treeItem.icon(0).cacheKey() == icon.cacheKey());
    CHECK(!QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning).isNull());
    QGuiApplication::setWindowIcon(icon);
    QWidget window;
    CHECK(window.windowIcon().cacheKey() == icon.cacheKey());
    window.setWindowIcon(green);
    CHECK(window.windowIcon().cacheKey() == green.cacheKey());
    QGuiApplication::setWindowIcon(QIcon());
}

// ---- Widgets ----

static void testWidgets() {
    QLabel label(QStringLiteral("<a href='x'>link</a>"));
    CHECK(label.textInteractionFlags() == Qt::LinksAccessibleByMouse);
    label.setTextInteractionFlags(Qt::TextSelectableByMouse);
    CHECK(label.textInteractionFlags() == Qt::TextSelectableByMouse);

    QWidget fixed;
    auto *layout = new QVBoxLayout(&fixed);
    auto *inner = new QPushButton(QStringLiteral("A fairly wide button"));
    layout->addWidget(inner);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    CHECK(layout->sizeConstraint() == QLayout::SetFixedSize);
    fixed.show();
    settle();
    CHECK(fixed.size() == layout->sizeHint() && fixed.minimumSize() == fixed.maximumSize());

    QWidget outer;
    auto *child = new QWidget(&outer);
    auto *grandchild = new QCheckBox(child);
    CHECK(grandchild->isVisibleTo(&outer));
    child->hide();
    CHECK(!grandchild->isVisibleTo(&outer) && grandchild->isVisibleTo(child));

    // Shown to the application only.
    QWidget offscreen;
    offscreen.setAttribute(Qt::WA_DontShowOnScreen);
    offscreen.resize(30, 20);
    offscreen.show();
    CHECK(offscreen.isVisible() && offscreen.windowHandle() == nullptr);
    offscreen.hide();
    CHECK(!offscreen.isVisible());

    // render() through a painter.
    QWidget red;
    red.resize(10, 10);
    red.setAutoFillBackground(true);
    QPalette palette;
    palette.setColor(QPalette::Window, Qt::red);
    red.setPalette(palette);
    QImage canvas(30, 30, QImage::Format_RGB32);
    canvas.fill(Qt::white);
    {
        QPainter p(&canvas);
        p.translate(10, 10);
        red.render(&p, QPoint(5, 5));
    }
    CHECK(canvas.pixel(18, 18) == qRgb(255, 0, 0) && canvas.pixel(12, 12) == qRgb(255, 255, 255));

    // QMenu::addAction with a receiver.
    QMenu menu;
    QCheckBox target;
    QAction *slotAction = menu.addAction(QStringLiteral("Toggle"), &target, &QAbstractButton::toggle);
    slotAction->trigger();
    CHECK(target.isChecked());
    QAction *named = menu.addAction(QStringLiteral("Click"), &target, SLOT(click()), QKeySequence(Qt::Key_F7));
    named->trigger();
    CHECK(!target.isChecked() && named->shortcut() == QKeySequence(Qt::Key_F7));

    QGuiApplication::setDesktopFileName(QString());
}

static void testShortcutOverride() {
    struct Claimer : QLineEdit {
        int presses = 0;
        bool event(QEvent *e) override {
            if (e->type() == QEvent::ShortcutOverride && static_cast<QKeyEvent *>(e)->key() == Qt::Key_Delete) {
                e->accept();
                return true;
            }
            if (e->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(e)->key() == Qt::Key_Delete)
                ++presses;
            return QLineEdit::event(e);
        }
    };
    QWidget window;
    auto *edit = new Claimer;
    auto *layout = new QVBoxLayout(&window);
    layout->addWidget(edit);
    int fired = 0;
    auto *action = new QAction(&window);
    action->setShortcut(QKeySequence(Qt::Key_Delete));
    QObject::connect(action, &QAction::triggered, [&fired] { ++fired; });
    window.addAction(action);
    auto *f9 = new QAction(&window);
    f9->setShortcut(QKeySequence(Qt::Key_F9));
    int f9Fired = 0;
    QObject::connect(f9, &QAction::triggered, [&f9Fired] { ++f9Fired; });
    window.addAction(f9);
    window.show();
    settle();
    edit->setFocus();
    sendKey(&window, Qt::Key_Delete);
    CHECK(fired == 0 && edit->presses == 1); // the line edit took Delete
    sendKey(&window, Qt::Key_F9);
    CHECK(f9Fired == 1); // a key nobody claims still triggers its action
}

static void testContextMenus() {
    struct Default : QWidget {
        int menus = 0;
        QContextMenuEvent::Reason reason = QContextMenuEvent::Other;
        QPoint pos;
        void contextMenuEvent(QContextMenuEvent *e) override {
            ++menus;
            reason = e->reason();
            pos = e->pos();
            e->accept();
        }
    };
    QWidget window;
    window.resize(200, 200);
    auto *own = new Default;
    own->setParent(&window);
    own->setGeometry(10, 10, 50, 50);
    auto *custom = new QListWidget(&window);
    custom->setGeometry(100, 10, 80, 80);
    custom->setContextMenuPolicy(Qt::CustomContextMenu);
    CHECK(custom->contextMenuPolicy() == Qt::CustomContextMenu);
    QList<QPoint> requested;
    QObject::connect(custom, &QWidget::customContextMenuRequested, [&](const QPoint &p) { requested.append(p); });
    auto *none = new Default;
    none->setParent(own);
    none->setGeometry(5, 5, 20, 20);
    none->setContextMenuPolicy(Qt::NoContextMenu); // goes to the parent
    window.show();
    settle();
    sendMouse(&window, QEvent::MouseButtonPress, QPoint(30, 40), Qt::RightButton);
    sendMouse(&window, QEvent::MouseButtonRelease, QPoint(30, 40), Qt::RightButton);
    CHECK(own->menus == 1 && own->reason == QContextMenuEvent::Mouse && own->pos == QPoint(20, 30));
    sendMouse(&window, QEvent::MouseButtonPress, QPoint(20, 20), Qt::RightButton); // on `none`
    sendMouse(&window, QEvent::MouseButtonRelease, QPoint(20, 20), Qt::RightButton);
    CHECK(own->menus == 2 && none->menus == 0 && own->pos == QPoint(10, 10));
    sendMouse(&window, QEvent::MouseButtonPress, QPoint(120, 30), Qt::RightButton);
    sendMouse(&window, QEvent::MouseButtonRelease, QPoint(120, 30), Qt::RightButton);
    CHECK(requested.size() == 1);
    if (!requested.isEmpty()) // in the coordinates of the viewport
        CHECK(custom->viewport()->mapTo(&window, requested.first()) == QPoint(120, 30));
    sendMouse(&window, QEvent::MouseButtonPress, QPoint(30, 40), Qt::LeftButton);
    sendMouse(&window, QEvent::MouseButtonRelease, QPoint(30, 40), Qt::LeftButton);
    CHECK(own->menus == 2); // the left button asks for nothing
    own->setFocusPolicy(Qt::StrongFocus);
    own->setFocus();
    sendKey(&window, Qt::Key_Menu);
    CHECK(own->menus == 3 && own->reason == QContextMenuEvent::Keyboard && own->pos == QPoint(24, 24));

    // A scene gives the request to the item under it.
    struct Item : QGraphicsRectItem {
        int menus = 0;
        QPointF pos;
        using QGraphicsRectItem::QGraphicsRectItem;
        void contextMenuEvent(QGraphicsSceneContextMenuEvent *e) override {
            ++menus;
            pos = e->pos();
            e->accept();
        }
    };
    QGraphicsScene scene(0, 0, 100, 100);
    auto *item = new Item(0, 0, 40, 40);
    item->setPos(20, 20);
    scene.addItem(item);
    QGraphicsView view(&scene);
    view.resize(160, 160);
    view.show();
    settle();
    const QPoint at = view.viewport()->mapTo(&view, view.mapFromScene(QPointF(30, 30)));
    sendMouse(&view, QEvent::MouseButtonPress, at, Qt::RightButton);
    sendMouse(&view, QEvent::MouseButtonRelease, at, Qt::RightButton);
    CHECK(item->menus == 1 && qAbs(item->pos.x() - 10) <= 1 && qAbs(item->pos.y() - 10) <= 1);
}

static void testItemViews() {
    // Header resize modes.
    QTableWidget table(3, 3);
    table.resize(400, 200);
    table.setHorizontalHeaderLabels({QStringLiteral("Key"), QStringLiteral("Value"), QStringLiteral("On")});
    QHeaderView *header = table.horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Fixed);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    CHECK(header->sectionResizeMode(0) == QHeaderView::Fixed && header->sectionResizeMode(1) == QHeaderView::Stretch);
    CHECK(header->stretchSectionCount() == 1);
    table.show();
    settle();
    CHECK(header->sectionSize(0) + header->sectionSize(1) + header->sectionSize(2) >= header->width() - 1);
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    CHECK(header->sectionResizeMode(2) == QHeaderView::ResizeToContents);

    // Header items, check boxes, cell widgets, editing and itemChanged.
    QTableWidgetItem *keyHeader = table.horizontalHeaderItem(0);
    CHECK(keyHeader && keyHeader->text() == QLatin1String("Key"));
    keyHeader->setToolTip(QStringLiteral("The key"));
    keyHeader->setText(QStringLiteral("Name"));
    CHECK(table.horizontalHeaderItem(0)->text() == QLatin1String("Name") && table.verticalHeaderItem(0) == nullptr);
    QSignalSpy changed(&table, &QTableWidget::itemChanged);
    auto *check = new QTableWidgetItem(QStringLiteral("enabled"));
    check->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    table.setItem(0, 2, check);
    check->setCheckState(Qt::Checked);
    CHECK(check->checkState() == Qt::Checked && changed.size() == 1 && check->data(Qt::CheckStateRole).toInt() == 2);
    check->setData(Qt::UserRole, 42);
    CHECK(check->data(Qt::UserRole).toInt() == 42 && changed.size() == 2);
    auto *combo = new QComboBox;
    table.setCellWidget(1, 1, combo);
    CHECK(table.cellWidget(1, 1) == combo && combo->parentWidget() == table.viewport());
    table.insertRow(0);
    CHECK(table.cellWidget(2, 1) == combo && table.cellWidget(1, 1) == nullptr);
    table.removeRow(2);
    settle();
    CHECK(table.cellWidget(2, 1) == nullptr);
    auto *value = new QTableWidgetItem(QStringLiteral("old"));
    table.setItem(0, 1, value);
    table.editItem(value);
    auto *editor = table.viewport()->findChild<QLineEdit *>();
    CHECK(editor != nullptr);
    if (editor) {
        editor->setText(QStringLiteral("new"));
        emit editor->editingFinished();
    }
    settle();
    CHECK(value->text() == QLatin1String("new"));

    // List items: data, size hints, item widgets.
    QListWidget list;
    list.resize(200, 200);
    auto *a = new QListWidgetItem(QStringLiteral("a"), &list);
    auto *b = new QListWidgetItem(QStringLiteral("b"), &list);
    a->setData(Qt::UserRole + 1, QStringLiteral("x"));
    CHECK(a->data(Qt::UserRole + 1).toString() == QLatin1String("x") && a->data(Qt::DisplayRole) == QLatin1String("a"));
    a->setSizeHint(QSize(0, 40));
    auto *row = new QPushButton(QStringLiteral("row"));
    list.setItemWidget(a, row);
    list.show();
    settle();
    CHECK(list.itemWidget(a) == row && list.visualItemRect(a).height() == 40);
    CHECK(list.visualItemRect(b).top() == 40 && list.itemAt(QPoint(5, 45)) == b);
    CHECK(row->geometry() == list.visualItemRect(a));

    // Trees: hidden and spanned rows, and a layout at once.
    QTreeWidget tree;
    tree.setColumnCount(2);
    auto *top = new QTreeWidgetItem(&tree, QStringList{QStringLiteral("Heading")});
    auto *other = new QTreeWidgetItem(&tree, QStringList{QStringLiteral("Other")});
    top->setFirstColumnSpanned(true);
    CHECK(top->isFirstColumnSpanned() && !other->isFirstColumnSpanned());
    other->setHidden(true);
    CHECK(other->isHidden() && !top->isHidden());
    tree.doItemsLayout();
    CHECK(tree.visualItemRect(other).isEmpty());
    other->setHidden(false);
    CHECK(!tree.visualItemRect(other).isEmpty());
}

static void testComboBox() {
    QComboBox combo;
    combo.addItems({QStringLiteral("one"), QStringLiteral("two")});
    CHECK(!combo.isEditable() && combo.lineEdit() == nullptr);
    combo.setEditable(true);
    CHECK(combo.isEditable() && combo.lineEdit() && combo.currentText() == QLatin1String("one"));
    QStringList edits;
    QObject::connect(&combo, &QComboBox::editTextChanged, [&](const QString &t) { edits.append(t); });
    combo.setEditText(QStringLiteral("three"));
    CHECK(combo.currentText() == QLatin1String("three") && edits == QStringList{QStringLiteral("three")});
    emit combo.lineEdit()->returnPressed();
    CHECK(combo.count() == 3 && combo.currentIndex() == 2 && combo.itemText(2) == QLatin1String("three"));
    combo.setInsertPolicy(QComboBox::NoInsert);
    combo.setEditText(QStringLiteral("four"));
    emit combo.lineEdit()->returnPressed();
    CHECK(combo.count() == 3);
    combo.setCurrentIndex(0);
    CHECK(combo.lineEdit()->text() == QLatin1String("one"));
    combo.setEditable(false);
    CHECK(!combo.isEditable() && combo.currentText() == QLatin1String("one"));
}

static void testGraphicsAndSpies() {
    QGraphicsScene scene(0, 0, 100, 100);
    auto *group = new QGraphicsItemGroup;
    CHECK(group->handlesChildEvents());
    struct Child : QGraphicsRectItem {
        int presses = 0;
        using QGraphicsRectItem::QGraphicsRectItem;
        void mousePressEvent(QGraphicsSceneMouseEvent *e) override {
            ++presses;
            e->accept();
        }
    };
    auto *child = new Child(0, 0, 50, 50);
    group->addToGroup(child);
    scene.addItem(group);
    QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
    press.setScenePos(QPointF(10, 10));
    press.setButton(Qt::LeftButton);
    press.setButtons(Qt::LeftButton);
    QCoreApplication::sendEvent(&scene, &press);
    CHECK(child->presses == 0); // the group takes it
    group->setHandlesChildEvents(false);
    QGraphicsSceneMouseEvent again(QEvent::GraphicsSceneMousePress);
    again.setScenePos(QPointF(10, 10));
    again.setButton(Qt::LeftButton);
    again.setButtons(Qt::LeftButton);
    QCoreApplication::sendEvent(&scene, &again);
    CHECK(child->presses == 1);

    QSpinBox spin;
    QSignalSpy byName(&spin, SIGNAL(valueChanged(int)));
    CHECK(byName.isValid() && byName.signal() == "valueChanged(int)");
    spin.setValue(5);
    CHECK(byName.size() == 1 && byName.first().first().toInt() == 5);
    QSignalSpy missing(&spin, SIGNAL(noSuchSignal()));
    CHECK(!missing.isValid());
}

static void testClipboard() {
    QClipboard *clipboard = QGuiApplication::clipboard();
    CHECK(clipboard && clipboard == QApplication::clipboard());
    int changes = 0;
    QObject::connect(clipboard, &QClipboard::dataChanged, [&changes] { ++changes; });
    clipboard->setText(QStringLiteral("hello"));
    CHECK(clipboard->text() == QLatin1String("hello") && clipboard->ownsClipboard() && changes == 1);
    QString subtype;
    CHECK(clipboard->text(subtype) == QLatin1String("hello") && subtype == QLatin1String("plain"));
    auto *data = new QMimeData;
    data->setData(QStringLiteral("application/x-shape"), "square");
    clipboard->setMimeData(data);
    CHECK(clipboard->mimeData()->hasFormat(QStringLiteral("application/x-shape")) && changes == 2);
    QImage image(4, 4, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    clipboard->setImage(image);
    CHECK(clipboard->image().pixel(1, 1) == qRgb(0, 0, 255) && clipboard->mimeData()->hasFormat("image/png"));
    CHECK(!clipboard->supportsSelection() && clipboard->text(QClipboard::Selection).isEmpty());
    clipboard->clear();
    CHECK(clipboard->mimeData() == nullptr && clipboard->text().isEmpty());
}

// A tree whose drags carry the text of the items in a format of its own, as gva-architect's palette does.
struct PaletteTree : QTreeWidget {
    QStringList mimeTypes() const override { return {QStringLiteral("application/x-part")}; }
    QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const override {
        auto *data = new QMimeData;
        data->setData(QStringLiteral("application/x-part"), items.value(0) ? items.first()->text(0).toUtf8() : "");
        return data;
    }
};

struct DropWidget : QWidget {
    int enters = 0, moves = 0, leaves = 0;
    QByteArray dropped;
    QPoint at;
    DropWidget() { setAcceptDrops(true); }
    void dragEnterEvent(QDragEnterEvent *e) override {
        ++enters;
        if (e->mimeData()->hasFormat(QStringLiteral("application/x-part")))
            e->acceptProposedAction();
    }
    void dragMoveEvent(QDragMoveEvent *e) override {
        ++moves;
        e->acceptProposedAction();
    }
    void dragLeaveEvent(QDragLeaveEvent *) override { ++leaves; }
    void dropEvent(QDropEvent *e) override {
        dropped = e->mimeData()->data(QStringLiteral("application/x-part"));
        at = e->position().toPoint();
        e->setDropAction(Qt::CopyAction);
        e->accept();
    }
};

// A scene that takes drops itself, as gva-architect's does.
struct DropScene : QGraphicsScene {
    QPointF droppedAt;
    QByteArray dropped;
    using QGraphicsScene::QGraphicsScene;
    void dragEnterEvent(QGraphicsSceneDragDropEvent *e) override { e->acceptProposedAction(); }
    void dragMoveEvent(QGraphicsSceneDragDropEvent *e) override { e->acceptProposedAction(); }
    void dropEvent(QGraphicsSceneDragDropEvent *e) override {
        dropped = e->mimeData()->data(QStringLiteral("application/x-part"));
        droppedAt = e->scenePos();
        e->acceptProposedAction();
    }
};

struct DropItem : QGraphicsRectItem {
    QByteArray dropped;
    using QGraphicsRectItem::QGraphicsRectItem;
    void dragEnterEvent(QGraphicsSceneDragDropEvent *e) override { e->acceptProposedAction(); }
    void dropEvent(QGraphicsSceneDragDropEvent *e) override {
        dropped = e->mimeData()->data(QStringLiteral("application/x-part"));
        e->acceptProposedAction();
    }
};

// Drags the first item of `tree` to `to` in `window`, with the moves and the release sent from the drag's loop.
static void dragTo(QWidget *window, QTreeWidget *tree, const QPoint &to, const QPoint &via = QPoint(-1, -1)) {
    const QPoint from = tree->viewport()->mapTo(window, tree->visualItemRect(tree->topLevelItem(0)).center());
    sendMouse(window, QEvent::MouseButtonPress, from, Qt::LeftButton);
    QTimer::singleShot(20, window, [=] {
        QWindow *handle = window->windowHandle();
        if (via.x() >= 0) {
            QMouseEvent over(QEvent::MouseMove, QPointF(via), QPointF(window->mapToGlobal(via)), Qt::NoButton,
                             Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(handle, &over);
        }
        QMouseEvent move(QEvent::MouseMove, QPointF(to - QPoint(3, 3)), QPointF(window->mapToGlobal(to - QPoint(3, 3))),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(handle, &move);
        QMouseEvent again(QEvent::MouseMove, QPointF(to), QPointF(window->mapToGlobal(to)), Qt::NoButton,
                          Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(handle, &again);
    });
    QTimer::singleShot(40, window, [=] { sendMouse(window, QEvent::MouseButtonRelease, to, Qt::LeftButton); });
    // Moving past the drag distance starts the drag, whose loop runs the timers above.
    const QPoint past = from + QPoint(QApplication::startDragDistance() + 2, 0);
    QMouseEvent start(QEvent::MouseMove, QPointF(past), QPointF(window->mapToGlobal(past)), Qt::NoButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(window->windowHandle(), &start);
}

static void testDragAndDrop() {
    QWidget window;
    window.resize(400, 300);
    auto *tree = new PaletteTree;
    tree->setParent(&window);
    tree->setGeometry(0, 0, 120, 300);
    tree->setDragDropMode(QAbstractItemView::DragOnly);
    CHECK(tree->dragEnabled() && !tree->acceptDrops() && tree->dragDropMode() == QAbstractItemView::DragOnly);
    new QTreeWidgetItem(tree, QStringList{QStringLiteral("camera")});
    auto *target = new DropWidget;
    target->setParent(&window);
    target->setGeometry(140, 10, 100, 100);
    DropScene scene(0, 0, 200, 200);
    auto *view = new QGraphicsView(&scene, &window);
    view->setGeometry(140, 150, 120, 120);
    CHECK(view->acceptDrops());
    window.show();
    settle();
    CHECK(QApplication::startDragDistance() > 0);

    dragTo(&window, tree, QPoint(180, 50));
    settle();
    CHECK(target->enters == 1 && target->moves >= 1 && target->dropped == "camera");
    CHECK(target->at == QPoint(40, 40));

    const QPoint inView = view->viewport()->mapTo(&window, view->mapFromScene(QPointF(50, 50)));
    dragTo(&window, tree, inView);
    settle();
    CHECK(scene.dropped == "camera" && qAbs(scene.droppedAt.x() - 50) <= 2 && qAbs(scene.droppedAt.y() - 50) <= 2);

    // A drag that goes over the target and ends where nothing takes it.
    target->dropped.clear();
    const int enters = target->enters;
    dragTo(&window, tree, QPoint(300, 20), QPoint(180, 50));
    settle();
    CHECK(target->enters == enters + 1 && target->leaves == 1 && target->dropped.isEmpty());

    // The default scene gives drops to the item under them that accepts drops.
    QGraphicsScene plain(0, 0, 200, 200);
    auto *item = new DropItem(0, 0, 60, 60);
    item->setAcceptDrops(true);
    CHECK(item->acceptDrops());
    plain.addItem(item);
    view->setScene(&plain);
    const QPoint onItem = view->viewport()->mapTo(&window, view->mapFromScene(QPointF(20, 20)));
    dragTo(&window, tree, onItem);
    settle();
    CHECK(item->dropped == "camera");

    // QDrag on its own: no window takes it.
    auto *drag = new QDrag(&window);
    CHECK(drag->source() == &window && drag->mimeData() == nullptr);
    CHECK(drag->exec() == Qt::IgnoreAction); // without data there is nothing to drag
    drag->deleteLater();
}

// ---- Validators, desktop services and the standard item model ----

class UrlSink : public QObject {
    Q_OBJECT
public:
    QUrl last;
public slots:
    void open(const QUrl &url) { last = url; }
};

static void testValidators() {
    QIntValidator port(1, 65535);
    QString text;
    int pos = 0;
    const auto check = [&](const QValidator &v, const char *input) {
        text = QString::fromLatin1(input);
        pos = static_cast<int>(text.size());
        return v.validate(text, pos);
    };
    CHECK(check(port, "") == QValidator::Intermediate);
    CHECK(check(port, "8080") == QValidator::Acceptable);
    CHECK(check(port, "0") == QValidator::Intermediate);
    CHECK(check(port, "-1") == QValidator::Invalid);
    CHECK(check(port, "12a") == QValidator::Invalid);
    CHECK(check(port, "700000") == QValidator::Invalid);
    QIntValidator signedRange(-50, 50);
    CHECK(check(signedRange, "-") == QValidator::Intermediate);
    CHECK(check(signedRange, "-49") == QValidator::Acceptable);
    CHECK(check(signedRange, "-51") == QValidator::Invalid);
    QSignalSpy topSpy(&port, &QIntValidator::topChanged);
    port.setTop(1024);
    CHECK(port.top() == 1024 && topSpy.count() == 1);

    QDoubleValidator gain(0.0, 10.0, 2);
    gain.setLocale(QLocale::c());
    gain.setNotation(QDoubleValidator::StandardNotation);
    CHECK(check(gain, "2.5") == QValidator::Acceptable);
    CHECK(check(gain, "2.") == QValidator::Acceptable);
    CHECK(check(gain, ".") == QValidator::Intermediate);
    CHECK(check(gain, "2.555") == QValidator::Invalid);
    CHECK(check(gain, "11") == QValidator::Invalid);
    CHECK(check(gain, "-1") == QValidator::Invalid);
    CHECK(check(gain, "1e2") == QValidator::Invalid);
    gain.setNotation(QDoubleValidator::ScientificNotation);
    CHECK(check(gain, "1e") == QValidator::Intermediate && check(gain, "1e0") == QValidator::Acceptable);

    QRegularExpressionValidator callSign(QRegularExpression("[A-Z]{2}[0-9]{1,3}"));
    CHECK(check(callSign, "AB12") == QValidator::Acceptable);
    CHECK(check(callSign, "AB") == QValidator::Intermediate);
    CHECK(check(callSign, "a") == QValidator::Invalid);

    // A line edit refuses invalid edits and finishes only acceptable text.
    QLineEdit edit;
    edit.setValidator(&port);
    CHECK(edit.validator() == &port);
    QSignalSpy rejected(&edit, &QLineEdit::inputRejected);
    QSignalSpy returned(&edit, &QLineEdit::returnPressed);
    edit.insert("80");
    edit.insert("x");
    CHECK(edit.text() == "80" && rejected.count() == 1 && edit.hasAcceptableInput());
    edit.setText("0");
    CHECK(!edit.hasAcceptableInput());
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::sendEvent(&edit, &enter);
    CHECK(returned.count() == 0);
    edit.setText("443");
    QCoreApplication::sendEvent(&edit, &enter);
    CHECK(returned.count() == 1);
    edit.setValidator(nullptr);
    edit.insert("x");
    CHECK(edit.text().endsWith("x"));
}

static void testDesktopServices() {
    UrlSink sink;
    QDesktopServices::setUrlHandler("help", &sink, "open");
    CHECK(QDesktopServices::openUrl(QUrl("help:topics/start")));
    CHECK(sink.last == QUrl("help:topics/start"));
    QDesktopServices::unsetUrlHandler("help");
    sink.last = QUrl();
    CHECK(!QDesktopServices::openUrl(QUrl()));
    CHECK(sink.last.isEmpty());
}

static void testStandardItemModel() {
    QStandardItemModel model(0, 2);
    model.setHorizontalHeaderLabels({"Name", "Size"});
    CHECK(model.headerData(1, Qt::Horizontal).toString() == "Size");
    CHECK(model.headerData(0, Qt::Vertical).isValid() == false); // no rows yet
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy changed(&model, &QStandardItemModel::itemChanged);
    auto *fruit = new QStandardItem("Fruit");
    model.appendRow({fruit, new QStandardItem("2")});
    fruit->appendRow(new QStandardItem("Cherry"));
    fruit->appendRow(new QStandardItem("Apple"));
    CHECK(inserted.count() == 3 && model.rowCount() == 1 && model.columnCount() == 2);
    const QModelIndex fruitIndex = model.index(0, 0);
    CHECK(model.itemFromIndex(fruitIndex) == fruit && fruit->index() == fruitIndex);
    CHECK(model.rowCount(fruitIndex) == 2 && model.hasChildren(fruitIndex));
    const QModelIndex cherry = model.index(0, 0, fruitIndex);
    CHECK(model.data(cherry).toString() == "Cherry" && model.parent(cherry) == fruitIndex);
    CHECK(fruit->child(1)->parent() == fruit && fruit->parent() == nullptr && fruit->child(1)->row() == 1);
    CHECK(model.headerData(0, Qt::Vertical).toString() == "1");

    CHECK(model.setData(cherry, "Cherries"));
    CHECK(fruit->child(0)->text() == "Cherries" && changed.count() == 1);
    fruit->child(0)->setEnabled(false);
    CHECK(!(model.flags(cherry) & Qt::ItemIsEnabled) && changed.count() == 2);
    fruit->setCheckable(true);
    CHECK(fruit->checkState() == Qt::Unchecked && (model.flags(fruitIndex) & Qt::ItemIsUserCheckable));

    // Sorting moves the rows and the persistent indexes with them.
    const QPersistentModelIndex kept(cherry);
    model.sort(0);
    CHECK(fruit->child(0)->text() == "Apple" && kept.row() == 1 && kept.data().toString() == "Cherries");
    model.sort(0, Qt::DescendingOrder);
    CHECK(fruit->child(0)->text() == "Cherries" && kept.row() == 0);

    CHECK(model.findItems("apple", Qt::MatchFixedString | Qt::MatchRecursive).size() == 1);
    CHECK(model.findItems("Apple").isEmpty()); // not at the top level
    CHECK(model.findItems("Fr", Qt::MatchStartsWith).value(0) == fruit);

    // An empty place gets an item when it is set through an index.
    model.setRowCount(3);
    CHECK(model.item(2) == nullptr && model.setData(model.index(2, 1), 42));
    CHECK(model.item(2, 1) && model.item(2, 1)->data(Qt::DisplayRole).toInt() == 42);
    const QList<QStandardItem *> taken = model.takeRow(0);
    CHECK(taken.size() == 2 && taken.at(0) == fruit && fruit->model() == nullptr && model.rowCount() == 2);
    qDeleteAll(taken);
    CHECK(model.removeRows(0, 2) && model.rowCount() == 0);
    model.clear();
    CHECK(model.columnCount() == 0 && model.horizontalHeaderItem(0) == nullptr);
}

static void testComboBoxModel() {
    QComboBox combo;
    combo.addItems({"CMake", "Nt 6", "qmake"});
    auto *model = qobject_cast<QStandardItemModel *>(combo.model());
    CHECK(model && model->rowCount() == 3 && model->item(1)->text() == "Nt 6");
    model->item(1)->setEnabled(false);
    combo.setItemData(1, "not installed", Qt::ToolTipRole);
    CHECK(model->item(1)->toolTip() == "not installed");
    combo.setCurrentIndex(0);
    QKeyEvent down(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
    QCoreApplication::sendEvent(&combo, &down);
    CHECK(combo.currentIndex() == 2); // the disabled item is skipped
    QSignalSpy text(&combo, &QComboBox::currentTextChanged);
    model->item(2)->setText("Meson");
    CHECK(combo.currentText() == "Meson" && text.count() == 1);
    model->removeRow(2);
    CHECK(combo.currentIndex() == 1 && combo.count() == 2);
    model->insertRow(0, new QStandardItem("Bazel"));
    CHECK(combo.currentIndex() == 2 && combo.itemText(0) == "Bazel");

    QStandardItemModel other(0, 2);
    other.appendRow({new QStandardItem("a"), new QStandardItem("first")});
    other.appendRow({new QStandardItem("b"), new QStandardItem("second")});
    combo.setModel(&other);
    combo.setModelColumn(1);
    CHECK(combo.count() == 2 && combo.currentIndex() == 0 && combo.currentText() == "first");
    combo.addItem("third");
    CHECK(other.item(2, 1)->text() == "third" && combo.itemText(2) == "third");
}

static void testScroller() {
    QWidget window;
    window.resize(240, 240);
    auto *list = new QListWidget(&window);
    list->setGeometry(0, 0, 200, 200);
    for (int i = 0; i < 200; ++i)
        list->addItem(QString("Message %1").arg(i));
    window.show();
    settle();
    QWidget *viewport = list->viewport();
    CHECK(!QScroller::hasScroller(viewport));
    CHECK(QScroller::grabGesture(viewport, QScroller::LeftMouseButtonGesture) == Qt::CustomGesture);
    QScroller *scroller = QScroller::scroller(viewport);
    CHECK(scroller && QScroller::hasScroller(viewport) && scroller->target() == viewport);
    CHECK(QScroller::grabbedGesture(viewport) == Qt::CustomGesture);
    QScrollerProperties props = scroller->scrollerProperties();
    CHECK(props.scrollMetric(QScrollerProperties::DragStartDistance).toDouble() > 0);
    props.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    props.setScrollMetric(QScrollerProperties::DragStartDistance, 0.004);
    QSignalSpy propsSpy(scroller, &QScroller::scrollerPropertiesChanged);
    scroller->setScrollerProperties(props);
    CHECK(propsSpy.count() == 1 && scroller->scrollerProperties() == props);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    CHECK(list->verticalScrollMode() == QAbstractItemView::ScrollPerPixel);

    // A slow drag moves the content with the mouse and stops with it.
    QScrollBar *bar = list->verticalScrollBar();
    CHECK(bar->maximum() > 100 && bar->value() == 0);
    QSignalSpy states(scroller, &QScroller::stateChanged);
    CHECK(!scroller->handleInput(QScroller::InputPress, QPointF(50, 150), 1000));
    CHECK(scroller->state() == QScroller::Pressed);
    CHECK(scroller->handleInput(QScroller::InputMove, QPointF(50, 120), 1100));
    CHECK(scroller->state() == QScroller::Dragging);
    CHECK(scroller->handleInput(QScroller::InputMove, QPointF(50, 80), 1200));
    CHECK(bar->value() > 30);
    CHECK(scroller->handleInput(QScroller::InputRelease, QPointF(50, 80), 1500)); // held still: no flick
    CHECK(scroller->state() == QScroller::Inactive && states.count() == 3);
    const int afterDrag = bar->value();

    // A flick goes on after the release and slows down.
    QElapsedTimer clock;
    clock.start();
    const qint64 t = 100000;
    scroller->handleInput(QScroller::InputPress, QPointF(50, 180), t);
    scroller->handleInput(QScroller::InputMove, QPointF(50, 160), t + 10);
    scroller->handleInput(QScroller::InputMove, QPointF(50, 120), t + 20);
    scroller->handleInput(QScroller::InputMove, QPointF(50, 80), t + 30);
    // The release time must be close to the clock of the scroller, which handleInput() uses without a timestamp.
    CHECK(scroller->handleInput(QScroller::InputRelease, QPointF(50, 80)));
    CHECK(scroller->state() == QScroller::Scrolling || bar->value() > afterDrag);
    const int released = bar->value();
    while (scroller->state() == QScroller::Scrolling && clock.elapsed() < 3000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    CHECK(scroller->state() == QScroller::Inactive && bar->value() >= released);

    // The gesture: mouse events on the viewport.
    const int before = bar->value();
    const auto mouse = [&](QEvent::Type type, int y) {
        QMouseEvent e(type, QPointF(50, y), QPointF(viewport->mapToGlobal(QPoint(50, y))), Qt::LeftButton,
                      type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::MouseButtons(Qt::LeftButton),
                      Qt::NoModifier);
        QCoreApplication::sendEvent(viewport, &e);
    };
    mouse(QEvent::MouseButtonPress, 50);
    mouse(QEvent::MouseMove, 80);
    mouse(QEvent::MouseMove, 110);
    CHECK(scroller->state() == QScroller::Dragging && bar->value() < before);
    scroller->stop();
    mouse(QEvent::MouseButtonRelease, 110);
    CHECK(scroller->state() == QScroller::Inactive);

    scroller->scrollTo(QPointF(0, 0));
    CHECK(bar->value() == 0);
    scroller->scrollTo(QPointF(0, 300), 50);
    CHECK(scroller->state() == QScroller::Scrolling && scroller->finalPosition().y() == 300);
    clock.restart();
    while (scroller->state() == QScroller::Scrolling && clock.elapsed() < 2000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    CHECK(bar->value() == 300);
    scroller->ensureVisible(QRectF(0, 10, 10, 10), 0, 5, 0);
    CHECK(bar->value() == 5);

    // The press is held back: a drag that starts in time selects nothing, and a click arrives whole.
    scroller->scrollTo(QPointF(0, 0));
    list->clearSelection();
    list->setCurrentRow(-1);
    QSignalSpy pressed(list, &QListWidget::itemPressed);
    QSignalSpy clicked(list, &QListWidget::itemClicked);
    mouse(QEvent::MouseButtonPress, 150);
    CHECK(pressed.count() == 0 && list->selectedItems().isEmpty());
    mouse(QEvent::MouseMove, 148); // no further than a finger trembles
    mouse(QEvent::MouseMove, 100);
    mouse(QEvent::MouseMove, 60);
    CHECK(scroller->state() == QScroller::Dragging && bar->value() > 0);
    scroller->stop();
    mouse(QEvent::MouseButtonRelease, 60);
    const auto elapse = [](int ms) {
        QElapsedTimer waited;
        waited.start();
        while (waited.elapsed() < ms)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    };
    elapse(350);
    CHECK(pressed.count() == 0 && clicked.count() == 0 && list->selectedItems().isEmpty());

    mouse(QEvent::MouseButtonPress, 50);
    mouse(QEvent::MouseButtonRelease, 50);
    CHECK(pressed.count() == 1 && clicked.count() == 1 && list->selectedItems().size() == 1);
    CHECK(scroller->state() == QScroller::Inactive);
    elapse(350);
    CHECK(pressed.count() == 1); // once only

    // A press that is held arrives when the delay is over.
    list->clearSelection();
    mouse(QEvent::MouseButtonPress, 90);
    CHECK(pressed.count() == 1);
    elapse(350);
    CHECK(pressed.count() == 2 && clicked.count() == 1 && list->selectedItems().size() == 1);
    mouse(QEvent::MouseButtonRelease, 90);
    CHECK(clicked.count() == 2);

    // Without a delay the target gets the press at once.
    props.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.0);
    scroller->setScrollerProperties(props);
    mouse(QEvent::MouseButtonPress, 50);
    CHECK(pressed.count() == 3);
    mouse(QEvent::MouseButtonRelease, 50);

    // A gesture that is given up takes the press it holds with it.
    props.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.25);
    scroller->setScrollerProperties(props);
    mouse(QEvent::MouseButtonPress, 90);
    QScroller::ungrabGesture(viewport);
    elapse(350);
    CHECK(pressed.count() == 3);

    CHECK(QScroller::grabbedGesture(viewport) == 0);
    delete list;
    CHECK(!QScroller::hasScroller(viewport));
}

static void testGraphicsWidget() {
    QGraphicsScene scene(0, 0, 800, 600);
    auto *container = new QGraphicsWidget();
    CHECK(container->type() == QGraphicsWidget::Type && container->size() == QSizeF(0, 0));
    container->setZValue(5);
    container->setVisible(false);
    scene.addItem(container);

    auto *text = new QLabel("ATLAS");
    text->setFixedSize(120, 40);
    auto *textProxy = new QGraphicsProxyWidget(container);
    textProxy->setWidget(text);
    auto *image = new QLabel;
    image->setFixedSize(200, 100);
    auto *imageProxy = new QGraphicsProxyWidget(container);
    imageProxy->setWidget(image);
    CHECK(textProxy->preferredSize() == QSizeF(120, 40) && imageProxy->maximumSize() == QSizeF(200, 100));

    auto *layout = new QGraphicsLinearLayout(Qt::Horizontal, container);
    CHECK(container->layout() == layout && layout->orientation() == Qt::Horizontal);
    layout->setSpacing(12);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addItem(textProxy);
    layout->addItem(imageProxy);
    layout->setAlignment(textProxy, Qt::AlignVCenter);
    layout->setAlignment(imageProxy, Qt::AlignVCenter);
    container->setLayout(layout); // the same layout again changes nothing
    CHECK(layout->count() == 2 && layout->itemAt(1) == imageProxy && layout->spacing() == 12);
    CHECK(static_cast<QGraphicsProxyWidget *>(layout->itemAt(0))->widget() == text);

    container->adjustSize();
    CHECK(container->size() == QSizeF(332, 100));
    CHECK(textProxy->geometry() == QRectF(0, 30, 120, 40)); // centred down
    CHECK(imageProxy->geometry() == QRectF(132, 0, 200, 100));
    container->setPos(234, 250);
    CHECK(container->geometry() == QRectF(234, 250, 332, 100));
    CHECK(textProxy->scenePos() == QPointF(234, 280));

    // A bigger image: the layout follows on adjustSize().
    image->setFixedSize(300, 150);
    container->adjustSize();
    CHECK(container->size() == QSizeF(432, 150) && textProxy->pos() == QPointF(0, 55));

    // Room beyond the preferred size goes to a stretch.
    QGraphicsWidget column;
    auto *vertical = new QGraphicsLinearLayout(Qt::Vertical);
    vertical->setContentsMargins(4, 4, 4, 4);
    vertical->setSpacing(2);
    auto *a = new QGraphicsWidget;
    a->setPreferredSize(30, 20);
    a->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *b = new QGraphicsWidget;
    b->setPreferredSize(30, 20);
    vertical->addItem(a);
    vertical->addStretch();
    vertical->addItem(b);
    column.setLayout(vertical);
    CHECK(a->parentItem() == &column && a->parentLayoutItem() == vertical);
    CHECK(vertical->count() == 2 && vertical->itemAt(1) == b);
    column.resize(60, 100);
    CHECK(a->geometry() == QRectF(4, 4, 30, 20));
    CHECK(b->geometry().bottom() == 96 && b->geometry().width() == 52);
    QSignalSpy geometry(b, &QGraphicsWidget::geometryChanged);
    vertical->setStretchFactor(b, 2);
    CHECK(vertical->stretchFactor(b) == 2);
    QCoreApplication::processEvents(); // the posted layout request
    CHECK(b->geometry().height() > 20 && geometry.count() >= 1);
    vertical->removeItem(a);
    CHECK(vertical->count() == 1 && a->parentLayoutItem() == nullptr);
    delete b; // leaves the layout
    CHECK(vertical->count() == 0);
}

static bool hasTable(QTextFrame *frame) {
    for (QTextFrame *child : frame->childFrames())
        if (qobject_cast<QTextTable *>(child) || hasTable(child))
            return true;
    return false;
}

static void testMarkdownAndTables() {
    const QString markdown = QStringLiteral(
        "# Help\n\nSome *emphasis*, **strong** text, `code` and a [link](topic.md).\n\n"
        "- one\n- two\n\n1. first\n2. second\n\n"
        "| Key | Action |\n|-----|:------:|\n| F1  | Help   |\n| Esc | Close \\| quit |\n\n"
        "```\nint x = 1;\n```\n\n> quoted\n\n---\n\nend  \nline\n");
    QTextDocument doc;
    doc.setMarkdown(markdown);
    const QString plain = doc.toPlainText();
    CHECK(plain.startsWith("Help\n"));
    CHECK(plain.contains("Some emphasis, strong text, code and a link."));
    CHECK(plain.contains("Key\tAction") && plain.contains("Esc\tClose | quit"));
    CHECK(plain.contains("int x = 1;") && plain.contains("quoted"));
    CHECK(doc.firstBlock().blockFormat().headingLevel() == 1);
    CHECK(doc.documentLayout()->anchorAt(QPointF(-1, -1)).isEmpty());

    QTextFrame *root = doc.rootFrame();
    CHECK(root && root->parentFrame() == nullptr && root->document() == &doc);
    CHECK(root->firstPosition() == 0 && root->lastPosition() == doc.characterCount() - 1);
    CHECK(hasTable(root));
    const QList<QTextFrame *> frames = root->childFrames();
    auto *table = frames.isEmpty() ? nullptr : qobject_cast<QTextTable *>(frames.first());
    CHECK(table && table->rows() == 3 && table->columns() == 2 && table->parentFrame() == root);
    if (table) {
        CHECK(table->format().border() == 1 && table->format().headerRowCount() == 1);
        CHECK(table->format().isTableFormat() && table->frameFormat().isFrameFormat());
        CHECK(doc.toPlainText().mid(table->firstPosition()).startsWith("Key"));
        QSignalSpy changed(&doc, &QTextDocument::contentsChanged);
        QTextTableFormat format = table->format();
        format.setBorder(2);
        format.setBorderBrush(QColor(255, 0, 0));
        format.setCellPadding(5);
        format.setCellSpacing(0);
        format.setHeaderRowCount(1);
        table->setFormat(format);
        CHECK(changed.count() == 1 && table->format().border() == 2 && table->format().columns() == 2);
        // The border is drawn in its color.
        QImage image(doc.size().toSize(), QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        doc.drawContents(&painter);
        painter.end();
        int red = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                red += qRed(image.pixel(x, y)) > 200 && qGreen(image.pixel(x, y)) < 60;
        CHECK(red > 100);
    }
    const QString back = doc.toMarkdown();
    CHECK(back.contains("# Help") && back.contains("|Key|Action|") && back.contains("|---|---|"));
    CHECK(back.contains("- one") && back.contains("1. first") && back.contains("*emphasis*"));

    // Tables of HTML are frames too; a new content makes new frames.
    doc.setHtml("<table border=\"1\"><tr><th>a</th><th>b</th><th>c</th></tr><tr><td>1</td><td>2</td></tr></table>");
    CHECK(doc.rootFrame() == root && root->childFrames().size() == 1);
    auto *htmlTable = qobject_cast<QTextTable *>(root->childFrames().value(0));
    CHECK(htmlTable && htmlTable->rows() == 2 && htmlTable->columns() == 3);
    doc.setPlainText("no tables");
    CHECK(root->childFrames().isEmpty() && !hasTable(root));

    // A browser shows Markdown, from setMarkdown() or a .md source, and finds text in it.
    QTextBrowser browser;
    browser.setMarkdown(markdown);
    CHECK(browser.toPlainText().contains("Key\tAction") && hasTable(browser.document()->rootFrame()));
    CHECK(browser.toMarkdown().contains("# Help"));
    CHECK(browser.find("help"));
    CHECK(browser.textCursor().selectedText() == "Help");
    CHECK(browser.find("HELP")); // the F1 row
    CHECK(!browser.find("help", QTextDocument::FindCaseSensitively));
    CHECK(browser.find("Help", QTextDocument::FindBackward | QTextDocument::FindCaseSensitively));
    CHECK(browser.find(QRegularExpression("F\\d")) && browser.textCursor().selectedText() == "F1");
    CHECK(!browser.find("emph", QTextDocument::FindWholeWords));
    QTemporaryDir dir;
    QFile file(dir.filePath("help.md"));
    CHECK(file.open(QIODevice::WriteOnly));
    file.write("## Topic\n\n| A | B |\n|---|---|\n| 1 | 2 |\n");
    file.close();
    browser.setSource(QUrl::fromLocalFile(file.fileName()));
    CHECK(browser.toPlainText().startsWith("Topic") && hasTable(browser.document()->rootFrame()));

    // The converter on its own: tight and loose lists, task lists, autolinks, raw HTML and escapes.
    doc.setMarkdown("- [ ] todo\n- [x] done\n\nVisit https://example.org or <me@example.org>.\n\n"
                    "<b>bold</b> \\*not emphasis\\*\n\n~~gone~~ and _under_\n\nTitle\n=====\n");
    const QString text = doc.toPlainText();
    CHECK(text.contains(QChar(0x2610)) && text.contains(QChar(0x2611)));
    CHECK(text.contains("https://example.org") && text.contains("me@example.org"));
    CHECK(text.contains("bold *not emphasis*") && text.contains("gone and under") && text.contains("Title"));
    CHECK(doc.lastBlock().blockFormat().headingLevel() == 1);
    doc.setMarkdown("<b>x</b>", QTextDocument::MarkdownNoHTML);
    CHECK(doc.toPlainText() == "<b>x</b>");
}

// ---- The tail of the ldm survey: widgets, views, text and the rest ----

class CountingDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    mutable int painted = 0;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        ++painted;
        QStyledItemDelegate::paint(painter, option, index);
    }
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override { return QSize(100, 40); }
};

class Clicks : public QObject {
    Q_OBJECT
public:
    int count = 0;
public slots:
    void hit() { ++count; }
};

static void testWidgetTail() {
    static_assert(QT_CONFIG(tooltip) && QT_CONFIG(shortcut), "uic features");
    CHECK(QWIDGETSIZE_MAX == QLAYOUTSIZE_MAX);
    CHECK(Qt::Key_Underscore == 0x5f && Qt::Key_BracketLeft == 0x5b && Qt::Key_Super_L == 0x01000053);
    CHECK(Qt::ReplaceSelection != Qt::AddToSelection);
    CHECK(QKeySequence(QKeySequence::Preferences) == QKeySequence(Qt::CTRL | Qt::Key_Comma));

    QWidget window;
    window.resize(400, 300);
    auto *layout = new QVBoxLayout(&window);
    layout->setContentsMargins(0, 0, 0, 0);
    window.setContentsMargins(10, 20, 30, 40);
    CHECK(window.contentsMargins() == QMargins(10, 20, 30, 40) && window.contentsRect() == QRect(10, 20, 360, 240));
    auto *bar = new QMenuBar;
    bar->addMenu("File");
    layout->setMenuBar(bar);
    CHECK(layout->menuBar() == bar && bar->parentWidget() == &window);
    auto *label = new QLabel("&Name:");
    auto *edit = new QLineEdit;
    label->setBuddy(edit);
    CHECK(label->buddy() == edit);
    layout->addWidget(label);
    layout->addWidget(edit);
    label->setFrameShape(QFrame::Box);
    CHECK(label->frameShape() == QFrame::Box && label->frameWidth() > 0);
    label->setStatusTip("tip");
    label->setWhatsThis("help");
    label->setAccessibleName("name");
    CHECK(label->statusTip() == "tip" && label->whatsThis() == "help" && label->accessibleName() == "name");
    edit->setInputMethodHints(Qt::ImhHiddenText | Qt::ImhSensitiveData);
    CHECK(edit->inputMethodHints() & Qt::ImhSensitiveData);
    window.show();
    settle();
    CHECK(bar->geometry().top() == 20 && label->geometry().top() >= bar->geometry().bottom());
    CHECK(label->geometry().left() == 10);
    sendKey(&window, Qt::Key_N, Qt::AltModifier);
    settle();
    CHECK(edit->hasFocus());

    // grab() and an opacity effect.
    auto *red = new QWidget(&window);
    red->setAutoFillBackground(true);
    QPalette palette = red->palette();
    palette.setColor(QPalette::Window, Qt::red);
    red->setPalette(palette);
    red->setGeometry(300, 200, 40, 30);
    red->show();
    const QPixmap shot = red->grab();
    CHECK(shot.size() == QSize(40, 30) && qRed(shot.toImage().pixel(5, 5)) > 200);
    auto *effect = new QGraphicsOpacityEffect;
    effect->setOpacity(0.5);
    red->setGraphicsEffect(effect);
    CHECK(red->graphicsEffect() == effect && effect->parent() == red);
    red->setMinimumWidth(10);
    CHECK(red->minimumWidth() == 10 && red->maximumWidth() == QWIDGETSIZE_MAX);

    // A grab of the mouse sends every move to the grabber.
    struct Grabber : QWidget {
        int moves = 0;
        void mouseMoveEvent(QMouseEvent *) override { ++moves; }
    };
    auto *grabber = new Grabber;
    grabber->setParent(&window);
    grabber->setGeometry(0, 0, 10, 10);
    grabber->show();
    grabber->grabMouse();
    CHECK(QWidget::mouseGrabber() == grabber);
    sendMouse(&window, QEvent::MouseMove, QPoint(200, 150), Qt::NoButton);
    CHECK(grabber->moves == 1);
    grabber->releaseMouse();
    CHECK(QWidget::mouseGrabber() == nullptr);

    // Buttons: shortcuts and the mnemonic of the text.
    auto *button = new QPushButton("&Save", &window);
    button->show();
    QSignalSpy clicked(button, &QPushButton::clicked);
    CHECK(button->shortcut() == QKeySequence(Qt::ALT | Qt::Key_S));
    sendKey(&window, Qt::Key_S, Qt::AltModifier);
    settle();
    CHECK(clicked.count() == 1);
    button->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K));
    sendKey(&window, Qt::Key_K, Qt::ControlModifier);
    settle();
    CHECK(clicked.count() == 2);

    QAction action("&Open");
    action.setIconText("Open it");
    action.setMenuRole(QAction::PreferencesRole);
    action.setWhatsThis("Opens");
    action.setIconVisibleInMenu(false);
    CHECK(action.iconText() == "Open it" && action.menuRole() == QAction::PreferencesRole);
    CHECK(action.whatsThis() == "Opens" && !action.isIconVisibleInMenu());

    // Menus with a shortcut and a receiver.
    QMenu menu;
    Clicks sink;
    QAction *a1 = menu.addAction("One", QKeySequence(Qt::CTRL | Qt::Key_1), &sink, &Clicks::hit);
    QAction *a2 = menu.addAction("Two", QKeySequence::Copy, &sink, [&sink] { sink.count += 10; });
    QAction *a3 = menu.addAction("Three", QKeySequence::Paste, [&sink] { sink.count += 100; });
    QAction *a4 = menu.addAction("Four", QKeySequence(Qt::Key_F4), &sink, SLOT(hit()));
    a1->trigger();
    a2->trigger();
    a3->trigger();
    a4->trigger();
    CHECK(sink.count == 112 && a2->shortcut() == QKeySequence(QKeySequence::Copy));

    // Spin box buttons, combo box placeholder, view and delegate.
    QSpinBox spin;
    spin.resize(120, 30);
    spin.setButtonSymbols(QAbstractSpinBox::NoButtons);
    CHECK(spin.buttonSymbols() == QAbstractSpinBox::NoButtons);
    QComboBox combo(&window);
    combo.setPlaceholderText("Pick one");
    CHECK(combo.placeholderText() == "Pick one");
    combo.addItems({"Alpha", "beta", "Gamma"});
    CHECK(combo.findText("BETA", Qt::MatchFixedString) == 1 && combo.findText("BETA") == -1);
    combo.view()->setMinimumWidth(280);
    CountingDelegate delegate;
    combo.setItemDelegate(&delegate);
    CHECK(combo.itemDelegate() == &delegate);
    combo.setGeometry(10, 10, 100, 25);
    combo.show();
    combo.showPopup();
    settle();
    window.repaint();
    settle();
    CHECK(combo.isPopupVisible() && delegate.painted >= 3);
    combo.hidePopup();

    // Form rows.
    QWidget formHost;
    auto *form = new QFormLayout(&formHost);
    auto *field = new QLineEdit;
    form->addRow("Name", field);
    form->addRow("Port", new QSpinBox);
    form->setRowVisible(field, false);
    CHECK(!form->isRowVisible(0) && form->isRowVisible(1) && field->isHidden());
    form->setRowVisible(0, true);
    CHECK(!field->isHidden());
    const QFormLayout::TakeRowResult row = form->takeRow(field);
    CHECK(row.fieldItem && row.fieldItem->widget() == field && form->rowCount() == 1);
    delete row.labelItem->widget();
    delete row.labelItem;
    delete row.fieldItem;
    delete field;

    // Dock sizes.
    QMainWindow main;
    main.resize(600, 400);
    auto *dock = new QDockWidget("Properties");
    dock->setWidget(new QLabel("x"));
    main.addDockWidget(Qt::LeftDockWidgetArea, dock);
    main.setCentralWidget(new QWidget);
    main.resizeDocks({dock}, {150}, Qt::Horizontal);
    main.show();
    settle();
    CHECK(dock->width() == 150);

    // Tab buttons.
    QTabBar tabs;
    tabs.addTab("One");
    tabs.addTab("Two");
    auto *close = new QToolButton;
    close->setFixedSize(12, 12);
    tabs.setTabButton(1, QTabBar::RightSide, close);
    tabs.setExpanding(false);
    tabs.setDrawBase(false);
    tabs.setDocumentMode(true);
    tabs.resize(tabs.sizeHint());
    tabs.show();
    settle();
    CHECK(tabs.tabButton(1, QTabBar::RightSide) == close && close->parentWidget() == &tabs);
    CHECK(tabs.tabRect(1).contains(close->geometry()));
    CHECK(!tabs.expanding() && !tabs.drawBase() && tabs.documentMode());
}

static void testViewTail() {
    // QListWidget: hidden items, icon mode, spacing.
    QListWidget list;
    list.resize(300, 200);
    auto *first = new QListWidgetItem(&list);
    first->setText("first");
    CHECK(first->listWidget() == &list && first->type() == QListWidgetItem::Type);
    new QListWidgetItem("second", &list);
    new QListWidgetItem("third", &list);
    list.item(1)->setHidden(true);
    CHECK(list.item(1)->isHidden() && list.isRowHidden(1));
    list.show();
    settle();
    CHECK(list.visualItemRect(list.item(1)).isEmpty());
    CHECK(list.visualItemRect(list.item(2)).top() == list.visualItemRect(list.item(0)).bottom() + 1);
    list.setSpacing(4);
    CHECK(list.visualItemRect(list.item(0)).top() == 4);
    list.setViewMode(QListView::IconMode);
    list.setGridSize(QSize(80, 70));
    list.setMovement(QListView::Static);
    list.setResizeMode(QListView::Adjust);
    list.setUniformItemSizes(true);
    list.setWordWrap(true);
    const QRect a = list.visualItemRect(list.item(0)), c = list.visualItemRect(list.item(2));
    CHECK(a.size() == QSize(80, 70) && c.top() == a.top() && c.left() == a.right() + 1 + 4);
    CHECK(list.itemAt(c.center()) == list.item(2));
    list.item(0)->setSelected(true);
    list.clearSelection();
    CHECK(list.selectedItems().isEmpty());

    // QTableWidget: sorting, hiding, row sizes.
    QTableWidget table(3, 2);
    table.setItem(0, 0, new QTableWidgetItem("10"));
    table.setItem(1, 0, new QTableWidgetItem("9"));
    table.setItem(2, 0, new QTableWidgetItem("100"));
    table.setItem(0, 1, new QTableWidgetItem("ten"));
    table.setCellWidget(1, 1, new QLabel("nine"));
    table.setCurrentCell(0, 0);
    table.sortItems(0);
    CHECK(table.item(0, 0)->text() == "9" && table.item(1, 0)->text() == "10" && table.item(2, 0)->text() == "100");
    CHECK(table.item(1, 1)->text() == "ten" && table.currentRow() == 1);
    CHECK(qobject_cast<QLabel *>(table.cellWidget(0, 1)));
    table.setSortingEnabled(true);
    table.horizontalHeader()->setSortIndicator(0, Qt::DescendingOrder);
    CHECK(table.isSortingEnabled() && table.item(0, 0)->text() == "100");
    table.setRowHidden(1, true);
    CHECK(table.isRowHidden(1) && table.rowHeight(1) == 0);
    table.showRow(1);
    table.setWordWrap(true);
    table.item(0, 0)->setText("a long text that wraps over a few lines of the cell");
    table.resizeRowsToContents();
    CHECK(table.rowHeight(0) > table.rowHeight(2));
    table.selectRow(2);
    CHECK(table.currentRow() == 2);

    // QHeaderView: moving sections and saving the state.
    QHeaderView *header = table.horizontalHeader();
    header->setSectionsMovable(true);
    QSignalSpy moved(header, &QHeaderView::sectionMoved);
    header->moveSection(0, 1);
    CHECK(moved.count() == 1 && header->logicalIndex(0) == 1 && header->visualIndex(0) == 1);
    CHECK(header->sectionPosition(1) == 0 && header->sectionsMoved());
    const QByteArray state = header->saveState();
    header->moveSection(1, 0);
    header->resizeSection(1, 33);
    CHECK(!header->sectionsMoved());
    CHECK(header->restoreState(state) && header->visualIndex(0) == 1 && header->sectionSize(1) != 33);
    CHECK(!header->restoreState(QByteArray("junk")));

    // QTreeWidget: item widgets and the item iterator.
    QTreeWidget tree;
    tree.setColumnCount(2);
    auto *top = new QTreeWidgetItem(&tree, QStringList{"top"});
    auto *child = new QTreeWidgetItem(top, QStringList{"child"});
    child->setCheckState(0, Qt::Checked);
    new QTreeWidgetItem(&tree, QStringList{"other"});
    top->setExpanded(true);
    auto *progress = new QProgressBar;
    tree.setItemWidget(child, 1, progress);
    CHECK(tree.itemWidget(child, 1) == progress && progress->parentWidget() == tree.viewport());
    tree.resize(300, 200);
    tree.show();
    settle();
    tree.viewport()->repaint();
    CHECK(progress->isVisible() && progress->geometry() == tree.visualItemRect(child).intersected(
                                                                 tree.visualRect(tree.indexFromItem(child, 1))));
    QStringList order;
    for (QTreeWidgetItemIterator it(&tree); *it; ++it)
        order << (*it)->text(0);
    CHECK(order == QStringList({"top", "child", "other"}));
    QTreeWidgetItemIterator checked(&tree, QTreeWidgetItemIterator::Checked);
    CHECK(*checked == child && *(++checked) == nullptr);
    tree.removeItemWidget(child, 1);
    CHECK(tree.itemWidget(child, 1) == nullptr);

    // QGraphicsView: rubber band selection.
    QGraphicsScene scene(0, 0, 200, 200);
    auto *r1 = scene.addRect(10, 10, 20, 20);
    auto *r2 = scene.addRect(100, 100, 20, 20);
    r1->setFlag(QGraphicsItem::ItemIsSelectable);
    r2->setFlag(QGraphicsItem::ItemIsSelectable);
    QGraphicsView view(&scene);
    view.resize(260, 260);
    view.setDragMode(QGraphicsView::RubberBandDrag);
    view.setRubberBandSelectionMode(Qt::ContainsItemShape);
    CHECK(view.rubberBandSelectionMode() == Qt::ContainsItemShape);
    view.show();
    settle();
    QSignalSpy band(&view, &QGraphicsView::rubberBandChanged);
    const QPoint from = view.mapFromScene(QPointF(0, 0)), to = view.mapFromScene(QPointF(50, 50));
    const auto mouse = [&](QEvent::Type type, QPoint pos) {
        QMouseEvent e(type, QPointF(pos), QPointF(view.viewport()->mapToGlobal(pos)), Qt::LeftButton,
                      type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::MouseButtons(Qt::LeftButton),
                      Qt::NoModifier);
        QCoreApplication::sendEvent(view.viewport(), &e);
    };
    mouse(QEvent::MouseButtonPress, from);
    mouse(QEvent::MouseMove, to);
    CHECK(!view.rubberBandRect().isNull() && r1->isSelected() && !r2->isSelected());
    mouse(QEvent::MouseButtonRelease, to);
    CHECK(view.rubberBandRect().isNull() && band.count() == 2);
    QPainterPath path;
    path.addRect(90, 90, 40, 40);
    scene.setSelectionArea(path, Qt::AddToSelection, Qt::IntersectsItemShape);
    CHECK(r1->isSelected() && r2->isSelected());
}

static void testTextAndCoreTail() {
    // Editing a document in place, with a style sheet.
    QTextEdit edit;
    edit.document()->setDefaultStyleSheet(".error { color: #ff0000; font-weight: bold } pre code { font-style: italic }");
    CHECK(edit.document()->defaultStyleSheet().contains(".error"));
    edit.setHtml("<p>Hello</p>");
    QTextCursor cursor(edit.document());
    CHECK(!cursor.isNull());
    cursor.movePosition(QTextCursor::End);
    // As in Qt, a fragment of one paragraph goes on in the paragraph at the cursor.
    cursor.insertHtml("<br><span class=\"error\">Oops</span>");
    CHECK(edit.toPlainText() == "Hello\nOops");
    CHECK(edit.document()->toHtml().contains("#ff0000"));
    const int mark = cursor.position();
    cursor.insertHtml("<b>more</b>");
    CHECK(edit.toPlainText() == "Hello\nOopsmore");
    cursor.setPosition(mark);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    CHECK(cursor.selectedText() == "more");
    cursor.insertHtml("done");
    CHECK(edit.toPlainText() == "Hello\nOopsdone");
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    CHECK(edit.toPlainText() == "\nOopsdone" && cursor.blockNumber() == 0);
    // A fragment of several paragraphs makes new paragraphs.
    cursor.movePosition(QTextCursor::End);
    cursor.insertHtml("<p>one</p><p>two</p>");
    CHECK(edit.toPlainText() == "\nOopsdoneone\ntwo" && edit.document()->blockCount() == 2);
    QTextCursor editCursor = edit.textCursor();
    editCursor.movePosition(QTextCursor::End);
    edit.setTextCursor(editCursor);
    edit.insertHtml("<i>!</i>");
    CHECK(edit.toPlainText().endsWith("two!") && edit.document()->toHtml().contains("italic"));

    // Core and GUI odds and ends.
    QSet<QString> set{"a", "bb", "ccc"};
    CHECK(set.removeIf([](const QString &v) { return v.size() > 1; }) == 2 && set.contains("a"));
    const QRegion region = QRegion(QRect(0, 0, 10, 10)).subtracted(QRegion(QRect(5, 0, 5, 10)));
    CHECK(region.boundingRect() == QRect(0, 0, 5, 10));
    CHECK(QRegion(QRect(0, 0, 10, 10)).xored(QRegion(QRect(5, 0, 10, 10))).boundingRect() == QRect(0, 0, 15, 10));
    QPixmap pixmap(20, 20);
    pixmap.fill(Qt::blue);
    CHECK(pixmap.copy(2, 3, 4, 5).size() == QSize(4, 5));
    const QVariant colorVariant = QColor(Qt::green);
    CHECK(colorVariant.value<QColor>() == QColor(Qt::green));
    QVariant fontVariant = QFont("Sans", 12);
    CHECK(fontVariant.value<QFont>().pointSize() == 12);
    QPointer<QObject> pointer = new QLabel;
    CHECK(qobject_cast<QLabel *>(pointer) != nullptr);
    delete pointer.data();
    CHECK(qputenv(QByteArray("NT_LDM_TAIL"), "1") && qgetenv("NT_LDM_TAIL") == "1");
    qDebug() << QTransform().scale(2, 2);
    QSizePolicy policy;
    policy.setHeightForWidth(true);
    CHECK(policy.hasHeightForWidth());
    QScreen *screen = QGuiApplication::primaryScreen();
    CHECK(!screen || screen->virtualGeometry().contains(screen->geometry()));
    QInputMethod *im = QGuiApplication::inputMethod();
    QSignalSpy visible(im, &QInputMethod::visibleChanged);
    im->show();
    CHECK(im->isVisible() && visible.count() == 1);
    im->hide();
    CHECK(QRegularExpression(QRegularExpression::anchoredPattern(QLatin1String("a+"))).match("aaa").hasMatch());
    QGraphicsSceneMouseEvent sceneEvent(QEvent::GraphicsSceneMousePress);
    sceneEvent.setScreenPos(QPointF(3.4, 4.6));
    const QPoint screenPos = sceneEvent.screenPos();
    CHECK(screenPos == QPoint(3, 5));

    // Plain text editors: a document that follows the text, and a limit on the blocks.
    QPlainTextEdit console;
    console.setMaximumBlockCount(3);
    CHECK(console.maximumBlockCount() == 3);
    for (int i = 1; i <= 5; ++i)
        console.appendPlainText(QString("line %1").arg(i));
    CHECK(console.blockCount() == 3 && console.toPlainText().startsWith("line 3"));
    QTextCursor end(console.document());
    CHECK(console.document()->toPlainText() == console.toPlainText());
    end.movePosition(QTextCursor::End);
    end.insertText("!");
    console.setTextCursor(end);
    CHECK(console.toPlainText().endsWith("line 5!") && console.textCursor().position() == end.position());
    console.document()->setPlainText("reset");
    CHECK(console.toPlainText() == "reset");

    QMessageBox box;
    box.setTextFormat(Qt::PlainText);
    CHECK(box.textFormat() == Qt::PlainText);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    CHECK(QGuiApplication::highDpiScaleFactorRoundingPolicy() == Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    CHECK(qEnvironmentVariable(QByteArray("NT_LDM_TAIL")) == "1");
#ifndef QT_NO_SSL
    const QList<QSslCertificate> system = QSslConfiguration::systemCaCertificates();
    CHECK(system.isEmpty() || !system.first().isNull());
#endif

    // Network.
#ifndef QT_NO_SSL
    CHECK(QSslSocket::availableBackends().contains(QSslSocket::activeBackend()));
    CHECK(QSslSocket::setActiveBackend(QSslSocket::activeBackend()) && !QSslSocket::setActiveBackend("schannel"));
#endif
    QUdpSocket udp;
    CHECK(udp.bind(QHostAddress::AnyIPv4, 0));
    const QNetworkInterface loopback = QNetworkInterface::interfaceFromName("lo");
    if (loopback.isValid()) {
        udp.setMulticastInterface(loopback);
        CHECK(udp.multicastInterface().index() == loopback.index());
    }
    QWebSocket socket;
    int errors = 0;
    QObject::connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
                     [&errors](QAbstractSocket::SocketError) { ++errors; });
    socket.open(QUrl("ws://127.0.0.1:1"));
    QElapsedTimer clock;
    clock.start();
    while (errors == 0 && clock.elapsed() < 3000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    CHECK(errors == 1);
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QTemporaryDir dir;
    testKeys();
    testValues();
    testNetwork();
    testImages();
    testPainterWindow();
    testIcons(dir.path());
    testWidgets();
    testShortcutOverride();
    testContextMenus();
    testItemViews();
    testComboBox();
    testGraphicsAndSpies();
    testClipboard();
    testDragAndDrop();
    testValidators();
    testDesktopServices();
    testStandardItemModel();
    testComboBoxModel();
    testScroller();
    testGraphicsWidget();
    testMarkdownAndTables();
    testWidgetTail();
    testViewTail();
    testTextAndCoreTail();
    std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}

#include "test_ldm_gaps.moc" // NOLINT(bugprone-suspicious-include): generated by ntmoc
