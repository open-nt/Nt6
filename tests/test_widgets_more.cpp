// MIT License - see LICENSE
//
// Tests for more of Nt Widgets: the file system model and its icon provider, graphics layouts and transforms, and
// the smaller classes. The widgets are on the offscreen platform.
#include <QtCore/QtCore>
#include <QtGui/QtGui>
#include <QtTest/qsignalspy.h>
#include <QtTest/qtest.h>
#include <QtWidgets/QtWidgets>
#include <QtDBus/QtDBus>

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

static void write(const QString &path, const QByteArray &data) {
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
        file.write(data);
}

static void testFileSystemModel() {
    QTemporaryDir dir;
    const QString root = dir.path();
    QDir(root).mkdir("beta");
    QDir(root).mkdir("alpha");
    write(root + "/b.txt", "hello");
    write(root + "/a.png", QByteArray(2048, 'x'));
    write(root + "/.hidden", "h");

    QFileIconProvider icons;
    CHECK(!icons.icon(QAbstractFileIconProvider::Folder).isNull());
    CHECK(!icons.icon(QFileInfo(root + "/b.txt")).isNull());
    CHECK(icons.type(QFileInfo(root + "/a.png")) == "PNG File" && icons.type(QFileInfo(root + "/beta")) == "Folder");

    QFileSystemModel model;
    QSignalSpy rootChanged(&model, &QFileSystemModel::rootPathChanged);
    QSignalSpy loaded(&model, &QFileSystemModel::directoryLoaded);
    const QModelIndex top = model.setRootPath(root);
    CHECK(rootChanged.count() == 1 && model.rootPath() == QDir::cleanPath(root));
    CHECK(top.isValid() && model.filePath(top) == QDir::cleanPath(root) && model.isDir(top));
    CHECK(model.columnCount(top) == 4 && model.headerData(3, Qt::Horizontal).toString() == "Date Modified");
    // Folders first, then files, by name; the hidden file is left out.
    CHECK(model.rowCount(top) == 4);
    QStringList names;
    for (int r = 0; r < model.rowCount(top); ++r)
        names << model.index(r, 0, top).data().toString();
    CHECK(names == QStringList({"alpha", "beta", "a.png", "b.txt"}));
    const QModelIndex png = model.index(root + "/a.png");
    CHECK(png.isValid() && model.fileName(png) == "a.png" && model.size(png) == 2048 && !model.isDir(png));
    CHECK(model.type(png) == "PNG File" && png.siblingAtColumn(1).data().toString().contains("KiB"));
    CHECK(model.parent(png) == top && model.fileInfo(png).suffix() == "png");
    CHECK(png.data(QFileSystemModel::FilePathRole).toString() == QDir::cleanPath(root) + "/a.png");
    CHECK(!model.fileIcon(png).isNull() && model.lastModified(png).isValid());
    CHECK(model.hasChildren(model.index(root + "/alpha")) && !model.hasChildren(png));
    settle();
    CHECK(loaded.count() >= 1);

    // Name filters: files that do not match are shown disabled, or left out.
    model.setNameFilters({"*.txt"});
    CHECK(!(model.flags(model.index(root + "/a.png")) & Qt::ItemIsEnabled));
    CHECK(model.flags(model.index(root + "/b.txt")) & Qt::ItemIsEnabled);
    model.setNameFilterDisables(false);
    CHECK(!model.index(root + "/a.png").isValid() && model.rowCount(top) == 3);
    model.setNameFilters({});
    model.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    CHECK(model.rowCount(top) == 5);
    model.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);

    // Changes through the model, which is read-only by default.
    CHECK(!model.mkdir(top, "gamma").isValid());
    model.setReadOnly(false);
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
    const QModelIndex gamma = model.mkdir(top, "gamma");
    CHECK(gamma.isValid() && QDir(root + "/gamma").exists() && inserted.count() == 1);
    CHECK(gamma.row() == 2); // after alpha and beta
    const QPersistentModelIndex txt = model.index(root + "/b.txt");
    QSignalSpy renamed(&model, &QFileSystemModel::fileRenamed);
    CHECK(model.setData(txt, "0.txt"));
    CHECK(renamed.count() == 1 && QFile::exists(root + "/0.txt") && txt.data().toString() == "0.txt");
    CHECK(txt.row() == 3); // before a.png now
    CHECK(model.rmdir(gamma) && !QDir(root + "/gamma").exists());
    CHECK(model.remove(model.index(root + "/beta")) && !QDir(root + "/beta").exists());
    CHECK(model.rowCount(top) == 3);

    // Changes on disk are followed.
    write(root + "/z.txt", "z");
    QElapsedTimer clock;
    clock.start();
    while (!model.index(root + "/z.txt").isValid() && clock.elapsed() < 3000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    CHECK(model.index(root + "/z.txt").isValid());

    // Sorting by size, the largest first; folders stay first.
    model.sort(1, Qt::DescendingOrder);
    CHECK(model.index(0, 0, top).data().toString() == "alpha" && model.index(1, 0, top).data().toString() == "a.png");
    QMimeData *mime = model.mimeData({model.index(root + "/a.png")});
    CHECK(mime->urls().value(0) == QUrl::fromLocalFile(QDir::cleanPath(root) + "/a.png"));
    delete mime;

    // A tree view of the model.
    QTreeView view;
    view.setModel(&model);
    view.setRootIndex(model.index(root));
    view.resize(400, 300);
    view.show();
    settle();
    CHECK(view.indexAt(QPoint(5, 5)).isValid());
}

static QGraphicsWidget *box(qreal w, qreal h, QSizePolicy::Policy policy = QSizePolicy::Preferred) {
    auto *item = new QGraphicsWidget;
    item->setPreferredSize(w, h);
    item->setSizePolicy(policy, policy);
    return item;
}

struct EventWidget : QGraphicsWidget {
    QSizeF resized;
    QPointF moved;
    int moves = 0;
    void resizeEvent(QGraphicsSceneResizeEvent *e) override { resized = e->newSize(); }
    void moveEvent(QGraphicsSceneMoveEvent *e) override {
        moved = e->newPos();
        ++moves;
    }
};

static void testGraphicsLayoutsAndTransforms() {
    QGraphicsScene scene;
    auto *form = new QGraphicsWidget;
    scene.addItem(form);
    auto *grid = new QGraphicsGridLayout(form);
    CHECK(form->layout() == grid);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(4);
    QGraphicsWidget *a = box(40, 20), *b = box(60, 20), *c = box(30, 30, QSizePolicy::Fixed);
    grid->addItem(a, 0, 0);
    grid->addItem(b, 0, 1);
    grid->addItem(c, 1, 0, 1, 2, Qt::AlignHCenter);
    CHECK(grid->rowCount() == 2 && grid->columnCount() == 2 && grid->count() == 3);
    CHECK(grid->itemAt(1, 1) == c && grid->itemAt(0, 1) == b && a->parentItem() == form);
    form->adjustSize();
    CHECK(form->size() == QSizeF(104, 54));
    CHECK(a->geometry() == QRectF(0, 0, 40, 20) && b->geometry() == QRectF(44, 0, 60, 20));
    CHECK(c->geometry() == QRectF(37, 24, 30, 30)); // centred across both columns
    // Room beyond the preferred size goes to the column with a stretch factor.
    grid->setColumnStretchFactor(1, 1);
    form->resize(204, 54);
    CHECK(a->geometry().width() == 40 && b->geometry().width() == 160);
    grid->setColumnFixedWidth(0, 50);
    form->resize(204, 54);
    CHECK(a->geometry().width() == 50 && grid->columnMaximumWidth(0) == 50);
    grid->setRowSpacing(0, 10);
    form->resize(204, 60);
    CHECK(c->geometry().top() == 30 && grid->rowSpacing(0) == 10);
    grid->removeItem(c);
    CHECK(grid->count() == 2 && c->parentLayoutItem() == nullptr);
    delete c;

    // Resize and move events of a graphics widget.
    auto *events = new EventWidget;
    scene.addItem(events);
    events->setGeometry(QRectF(5, 6, 70, 30));
    CHECK(events->resized == QSizeF(70, 30) && events->moved == QPointF(5, 6));
    QSignalSpy geometry(events, &QGraphicsWidget::geometryChanged);
    events->setPos(10, 10);
    CHECK(events->moved == QPointF(10, 10) && geometry.count() == 1);

    // Transformations of items.
    auto *rect = scene.addRect(0, 0, 100, 50, QPen(Qt::NoPen));
    auto *scale = new QGraphicsScale(&scene);
    scale->setXScale(2);
    QSignalSpy scaleChanged(scale, &QGraphicsScale::scaleChanged);
    rect->setTransformations({scale});
    CHECK(rect->transformations().size() == 1);
    CHECK(rect->sceneBoundingRect() == QRectF(0, 0, 200, 50));
    scale->setYScale(3);
    CHECK(scaleChanged.count() == 1 && rect->sceneBoundingRect() == QRectF(0, 0, 200, 150));
    auto *rotation = new QGraphicsRotation(&scene);
    rotation->setAngle(90);
    rotation->setAxis(Qt::ZAxis);
    rect->setTransformations({rotation});
    const QPointF corner = rect->mapToScene(QPointF(100, 0));
    CHECK(qAbs(corner.x()) < 0.01 && qAbs(corner.y() - 100) < 0.01);
    rotation->setOrigin(QVector3D(50, 25, 0));
    rotation->setAxis(Qt::YAxis);
    rotation->setAngle(60);
    CHECK(rect->sceneBoundingRect().width() < 100 && rotation->axis() == QVector3D(0, 1, 0));
    delete rotation;
    CHECK(rect->transformations().isEmpty() && rect->sceneBoundingRect() == QRectF(0, 0, 100, 50));
    delete scale;
}

class Grip : public QSplitterHandle {
public:
    using QSplitterHandle::QSplitterHandle;
    void moveTo(int pos) { moveSplitter(pos); }
};

class GripSplitter : public QSplitter {
public:
    using QSplitter::QSplitter;

protected:
    QSplitterHandle *createHandle() override { return new Grip(orientation(), this); }
};

class Swatch : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString colorName READ colorName WRITE setColorName USER true)
public:
    explicit Swatch(QWidget *parent = nullptr) : QWidget(parent) {}
    QString colorName() const { return m_name; }
    void setColorName(const QString &name) { m_name = name; }

private:
    QString m_name;
};

static void testSmallerClasses() {
    // Splitter handles of one's own, and the state of a splitter.
    GripSplitter split;
    split.resize(300, 100);
    split.addWidget(new QWidget);
    split.addWidget(new QWidget);
    split.addWidget(new QWidget);
    split.show();
    settle();
    CHECK(split.handle(0) == nullptr && dynamic_cast<Grip *>(split.handle(1)));
    CHECK(split.handle(1)->splitter() == &split && split.handle(1)->orientation() == Qt::Horizontal);
    QSignalSpy moved(&split, &QSplitter::splitterMoved);
    static_cast<Grip *>(split.handle(1))->moveTo(50);
    CHECK(moved.count() == 1 && split.sizes().value(0) == 50);
    const QByteArray state = split.saveState();
    split.setSizes({100, 100, 100});
    CHECK(split.restoreState(state) && split.sizes().value(0) == 50 && !split.restoreState("bad"));
    auto *replacement = new QLabel("new");
    QWidget *old = split.replaceWidget(2, replacement);
    CHECK(old && old->parent() == nullptr && split.widget(2) == replacement);
    delete old;
    split.setOrientation(Qt::Vertical);
    CHECK(split.handle(1)->orientation() == Qt::Vertical);
    split.setOpaqueResize(false);
    CHECK(!split.opaqueResize() && !split.handle(1)->opaqueResize());

    // Style hint returns.
    QStyleHintReturnMask mask;
    mask.region = QRegion(0, 0, 4, 4);
    QStyleHintReturn *hint = &mask;
    CHECK(qstyleoption_cast<QStyleHintReturnMask *>(hint) == &mask);
    CHECK(qstyleoption_cast<QStyleHintReturnVariant *>(hint) == nullptr);
    QStyleHintReturnVariant variant;
    CHECK(variant.type == QStyleHintReturn::SH_Variant);

    // Selection ranges of a table.
    QTableWidget table(4, 3);
    table.setRangeSelected(QTableWidgetSelectionRange(1, 1, 2, 2), true);
    const QList<QTableWidgetSelectionRange> ranges = table.selectedRanges();
    CHECK(ranges.size() == 1 && ranges.first() == QTableWidgetSelectionRange(1, 1, 1, 1));
    CHECK(ranges.first().rowCount() == 1 && QTableWidgetSelectionRange(0, 0, 2, 1).columnCount() == 2);
    table.setRangeSelected(QTableWidgetSelectionRange(0, 0, 3, 2), false);
    CHECK(table.selectedRanges().isEmpty());

    // What's This mode.
    QWidget window;
    window.resize(200, 100);
    auto *field = new QLineEdit(&window);
    field->setWhatsThis("The port");
    window.show();
    settle();
    QAction *action = QWhatsThis::createAction(&window);
    action->trigger();
    CHECK(QWhatsThis::inWhatsThisMode());
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(5, 5), QPointF(field->mapToGlobal(QPoint(5, 5))),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(field, &press);
    CHECK(!QWhatsThis::inWhatsThisMode());
    QWhatsThis::hideText();

    // Editor factories.
    const QItemEditorFactory *standard = QItemEditorFactory::defaultFactory();
    QWidget host;
    QWidget *spin = standard->createEditor(QMetaType::Int, &host);
    CHECK(qobject_cast<QSpinBox *>(spin) && standard->valuePropertyName(QMetaType::Int) == "value");
    CHECK(qobject_cast<QLineEdit *>(standard->createEditor(QMetaType::QString, &host)));
    QItemEditorFactory factory;
    factory.registerEditor(QMetaType::QString, new QStandardItemEditorCreator<Swatch>);
    CHECK(qobject_cast<Swatch *>(factory.createEditor(QMetaType::QString, &host)));
    CHECK(factory.valuePropertyName(QMetaType::QString) == "colorName");
    factory.registerEditor(QMetaType::Int, new QItemEditorCreator<QLineEdit>("text"));
    CHECK(qobject_cast<QLineEdit *>(factory.createEditor(QMetaType::Int, &host)));
    QStandardItemModel model(1, 1);
    model.setData(model.index(0, 0), QStringLiteral("red"));
    QStyledItemDelegate delegate;
    delegate.setItemEditorFactory(&factory);
    CHECK(delegate.itemEditorFactory() == &factory);
    QWidget *editor = delegate.createEditor(&host, QStyleOptionViewItem(), model.index(0, 0));
    CHECK(qobject_cast<Swatch *>(editor));
    delegate.setEditorData(editor, model.index(0, 0));
    CHECK(static_cast<Swatch *>(editor)->colorName() == "red");
    static_cast<Swatch *>(editor)->setColorName("blue");
    delegate.setModelData(editor, &model, model.index(0, 0));
    CHECK(model.data(model.index(0, 0)).toString() == "blue");
}

static void testColumnView() {
    QStandardItemModel model;
    auto *fruit = new QStandardItem("Fruit");
    fruit->appendRow(new QStandardItem("Apple"));
    auto *citrus = new QStandardItem("Citrus");
    citrus->appendRow(new QStandardItem("Lemon"));
    fruit->appendRow(citrus);
    model.appendRow(fruit);
    model.appendRow(new QStandardItem("Bread"));

    QColumnView view;
    view.resize(500, 200);
    auto *preview = new QLabel("preview");
    view.setPreviewWidget(preview);
    CHECK(view.previewWidget() == preview);
    view.setModel(&model);
    view.show();
    settle();
    CHECK(view.columnWidths().size() == 1);
    QSignalSpy previewed(&view, &QColumnView::updatePreviewWidget);
    // Choosing an item with children opens a column with them.
    view.setCurrentIndex(model.indexFromItem(fruit));
    settle();
    CHECK(view.columnWidths().size() == 2);
    view.setCurrentIndex(model.indexFromItem(citrus->child(0)));
    settle();
    CHECK(view.columnWidths().size() == 3 && view.currentIndex() == model.indexFromItem(citrus->child(0)));
    CHECK(previewed.count() == 1 && preview->isVisible());
    CHECK(view.visualRect(model.indexFromItem(citrus)).isValid());
    const QModelIndex hit = view.indexAt(view.visualRect(model.indexFromItem(citrus)).center());
    CHECK(hit == model.indexFromItem(citrus));
    view.setColumnWidths({100, 120});
    CHECK(view.columnWidths().value(0) == 100 && view.columnWidths().value(2) == 200);
    // Going back up closes the deeper columns.
    view.setCurrentIndex(model.indexFromItem(model.item(1)));
    settle();
    CHECK(view.columnWidths().size() == 1);
}

static void testSystemTrayIcon() {
    QPixmap red(16, 16);
    red.fill(Qt::red);
    QSystemTrayIcon tray{QIcon(red)};
    QMenu menu;
    menu.addAction("Quit");
    tray.setContextMenu(&menu);
    tray.setToolTip("Radio link");
    CHECK(tray.contextMenu() == &menu && tray.toolTip() == "Radio link" && !tray.icon().isNull());
    CHECK(!tray.isVisible() && tray.geometry().isNull());
    (void)QSystemTrayIcon::isSystemTrayAvailable();
    (void)QSystemTrayIcon::supportsMessages();
    QSignalSpy activated(&tray, &QSystemTrayIcon::activated);
    tray.show();
    CHECK(tray.isVisible());
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        // The item is on the bus: read a property and activate it, as a desktop would.
        const QString prefix = QStringLiteral("org.kde.StatusNotifierItem-%1-").arg(QCoreApplication::applicationPid());
        QString service;
        for (const QString &name : bus.interface()->registeredServiceNames().value())
            if (name.startsWith(prefix))
                service = name;
        CHECK(!service.isEmpty());
        QDBusMessage get = QDBusMessage::createMethodCall(service, "/StatusNotifierItem",
                                                          "org.freedesktop.DBus.Properties", "Get");
        get.setArguments({QStringLiteral("org.kde.StatusNotifierItem"), QStringLiteral("Category")});
        QDBusPendingCallWatcher getWatcher(bus.asyncCall(get));
        QDBusMessage activate = QDBusMessage::createMethodCall(service, "/StatusNotifierItem",
                                                               "org.kde.StatusNotifierItem", "Activate");
        activate.setArguments({0, 0});
        QDBusPendingCallWatcher activateWatcher(bus.asyncCall(activate));
        QElapsedTimer clock;
        clock.start();
        while ((!getWatcher.isFinished() || activated.isEmpty()) && clock.elapsed() < 3000)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        const QList<QVariant> reply = getWatcher.reply().arguments();
        CHECK(!reply.isEmpty() && reply.first().value<QDBusVariant>().variant().toString() == "ApplicationStatus");
        CHECK(activated.count() == 1 &&
              activated.first().first().value<QSystemTrayIcon::ActivationReason>() == QSystemTrayIcon::Trigger);
    } else {
        std::printf("no session bus: the D-Bus part of the tray icon is not tested\n");
    }
    tray.hide();
    CHECK(!tray.isVisible());
}

static void testDrawUtilAndAnimation() {
    QImage image(40, 40, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPalette palette;
    palette.setColor(QPalette::Light, Qt::white);
    palette.setColor(QPalette::Dark, Qt::black);
    {
        QPainter p(&image);
        const QBrush fill(Qt::red);
        qDrawShadePanel(&p, QRect(0, 0, 20, 20), palette, true, 2, &fill);
        qDrawPlainRect(&p, 20, 20, 20, 20, Qt::blue, 1);
    }
    CHECK(image.pixel(0, 5) == qRgb(0, 0, 0) && image.pixel(19, 5) == qRgb(255, 255, 255));
    CHECK(image.pixel(10, 10) == qRgb(255, 0, 0) && image.pixel(20, 30) == qRgb(0, 0, 255));
    CHECK(image.pixel(30, 30) == qRgb(255, 255, 255));

    // A border pixmap: the corners stay, the edges and the middle fill the rest.
    QPixmap frame(9, 9);
    frame.fill(Qt::green);
    {
        QPainter p(&frame);
        p.fillRect(QRect(3, 3, 3, 3), Qt::yellow);
        p.fillRect(QRect(0, 0, 3, 3), Qt::magenta);
    }
    QImage target(30, 30, QImage::Format_ARGB32);
    target.fill(Qt::white);
    {
        QPainter p(&target);
        qDrawBorderPixmap(&p, target.rect(), QMargins(3, 3, 3, 3), frame, frame.rect(), QMargins(3, 3, 3, 3),
                          QTileRules(Qt::RepeatTile, Qt::StretchTile));
    }
    CHECK(target.pixel(1, 1) == qRgb(255, 0, 255) && target.pixel(15, 15) == qRgb(255, 255, 0));
    CHECK(target.pixel(15, 1) == qRgb(0, 255, 0) && target.pixel(4, 1) == qRgb(0, 255, 0));
    CHECK(QTileRules().horizontal == Qt::StretchTile && QTileRules(Qt::RoundTile).vertical == Qt::RoundTile);

    // An item animation, driven step by step.
    QGraphicsScene scene;
    auto *ball = scene.addEllipse(0, 0, 10, 10);
    QGraphicsItemAnimation animation;
    animation.setItem(ball);
    QTimeLine timeLine(1000);
    animation.setTimeLine(&timeLine);
    CHECK(animation.item() == ball && animation.timeLine() == &timeLine);
    animation.setPosAt(0, QPointF(0, 0));
    animation.setPosAt(1, QPointF(200, 100));
    animation.setRotationAt(1, 90);
    animation.setScaleAt(0.5, 2, 2);
    CHECK(animation.posList().size() == 2 && animation.rotationList().size() == 1);
    animation.setStep(0.5);
    CHECK(ball->pos() == QPointF(100, 50) && animation.rotationAt(0.5) == 90 && animation.horizontalScaleAt(0.5) == 2);
    CHECK(animation.verticalScaleAt(1) == 2 && animation.horizontalShearAt(0.3) == 0);
    animation.clear();
    CHECK(animation.posList().isEmpty());
}

// Records the shortcut and change events that reach a widget.
class EventLog : public QWidget {
public:
    using QWidget::QWidget;
    QStringList log;

protected:
    bool event(QEvent *e) override {
        if (e->type() == QEvent::Shortcut) {
            auto *s = static_cast<QShortcutEvent *>(e);
            log << QStringLiteral("shortcut %1 %2%3")
                       .arg(s->shortcutId())
                       .arg(s->key().toString())
                       .arg(s->isAmbiguous() ? QStringLiteral(" ambiguous") : QString());
            return true;
        }
        if (e->type() == QEvent::LayoutDirectionChange)
            log << QStringLiteral("direction");
        if (e->type() == QEvent::LocaleChange)
            log << QStringLiteral("locale");
        if (e->type() == QEvent::IconTextChange)
            log << QStringLiteral("icontext");
        return QWidget::event(e);
    }
};

static void testWidgetExtras() {
    // Font information and decorations
    QFont font(QStringLiteral("Sans Serif"), 12);
    font.setUnderline(true);
    font.setStrikeOut(true);
    QFont other = font;
    CHECK(other == font);
    other.setOverline(true);
    CHECK(other != font && other.overline());
    font.setStyle(QFont::StyleItalic);
    CHECK(font.italic() && font.style() == QFont::StyleItalic);
    QFontInfo info(font);
    CHECK(info.pixelSize() == 16 && info.pointSize() == 12 && info.underline() && info.strikeOut() && !info.overline());
    CHECK(info.italic() && info.styleName() == QStringLiteral("Italic") && info.family() == QStringLiteral("Sans Serif"));
    {
        QImage plain(60, 30, QImage::Format_ARGB32);
        plain.fill(Qt::white);
        QImage lined = plain;
        QFont f(QStringLiteral("Sans Serif"), 12);
        QPainter p1(&plain);
        p1.setFont(f);
        p1.drawText(QPoint(2, 20), QStringLiteral("  "));
        p1.end();
        f.setUnderline(true);
        QPainter p2(&lined);
        p2.setFont(f);
        p2.drawText(QPoint(2, 20), QStringLiteral("  "));
        p2.end();
        CHECK(plain != lined); // spaces draw nothing, their underline does
    }

    // Geometry
    QWidget top;
    top.resize(300, 200);
    auto *a = new QWidget(&top);
    a->setGeometry(10, 10, 50, 20);
    auto *b = new QWidget(&top);
    b->setGeometry(100, 50, 40, 40);
    CHECK(top.childrenRect() == QRect(10, 10, 130, 80));
    CHECK(top.childrenRegion().boundingRect() == QRect(10, 10, 130, 80));
    b->setHidden(true);
    CHECK(b->isHidden() && top.childrenRect() == QRect(10, 10, 50, 20));
    b->setHidden(false);
    top.scroll(5, -5);
    CHECK(a->pos() == QPoint(15, 5) && b->pos() == QPoint(105, 45));
    top.setBaseSize(10, 20);
    top.setSizeIncrement(QSize(8, 16));
    CHECK(top.baseSize() == QSize(10, 20) && top.sizeIncrement() == QSize(8, 16));
    CHECK(top.frameGeometry() == top.geometry() && top.frameSize() == top.size());
    CHECK(!top.hasHeightForWidth() && top.heightForWidth(100) == -1);
    top.setMask(QRegion(0, 0, 10, 10));
    CHECK(top.mask() == QRegion(0, 0, 10, 10));
    top.clearMask();
    CHECK(top.mask().isEmpty());
    CHECK(top.topLevelWidget() == &top && a->nativeParentWidget() == &top && !top.nativeParentWidget());

    // Saving and restoring the geometry
    top.setGeometry(40, 50, 320, 240);
    const QByteArray saved = top.saveGeometry();
    CHECK(saved.size() > 40);
    QWidget restored;
    CHECK(restored.restoreGeometry(saved) && restored.size() == QSize(320, 240));
    CHECK(!restored.restoreGeometry(QByteArray("nonsense")));

    // Stacking
    auto *c = new QWidget(&top);
    CHECK(top.children().indexOf(c) == 2);
    c->stackUnder(a);
    CHECK(top.children().indexOf(c) < top.children().indexOf(a));
    c->stackUnder(&top); // not a sibling
    CHECK(top.children().indexOf(c) == 0);

    // Enabled state
    auto *inner = new QWidget(a);
    a->setEnabled(false);
    CHECK(!inner->isEnabled() && inner->isEnabledTo(a) && !inner->isEnabledTo(&top));
    a->setEnabled(true);

    // Window properties
    QSignalSpy iconText(&top, &QWidget::windowIconTextChanged);
    QSignalSpy icon(&top, &QWidget::windowIconChanged);
    top.setWindowIconText(QStringLiteral("Doc"));
    top.setWindowIconText(QStringLiteral("Doc"));
    CHECK(iconText.count() == 1 && top.windowIconText() == QStringLiteral("Doc"));
    top.setWindowIcon(QIcon());
    CHECK(icon.count() == 1);
    top.setWindowOpacity(1.5);
    CHECK(top.windowOpacity() == 1.0);
    top.setWindowOpacity(0.25);
    CHECK(a->windowOpacity() == 0.25);
    top.setWindowFilePath(QStringLiteral("/tmp/notes.txt"));
    top.setWindowRole(QStringLiteral("editor"));
    CHECK(top.windowFilePath() == QStringLiteral("/tmp/notes.txt") && top.windowRole() == QStringLiteral("editor"));
    top.setToolTipDuration(1500);
    top.setAccessibleIdentifier(QStringLiteral("main"));
    CHECK(top.toolTipDuration() == 1500 && top.accessibleIdentifier() == QStringLiteral("main"));
    top.setTabletTracking(true);
    CHECK(top.hasTabletTracking() && top.property("tabletTracking").toBool());

    // Layout direction and locale follow the parent
    EventLog parent;
    auto *child = new EventLog(&parent);
    CHECK(child->layoutDirection() == Qt::LeftToRight && child->isLeftToRight());
    parent.setLayoutDirection(Qt::RightToLeft);
    CHECK(child->isRightToLeft() && child->log == QStringList{QStringLiteral("direction")});
    child->setLayoutDirection(Qt::LeftToRight);
    parent.unsetLayoutDirection();
    CHECK(parent.isLeftToRight() && child->isLeftToRight());
    child->log.clear();
    parent.setLocale(QLocale(QLocale::German, QLocale::Germany));
    CHECK(child->locale().language() == QLocale::German && child->log == QStringList{QStringLiteral("locale")});
    parent.unsetLocale();
    CHECK(child->locale() == QLocale());
    child->log.clear();
    child->setWindowIconText(QStringLiteral("x"));
    CHECK(child->log == QStringList{QStringLiteral("icontext")});

    // Palette roles
    CHECK(child->backgroundRole() == QPalette::Window && child->foregroundRole() == QPalette::WindowText);
    child->setBackgroundRole(QPalette::Base);
    CHECK(child->foregroundRole() == QPalette::Text);
    child->setForegroundRole(QPalette::Highlight);
    CHECK(child->foregroundRole() == QPalette::Highlight);

    // Input method
    CHECK(!top.inputMethodQuery(Qt::ImEnabled).toBool());
    top.setAttribute(Qt::WA_InputMethodEnabled);
    CHECK(top.inputMethodQuery(Qt::ImEnabled).toBool() && !top.inputMethodQuery(Qt::ImFont).isValid());
}

static void testFocusExtras() {
    EventLog top;
    top.resize(200, 200);
    auto *first = new QLineEdit(&top);
    auto *second = new QLineEdit(&top);
    auto *third = new QLineEdit(&top);
    auto *label = new QLabel(QStringLiteral("x"), &top);
    top.show();
    settle();
    top.activateWindow();
    settle();
    CHECK(first->nextInFocusChain() == second && second->previousInFocusChain() == first);
    CHECK(label->nextInFocusChain() == &top && top.previousInFocusChain() == label);
    QWidget::setTabOrder(first, third);
    CHECK(first->nextInFocusChain() == third && third->nextInFocusChain() == second);
    first->setFocus();
    CHECK(first->focusNextChild() && third->hasFocus());

    // A focus proxy takes the focus for the widget
    label->setFocusProxy(second);
    CHECK(label->focusProxy() == second);
    label->setFocus();
    CHECK(second->hasFocus() && label->hasFocus());
    second->setFocusProxy(label); // a loop is refused
    CHECK(!second->focusProxy());
    label->setFocusProxy(nullptr);
    second->setEditFocus(true);
    CHECK(second->hasEditFocus());

    // Grabbed shortcuts arrive as QShortcutEvent
    QWindow *win = top.windowHandle();
    const int id = top.grabShortcut(QKeySequence(QStringLiteral("Ctrl+K")));
    CHECK(id > 0);
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyPress, Qt::Key_K, Qt::ControlModifier, QStringLiteral("k"));
    settle();
    CHECK(top.log == QStringList{QStringLiteral("shortcut %1 Ctrl+K").arg(id)});
    top.log.clear();
    top.setShortcutEnabled(id, false);
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyPress, Qt::Key_K, Qt::ControlModifier, QStringLiteral("k"));
    settle();
    CHECK(top.log.isEmpty());
    top.setShortcutEnabled(id);
    top.setShortcutAutoRepeat(id, false);
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyPress, Qt::Key_K, Qt::ControlModifier, QStringLiteral("k"),
                                           true);
    settle();
    CHECK(top.log.isEmpty());
    const int second_id = top.grabShortcut(QKeySequence(QStringLiteral("Ctrl+K")), Qt::ApplicationShortcut);
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyPress, Qt::Key_K, Qt::ControlModifier, QStringLiteral("k"));
    settle();
    CHECK(top.log == QStringList{QStringLiteral("shortcut %1 Ctrl+K ambiguous").arg(id)});
    top.log.clear();
    top.releaseShortcut(id);
    top.releaseShortcut(second_id);
    QWindowSystemInterface::handleKeyEvent(win, QEvent::KeyPress, Qt::Key_K, Qt::ControlModifier, QStringLiteral("k"));
    settle();
    CHECK(top.log.isEmpty());
    CHECK(QWidget::find(top.winId()) == &top && top.effectiveWinId() == top.winId());
}

static void testLineEditExtras() {
    QLineEdit edit;
    QSignalSpy edited(&edit, &QLineEdit::textEdited);
    edit.setText(QStringLiteral("hello world"));
    CHECK(!edit.isModified() && !edit.isUndoAvailable());
    edit.setSelection(0, 5);
    CHECK(edit.selectionLength() == 5);
    edit.copy();
    CHECK(QGuiApplication::clipboard()->text() == QStringLiteral("hello"));
    edit.cut();
    CHECK(edit.text() == QStringLiteral(" world") && edit.isModified() && edit.isUndoAvailable());
    edit.end(false);
    edit.paste();
    CHECK(edit.text() == QStringLiteral(" worldhello"));
    edit.undo();
    CHECK(edit.text() == QStringLiteral(" world") && edit.isRedoAvailable());
    edit.undo();
    CHECK(edit.text() == QStringLiteral("hello world") && !edit.isUndoAvailable());
    edit.redo();
    edit.redo();
    CHECK(edit.text() == QStringLiteral(" worldhello") && !edit.isRedoAvailable());
    edit.clear();
    CHECK(edit.text().isEmpty() && edit.isUndoAvailable());
    edit.undo();
    CHECK(edit.text() == QStringLiteral(" worldhello"));
    edit.setModified(false);
    CHECK(!edit.isModified());
    QGuiApplication::clipboard()->setText(QStringLiteral("a\nb"));
    edit.setText(QString());
    edit.paste();
    CHECK(edit.text() == QStringLiteral("a b"));

    // Password mode does not copy
    QGuiApplication::clipboard()->setText(QStringLiteral("before"));
    edit.setEchoMode(QLineEdit::Password);
    edit.selectAll();
    edit.copy();
    CHECK(QGuiApplication::clipboard()->text() == QStringLiteral("before"));
    edit.setEchoMode(QLineEdit::Normal);

    // Cursor movement
    edit.setText(QStringLiteral("one two three"));
    edit.home(false);
    edit.cursorForward(false, 2);
    CHECK(edit.cursorPosition() == 2);
    edit.cursorWordForward(true);
    CHECK(edit.cursorPosition() == 3 && edit.selectedText() == QStringLiteral("e"));
    edit.cursorWordForward(false);
    CHECK(edit.cursorPosition() == 7);
    edit.cursorWordBackward(false);
    CHECK(edit.cursorPosition() == 4);
    edit.cursorBackward(true, 3);
    CHECK(edit.cursorPosition() == 1 && edit.selectionLength() == 3);

    // Alignment and margins
    edit.resize(200, 30);
    edit.setText(QStringLiteral("x"));
    edit.setAlignment(Qt::AlignRight);
    CHECK(edit.alignment() == Qt::AlignRight);
    edit.setCursorPosition(0);
    QLineEdit left(QStringLiteral("x"));
    left.resize(200, 30);
    left.setCursorPosition(0);
    // Right aligned, the text sits at the right end: a point at the left is before it
    CHECK(edit.cursorPositionAt(QPoint(20, 15)) == 0 && left.cursorPositionAt(QPoint(20, 15)) == 1);
    CHECK(edit.cursorPositionAt(QPoint(199, 15)) == 1 && edit.inputMethodQuery(Qt::ImCursorRectangle).toRect().left() > 150);
    const QSize before = left.sizeHint();
    left.setTextMargins(4, 2, 6, 3);
    CHECK(left.textMargins() == QMargins(4, 2, 6, 3) && left.sizeHint() == before + QSize(10, 5));
    edit.setDragEnabled(true);
    edit.setCursorMoveStyle(Qt::VisualMoveStyle);
    CHECK(edit.dragEnabled() && edit.cursorMoveStyle() == Qt::VisualMoveStyle);

    // Input masks
    QLineEdit masked;
    masked.setInputMask(QStringLiteral("9999-99-99"));
    CHECK(masked.inputMask() == QStringLiteral("9999-99-99") && masked.displayText() == QStringLiteral("    -  -  "));
    CHECK(masked.text() == QStringLiteral("--") && !masked.hasAcceptableInput());
    masked.insert(QStringLiteral("2026x1118"));
    CHECK(masked.text() == QStringLiteral("2026-11-18") && masked.hasAcceptableInput());
    masked.setCursorPosition(5);
    masked.backspace();
    CHECK(masked.displayText() == QStringLiteral("202 -11-18") && masked.cursorPosition() == 3);
    CHECK(!masked.hasAcceptableInput());
    masked.setText(QStringLiteral("19991231"));
    CHECK(masked.text() == QStringLiteral("1999-12-31"));
    masked.setInputMask(QStringLiteral(">AAA;_"));
    CHECK(masked.displayText() == QStringLiteral("___"));
    masked.insert(QStringLiteral("ab"));
    CHECK(masked.text() == QStringLiteral("AB") && masked.displayText() == QStringLiteral("AB_"));
    masked.setInputMask(QString());
    CHECK(masked.text() == QStringLiteral("AB") && masked.inputMask().isEmpty());

    // Keys
    QLineEdit keys(QStringLiteral("abc"));
    keys.selectAll();
    QKeyEvent copyKey(QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier);
    QCoreApplication::sendEvent(&keys, &copyKey);
    CHECK(QGuiApplication::clipboard()->text() == QStringLiteral("abc"));
    QKeyEvent cutKey(QEvent::KeyPress, Qt::Key_X, Qt::ControlModifier);
    QCoreApplication::sendEvent(&keys, &cutKey);
    CHECK(keys.text().isEmpty());
    QKeyEvent undoKey(QEvent::KeyPress, Qt::Key_Z, Qt::ControlModifier);
    QCoreApplication::sendEvent(&keys, &undoKey);
    CHECK(keys.text() == QStringLiteral("abc"));

    // The context menu
    keys.setSelection(0, 1);
    QMenu *menu = keys.createStandardContextMenu();
    QStringList texts;
    for (QAction *a : menu->actions())
        if (!a->isSeparator())
            texts << a->text() + (a->isEnabled() ? QStringLiteral("+") : QStringLiteral("-"));
    CHECK(texts == (QStringList{QStringLiteral("&Undo-"), QStringLiteral("&Redo+"), QStringLiteral("Cu&t+"),
                                QStringLiteral("&Copy+"), QStringLiteral("&Paste+"), QStringLiteral("Delete+"),
                                QStringLiteral("Select All+")}));
    delete menu;
    keys.setReadOnly(true);
    menu = keys.createStandardContextMenu();
    CHECK(menu->actions().size() == 3);
    delete menu;
    CHECK(keys.inputMethodQuery(Qt::ImSurroundingText).toString() == QStringLiteral("abc"));
    CHECK(keys.inputMethodQuery(Qt::ImCursorPosition).toInt() == 1 && !keys.inputMethodQuery(Qt::ImEnabled).toBool());
}

static void testTabsAndMainWindow() {
    // Tab bar: icons, shapes, visibility, colors
    QTabBar bar;
    QPixmap red(16, 16);
    red.fill(Qt::red);
    const int first = bar.addTab(QIcon(red), QStringLiteral("First"));
    bar.addTab(QStringLiteral("Second"));
    bar.addTab(QStringLiteral("Third"));
    CHECK(!bar.tabIcon(first).isNull() && bar.tabIcon(1).isNull());
    const QSize horizontal = bar.sizeHint();
    bar.setShape(QTabBar::RoundedWest);
    CHECK(bar.shape() == QTabBar::RoundedWest && bar.sizeHint() == horizontal.transposed());
    bar.resize(bar.sizeHint());
    CHECK(bar.tabRect(1).top() == bar.tabRect(0).bottom() + 1 && bar.tabRect(0).width() == bar.width());
    CHECK(bar.tabAt(bar.tabRect(2).center()) == 2);
    bar.setShape(QTabBar::RoundedNorth);
    bar.resize(bar.sizeHint());
    bar.setTabVisible(1, false);
    CHECK(!bar.isTabVisible(1) && bar.tabRect(1).width() == 0 && bar.tabRect(2).left() == bar.tabRect(0).right() + 1);
    bar.setCurrentIndex(1);
    bar.setTabVisible(1, true);
    bar.setCurrentIndex(1);
    bar.setTabVisible(1, false);
    CHECK(bar.currentIndex() == 2);
    bar.setTabVisible(1, true);
    bar.setTabTextColor(0, Qt::blue);
    bar.setTabWhatsThis(0, QStringLiteral("help"));
    bar.setAccessibleTabName(0, QStringLiteral("first tab"));
    CHECK(bar.tabTextColor(0) == QColor(Qt::blue) && bar.tabWhatsThis(0) == QStringLiteral("help") &&
          bar.accessibleTabName(0) == QStringLiteral("first tab"));
    bar.setIconSize(QSize(24, 24));
    CHECK(bar.iconSize() == QSize(24, 24));
    // Eliding shortens the tabs to fit
    bar.resize(bar.sizeHint().width() / 2, bar.sizeHint().height());
    bar.setElideMode(Qt::ElideRight);
    CHECK(bar.tabRect(2).right() < bar.sizeHint().width() - 1);
    {
        QTabBar hiding;
        hiding.setAutoHide(true);
        hiding.addTab(QStringLiteral("one"));
        CHECK(hiding.isHidden());
        hiding.addTab(QStringLiteral("two"));
        CHECK(!hiding.isHidden());
    }

    // Tab widget: positions, corners, icons
    QTabWidget tabs;
    tabs.resize(300, 200);
    auto *one = new QWidget;
    auto *two = new QWidget;
    tabs.addTab(one, QIcon(red), QStringLiteral("One"));
    tabs.addTab(two, QStringLiteral("Two"));
    CHECK(!tabs.tabIcon(0).isNull());
    CHECK(tabs.tabBar()->geometry().top() == 0 && one->geometry().top() > 0);
    tabs.setTabPosition(QTabWidget::South);
    CHECK(tabs.tabPosition() == QTabWidget::South && tabs.tabBar()->shape() == QTabBar::RoundedSouth);
    CHECK(tabs.tabBar()->geometry().bottom() == 199 && one->geometry().top() == 1);
    tabs.setTabPosition(QTabWidget::West);
    tabs.setTabShape(QTabWidget::Triangular);
    CHECK(tabs.tabBar()->shape() == QTabBar::TriangularWest && tabs.tabBar()->geometry().left() == 0);
    CHECK(one->geometry().left() > tabs.tabBar()->width() - 2);
    tabs.setTabPosition(QTabWidget::North);
    auto *corner = new QWidget;
    corner->setFixedSize(30, 20);
    tabs.setCornerWidget(corner);
    CHECK(tabs.cornerWidget() == corner && corner->geometry().right() == 299 && tabs.tabBar()->width() == 270);
    tabs.setTabVisible(1, false);
    CHECK(!tabs.isTabVisible(1));
    tabs.setTabBarAutoHide(true);
    CHECK(tabs.tabBarAutoHide() && !tabs.tabBar()->isHidden());
    tabs.setElideMode(Qt::ElideMiddle);
    tabs.setUsesScrollButtons(false);
    CHECK(tabs.elideMode() == Qt::ElideMiddle && !tabs.usesScrollButtons());

    // Main window: tabbed dock widgets and the saved state
    QMainWindow window;
    window.setCentralWidget(new QWidget);
    auto *left = new QDockWidget(QStringLiteral("Left"));
    left->setObjectName(QStringLiteral("left"));
    auto *other = new QDockWidget(QStringLiteral("Other"));
    other->setObjectName(QStringLiteral("other"));
    auto *right = new QDockWidget(QStringLiteral("Right"));
    right->setObjectName(QStringLiteral("right"));
    auto *tools = new QToolBar(QStringLiteral("Tools"));
    tools->setObjectName(QStringLiteral("tools"));
    window.addToolBar(tools);
    window.addDockWidget(Qt::LeftDockWidgetArea, left);
    window.addDockWidget(Qt::RightDockWidgetArea, other);
    window.addDockWidget(Qt::RightDockWidgetArea, right);
    window.resize(600, 400);
    window.show();
    settle();
    QSignalSpy activated(&window, &QMainWindow::tabifiedDockWidgetActivated);
    window.tabifyDockWidget(left, other);
    settle();
    CHECK(window.dockWidgetArea(other) == Qt::LeftDockWidgetArea);
    CHECK(window.tabifiedDockWidgets(left) == QList<QDockWidget *>{other});
    CHECK(window.tabifiedDockWidgets(other) == QList<QDockWidget *>{left});
    CHECK(other->width() > 0 && left->width() == 0);
    QTabBar *dockTabs = window.findChild<QTabBar *>(QStringLiteral("qt_dockwidget_tabbar"));
    CHECK(dockTabs && dockTabs->count() == 2 && dockTabs->tabText(0) == QStringLiteral("Left") &&
          dockTabs->currentIndex() == 1);
    dockTabs->setCurrentIndex(0);
    settle();
    CHECK(activated.count() == 1 && left->width() > 0 && other->width() == 0);

    const QByteArray state = window.saveState(3);
    window.removeDockWidget(other);
    CHECK(window.tabifiedDockWidgets(left).isEmpty());
    window.addDockWidget(Qt::BottomDockWidgetArea, other);
    window.addToolBar(Qt::LeftToolBarArea, tools);
    CHECK(!window.restoreState(state, 4) && !window.restoreState(QByteArray("junk")));
    CHECK(window.restoreState(state, 3));
    settle();
    CHECK(window.dockWidgetArea(other) == Qt::LeftDockWidgetArea && window.toolBarArea(tools) == Qt::TopToolBarArea);
    CHECK(window.tabifiedDockWidgets(left) == QList<QDockWidget *>{other} && left->width() > 0);

    window.splitDockWidget(left, right, Qt::Vertical);
    CHECK(window.dockWidgetArea(right) == Qt::LeftDockWidgetArea && window.tabifiedDockWidgets(right).isEmpty());
    delete other;
    settle();
    CHECK(window.tabifiedDockWidgets(left).isEmpty());

    window.setDockNestingEnabled(true);
    window.setAnimated(false);
    CHECK(window.isDockNestingEnabled() && !window.isAnimated() &&
          window.dockOptions() == (QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks));
    window.setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    CHECK(window.corner(Qt::BottomLeftCorner) == Qt::LeftDockWidgetArea &&
          window.corner(Qt::TopLeftCorner) == Qt::TopDockWidgetArea);
    window.setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);
    CHECK(window.tabPosition(Qt::LeftDockWidgetArea) == QTabWidget::North &&
          window.tabPosition(Qt::RightDockWidgetArea) == QTabWidget::South);
    window.setDocumentMode(true);
    window.setTabShape(QTabWidget::Triangular);
    CHECK(window.documentMode() && window.tabShape() == QTabWidget::Triangular && !window.isSeparator(QPoint(1, 1)));
}

static void testMenuBarCornersAndTreeAnimation() {
    QTreeView tree;
    CHECK(!tree.isAnimated());
    tree.setAnimated(true);
    CHECK(tree.isAnimated() && tree.property("animated").toBool());

    QMenuBar bar;
    bar.addMenu(QStringLiteral("&File"));
    const QSize plain = bar.sizeHint();
    auto *right = new QWidget;
    right->setFixedSize(40, plain.height() + 10);
    bar.setCornerWidget(right);
    CHECK(bar.cornerWidget() == right && bar.cornerWidget(Qt::TopRightCorner) == right && right->parentWidget() == &bar);
    CHECK(bar.sizeHint() == QSize(plain.width() + 40, plain.height() + 10));
    auto *left = new QWidget;
    left->setFixedSize(30, 10);
    bar.setCornerWidget(left, Qt::TopLeftCorner);
    CHECK(bar.cornerWidget(Qt::TopLeftCorner) == left && bar.sizeHint().width() == plain.width() + 70);
    bar.resize(300, bar.sizeHint().height());
    CHECK(right->geometry().right() == 299 && left->x() == 0);
    CHECK(bar.actionGeometry(bar.actions().first()).left() >= 30);
    bar.setCornerWidget(nullptr);
    CHECK(!bar.cornerWidget() && right->isHidden() && bar.sizeHint().width() == plain.width() + 30);
}

// Exposes the protected block geometry of a plain text editor.
class GutterEdit : public QPlainTextEdit {
public:
    using QPlainTextEdit::blockBoundingGeometry;
    using QPlainTextEdit::blockBoundingRect;
    using QPlainTextEdit::contentOffset;
    using QPlainTextEdit::firstVisibleBlock;
    using QPlainTextEdit::getPaintContext;
    using QPlainTextEdit::QPlainTextEdit;
};

static void testTextEditExtras() {
    GutterEdit edit;
    edit.resize(300, 200);
    QSignalSpy copyable(&edit, &QPlainTextEdit::copyAvailable);
    QSignalSpy modified(&edit, &QPlainTextEdit::modificationChanged);
    edit.setPlainText(QStringLiteral("alpha beta\ngamma"));
    CHECK(!edit.document()->isModified() && modified.isEmpty());
    QTextCursor c = edit.textCursor();
    c.setPosition(0);
    c.setPosition(5, QTextCursor::KeepAnchor);
    edit.setTextCursor(c);
    CHECK(copyable.count() == 1 && copyable.last().at(0).toBool());
    edit.copy();
    CHECK(QGuiApplication::clipboard()->text() == QStringLiteral("alpha") && edit.canPaste());
    edit.cut();
    CHECK(edit.toPlainText() == QStringLiteral(" beta\ngamma") && edit.document()->isModified());
    CHECK(modified.count() == 1 && copyable.last().at(0).toBool() == false);
    edit.moveCursor(QTextCursor::End);
    edit.paste();
    CHECK(edit.toPlainText() == QStringLiteral(" beta\ngammaalpha"));
    edit.document()->setModified(false);
    CHECK(modified.count() == 2 && !modified.last().at(0).toBool());
    edit.setReadOnly(true);
    CHECK(!edit.canPaste() && !(edit.textInteractionFlags() & Qt::TextEditable));
    edit.setTextInteractionFlags(Qt::TextEditorInteraction);
    CHECK(!edit.isReadOnly());

    // Overwrite mode replaces up to the end of the line
    edit.setPlainText(QStringLiteral("abc\ndef"));
    edit.setOverwriteMode(true);
    edit.moveCursor(QTextCursor::Start);
    edit.moveCursor(QTextCursor::Right);
    QKeyEvent x(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, QStringLiteral("XYZ"));
    QCoreApplication::sendEvent(&edit, &x);
    CHECK(edit.toPlainText() == QStringLiteral("aXYZ\ndef"));
    edit.setOverwriteMode(false);

    // Block geometry for a line number area
    edit.setPlainText(QStringLiteral("one\ntwo\nthree"));
    QTextBlock block = edit.firstVisibleBlock();
    CHECK(block.isValid() && block.blockNumber() == 0 && block.isVisible());
    const QRectF first = edit.blockBoundingGeometry(block);
    const QRectF second = edit.blockBoundingGeometry(block.next());
    CHECK(second.top() == first.bottom() && edit.blockBoundingRect(block).topLeft() == QPointF(0, 0));
    CHECK(edit.contentOffset() == QPointF(0, 0) && edit.getPaintContext().cursorPosition == 0);

    // Wrapping at a fixed width
    edit.setPlainText(QStringLiteral("aaaa bbbb cccc dddd"));
    edit.setLineWrapMode(QPlainTextEdit::FixedColumnWidth);
    edit.setLineWrapColumnOrWidth(5);
    const QRectF wrapped = edit.blockBoundingGeometry(edit.firstVisibleBlock());
    CHECK(wrapped.height() >= 3 * edit.fontMetrics().height());
    edit.setWordWrapMode(QTextOption::NoWrap);
    CHECK(edit.blockBoundingGeometry(edit.firstVisibleBlock()).height() == edit.fontMetrics().height());
    edit.setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    edit.setLineWrapMode(QPlainTextEdit::WidgetWidth);

    // Extra selections, zoom, cursor width, menu
    QPlainTextEdit::ExtraSelection line;
    line.format.setBackground(QColor(Qt::yellow));
    line.format.setProperty(QTextFormat::FullWidthSelection, true);
    line.cursor = edit.textCursor();
    edit.setExtraSelections({line});
    CHECK(edit.extraSelections().size() == 1);
    edit.show();
    const QImage shot = edit.grab().toImage();
    CHECK(shot.pixelColor(edit.viewport()->width() - 5, edit.viewport()->y() + 6) == QColor(Qt::yellow));
    edit.hide();
    const qreal size = edit.font().pointSizeF();
    edit.zoomIn(2);
    CHECK(edit.font().pointSizeF() == size + 2);
    edit.zoomOut(2);
    CHECK(edit.font().pointSizeF() == size);
    edit.setCursorWidth(3);
    CHECK(edit.cursorWidth() == 3);
    QMenu *menu = edit.createStandardContextMenu();
    CHECK(menu->actions().size() == 9);
    delete menu;
    edit.appendHtml(QStringLiteral("<b>bold</b>"));
    CHECK(edit.toPlainText().endsWith(QStringLiteral("\nbold")));
    QSignalSpy requests(&edit, &QPlainTextEdit::updateRequest);
    edit.verticalScrollBar()->setValue(0);
    edit.show();
    edit.repaint();
    CHECK(!requests.isEmpty());
    edit.hide();

    // QTextEdit keeps a current format
    QTextEdit rich;
    QSignalSpy formats(&rich, &QTextEdit::currentCharFormatChanged);
    rich.setFontWeight(QFont::Bold);
    rich.setFontItalic(true);
    rich.setFontPointSize(20);
    rich.setTextColor(Qt::red);
    rich.setFontFamily(QStringLiteral("Serif"));
    CHECK(formats.count() == 5 && rich.fontWeight() == QFont::Bold && rich.fontItalic() && rich.fontPointSize() == 20);
    CHECK(rich.textColor() == QColor(Qt::red) && rich.fontFamily() == QStringLiteral("Serif"));
    CHECK(!rich.textBackgroundColor().isValid() && !rich.fontUnderline());
    rich.setAlignment(Qt::AlignHCenter);
    CHECK(rich.alignment() == Qt::AlignHCenter);
    rich.setAutoFormatting(QTextEdit::AutoAll);
    CHECK(rich.autoFormatting() == QTextEdit::AutoAll);
    QTextEdit::ExtraSelection viaTextEdit;
    viaTextEdit.cursor = rich.textCursor();
    rich.setExtraSelections({viaTextEdit});
    rich.setPlainText(QStringLiteral("x"));
    rich.selectAll();
    rich.cut();
    CHECK(rich.document()->isModified() && rich.toPlainText().isEmpty());
}

static void testApplicationAndMenuExtras() {
    auto *app = qobject_cast<QApplication *>(QCoreApplication::instance());
    QApplication::setDoubleClickInterval(250);
    QApplication::setCursorFlashTime(800);
    QApplication::setKeyboardInputInterval(300);
    QApplication::setWheelScrollLines(5);
    CHECK(QApplication::doubleClickInterval() == 250 &&
          QGuiApplication::styleHints()->mouseDoubleClickInterval() == 250);
    CHECK(QApplication::cursorFlashTime() == 800 && QApplication::keyboardInputInterval() == 300 &&
          QApplication::wheelScrollLines() == 5);
    QApplication::setDoubleClickInterval(400);
    CHECK(!QApplication::isEffectEnabled(Qt::UI_FadeMenu));
    QApplication::setEffectEnabled(Qt::UI_FadeMenu);
    CHECK(QApplication::isEffectEnabled(Qt::UI_FadeMenu) && !QApplication::isEffectEnabled(Qt::UI_AnimateCombo));
    QApplication::setEffectEnabled(Qt::UI_General);
    CHECK(QApplication::isEffectEnabled(Qt::UI_AnimateCombo));
    QApplication::setEffectEnabled(Qt::UI_General, false);
    QApplication::setNavigationMode(Qt::NavigationModeKeypadTabOrder);
    CHECK(QApplication::navigationMode() == Qt::NavigationModeKeypadTabOrder);
    QApplication::setNavigationMode(Qt::NavigationModeNone);
    app->setStyleSheet(QStringLiteral("QLabel { color: red }"));
    CHECK(app->styleSheet() == QStringLiteral("QLabel { color: red }") && app->autoSipEnabled());
    app->setStyleSheet(QString());
    QApplication::beep();

    // focusChanged follows the focus
    QWidget top;
    auto *a = new QLineEdit(&top);
    auto *b = new QLineEdit(&top);
    top.setGeometry(50, 60, 200, 100);
    top.show();
    settle();
    top.activateWindow();
    settle();
    QSignalSpy focus(app, &QApplication::focusChanged);
    b->setFocus();
    CHECK(!focus.isEmpty() && focus.last().at(1).value<QWidget *>() == b);
    a->setFocus();
    CHECK(focus.last().at(0).value<QWidget *>() == b && focus.last().at(1).value<QWidget *>() == a);
    CHECK(QApplication::topLevelAt(top.geometry().center()) == &top && !QApplication::topLevelAt(QPoint(-500, -500)));

    // Menus: sections, default action, icon, collapsing, popups
    QMenu menu(&top);
    QAction *open = menu.addAction(QStringLiteral("Open"));
    QAction *section = menu.addSection(QStringLiteral("Recent"));
    menu.addAction(QStringLiteral("one.txt"));
    menu.addSeparator();
    menu.addSeparator();
    CHECK(section->isSeparator() && section->text() == QStringLiteral("Recent"));
    CHECK(menu.actionGeometry(section).height() > menu.actionGeometry(open).height() / 2);
    const QAction *last = menu.actions().last();
    CHECK(menu.actionGeometry(const_cast<QAction *>(last)).isEmpty());
    menu.setSeparatorsCollapsible(false);
    CHECK(!menu.actionGeometry(const_cast<QAction *>(last)).isEmpty());
    menu.setDefaultAction(open);
    CHECK(menu.defaultAction() == open);
    QPixmap px(8, 8);
    px.fill(Qt::green);
    menu.setIcon(QIcon(px));
    CHECK(!menu.icon().isNull() && !menu.menuAction()->icon().isNull());
    menu.setToolTipsVisible(true);
    menu.setTearOffEnabled(true);
    CHECK(menu.toolTipsVisible() && menu.isTearOffEnabled() && !menu.isTearOffMenuVisible());
    QMenu *sub = menu.addMenu(QStringLiteral("Sub"));
    CHECK(QMenu::menuInAction(sub->menuAction()) == sub && !QMenu::menuInAction(open));
    CHECK(!QApplication::activePopupWidget());
    menu.popup(top.mapToGlobal(QPoint(5, 5)));
    settle();
    CHECK(QApplication::activePopupWidget() == &menu);
    menu.hide();
    CHECK(!QApplication::activePopupWidget());
}

// Reaches the protected parts of QAbstractItemView.
class OpenView : public QListView {
public:
    using QAbstractItemView::executeDelayedItemsLayout;
    using QAbstractItemView::scheduleDelayedItemsLayout;
    using QAbstractItemView::selectedIndexes;
    using QAbstractItemView::selectionCommand;
    using QAbstractItemView::setState;
    using QAbstractItemView::State;
    using QAbstractItemView::state;
    using QAbstractItemView::visualRegionForSelection;
    using QListView::QListView;
    int layouts = 0;
    void doItemsLayout() override {
        ++layouts;
        QListView::doItemsLayout();
    }
};

static void testItemViewExtras() {
    QStandardItemModel model;
    for (const char *name : {"January", "February", "March", "May", "Mars"})
        model.appendRow(new QStandardItem(QString::fromLatin1(name)));
    OpenView view;
    view.setModel(&model);
    view.resize(200, 200);
    view.show();
    settle();

    // Persistent editors
    const QModelIndex first = model.index(0, 0);
    view.openPersistentEditor(first);
    CHECK(view.isPersistentEditorOpen(first) && !view.isPersistentEditorOpen(model.index(1, 0)));
    auto *editor = view.viewport()->findChild<QLineEdit *>();
    CHECK(editor && editor->text() == QStringLiteral("January"));
    model.setData(first, QStringLiteral("Jan"));
    CHECK(editor && editor->text() == QStringLiteral("Jan"));
    view.openPersistentEditor(model.index(2, 0));
    model.removeRow(2);
    CHECK(!view.isPersistentEditorOpen(model.index(2, 0)));
    view.closePersistentEditor(first);
    settle();
    CHECK(!view.isPersistentEditorOpen(first));

    // Keyboard search
    view.setCurrentIndex(first);
    view.keyboardSearch(QStringLiteral("m"));
    CHECK(view.currentIndex().data().toString() == QStringLiteral("May"));
    view.keyboardSearch(QStringLiteral("m"));
    CHECK(view.currentIndex().data().toString() == QStringLiteral("Mars"));
    view.keyboardSearch(QString());
    QTest::qWait(QApplication::keyboardInputInterval() + 50);
    view.keyboardSearch(QStringLiteral("f"));
    CHECK(view.currentIndex().data().toString() == QStringLiteral("February"));
    view.setEditTriggers(QAbstractItemView::NoEditTriggers); // a typed key searches instead of editing
    QKeyEvent j(QEvent::KeyPress, Qt::Key_J, Qt::NoModifier, QStringLiteral("j"));
    QTest::qWait(QApplication::keyboardInputInterval() + 50);
    QCoreApplication::sendEvent(&view, &j);
    CHECK(view.currentIndex().data().toString() == QStringLiteral("Jan"));

    // Selection helpers
    view.setSelectionMode(QAbstractItemView::ExtendedSelection);
    view.clearSelection();
    view.selectionModel()->select(model.index(1, 0), QItemSelectionModel::Select);
    CHECK(view.selectedIndexes().size() == 1);
    QMouseEvent ctrlClick(QEvent::MouseButtonPress, QPointF(1, 1), QPointF(1, 1), Qt::LeftButton, Qt::LeftButton,
                          Qt::ControlModifier);
    CHECK(view.selectionCommand(first, &ctrlClick) == QItemSelectionModel::Toggle);
    view.setSelectionMode(QAbstractItemView::NoSelection);
    CHECK(view.selectionCommand(first) == QItemSelectionModel::NoUpdate);
    QItemSelection sel(model.index(0, 0), model.index(1, 0));
    CHECK(view.visualRegionForSelection(sel).boundingRect() ==
          view.visualRect(model.index(0, 0)).united(view.visualRect(model.index(1, 0))));
    CHECK(view.sizeHintForRow(0) > 0 && view.sizeHintForColumn(0) > 0 && view.sizeHintForRow(99) == -1);

    // Properties, state, delayed layout
    CHECK(view.hasAutoScroll() && view.autoScrollMargin() == 16 && !view.tabKeyNavigation());
    view.setAutoScrollMargin(8);
    view.setTabKeyNavigation(true);
    view.setDragDropOverwriteMode(true);
    view.setUpdateThreshold(10);
    CHECK(view.autoScrollMargin() == 8 && view.tabKeyNavigation() && view.dragDropOverwriteMode() &&
          view.updateThreshold() == 10);
    view.setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    view.resetVerticalScrollMode();
    CHECK(view.verticalScrollMode() == QAbstractItemView::ScrollPerPixel);
    view.setState(OpenView::State::DragSelectingState);
    CHECK(view.state() == OpenView::State::DragSelectingState);
    view.setState(OpenView::State::NoState);
    view.layouts = 0;
    view.scheduleDelayedItemsLayout();
    view.scheduleDelayedItemsLayout();
    view.executeDelayedItemsLayout();
    settle();
    CHECK(view.layouts == 1);

    // Tab moves the current item with tabKeyNavigation
    view.setCurrentIndex(first);
    view.setFocus();
    QKeyEvent tab(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
    QCoreApplication::sendEvent(&view, &tab);
    CHECK(view.currentIndex().row() == 1);

    // entered() with mouse tracking
    view.setMouseTracking(true);
    QSignalSpy entered(&view, &QAbstractItemView::entered);
    QMouseEvent move(QEvent::MouseMove, QPointF(view.visualRect(model.index(1, 0)).center()),
                     QPointF(view.visualRect(model.index(1, 0)).center()), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(view.viewport(), &move);
    CHECK(entered.count() == 1 && entered.last().at(0).value<QModelIndex>().row() == 1);
}

static void testTreeMenuBarComboExtras() {
    // Tree view: hidden columns, tree position, search, header
    QStandardItemModel model;
    model.setColumnCount(3);
    auto *fruit = new QStandardItem(QStringLiteral("Fruit"));
    fruit->appendRow({new QStandardItem(QStringLiteral("Apple")), new QStandardItem(QStringLiteral("red")),
                      new QStandardItem(QStringLiteral("1"))});
    fruit->appendRow({new QStandardItem(QStringLiteral("Banana")), new QStandardItem(QStringLiteral("yellow")),
                      new QStandardItem(QStringLiteral("2"))});
    model.appendRow({fruit, new QStandardItem(), new QStandardItem()});
    QTreeView tree;
    tree.setModel(&model);
    tree.resize(300, 200);
    tree.expandAll();
    tree.show();
    settle();
    CHECK(!tree.isColumnHidden(1));
    const int before = tree.visualRect(model.index(0, 2)).left();
    tree.hideColumn(1);
    CHECK(tree.isColumnHidden(1) && tree.visualRect(model.index(0, 2)).left() < before);
    tree.showColumn(1);
    CHECK(!tree.isColumnHidden(1));
    CHECK(tree.treePosition() == 0);
    const QModelIndex apple = model.index(0, 0, fruit->index());
    const int indented = tree.visualRect(apple).left() - tree.columnViewportPosition(0);
    tree.setTreePosition(1);
    CHECK(tree.treePosition() == 1 && tree.visualRect(apple).left() == tree.columnViewportPosition(0));
    CHECK(indented > 0 && tree.visualRect(apple.siblingAtColumn(1)).left() > tree.columnViewportPosition(1));
    tree.setTreePosition(-1);
    CHECK(tree.treePosition() == 0);
    tree.setWordWrap(true);
    tree.setAllColumnsShowFocus(true);
    tree.setAutoExpandDelay(300);
    CHECK(tree.wordWrap() && tree.allColumnsShowFocus() && tree.autoExpandDelay() == 300);
    tree.keyboardSearch(QStringLiteral("b"));
    CHECK(tree.currentIndex().data().toString() == QStringLiteral("Banana"));
    CHECK(tree.sizeHintForColumn(0) > tree.sizeHintForColumn(2));
    auto *header = new QHeaderView(Qt::Horizontal);
    tree.setHeader(header);
    CHECK(tree.header() == header && header->count() == 3);

    // Menu bar
    QMenuBar bar;
    bar.addMenu(QStringLiteral("File"));
    QAction *sep = bar.addSeparator();
    bar.addMenu(QStringLiteral("Help"));
    CHECK(sep->isSeparator() && bar.actions().size() == 3 && bar.actionGeometry(sep).isEmpty());
    CHECK(!bar.isDefaultUp() && !bar.isNativeMenuBar());
    bar.setDefaultUp(true);
    bar.setNativeMenuBar(true);
    CHECK(bar.isDefaultUp() && !bar.isNativeMenuBar());

    // Combo box
    QComboBox combo;
    combo.addItems({QStringLiteral("one"), QStringLiteral("two")});
    combo.insertItems(1, {QStringLiteral("a"), QStringLiteral("b")});
    CHECK(combo.count() == 4 && combo.itemText(1) == QStringLiteral("a") && combo.itemText(3) == QStringLiteral("two"));
    combo.insertSeparator(2);
    CHECK(combo.count() == 5 && !(combo.model()->flags(combo.model()->index(2, 0)) & Qt::ItemIsEnabled));
    combo.setMaxCount(3);
    CHECK(combo.count() == 3 && combo.maxCount() == 3);
    combo.addItem(QStringLiteral("over"));
    CHECK(combo.count() == 3);
    QSignalSpy textActivated(&combo, &QComboBox::textActivated);
    combo.setFrame(false);
    CHECK(!combo.hasFrame());
    const int narrow = combo.sizeHint().width();
    combo.setMinimumContentsLength(40);
    CHECK(combo.sizeHint().width() > narrow);
    combo.setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    CHECK(combo.sizeAdjustPolicy() == QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo.setLabelDrawingMode(QComboBox::UseDelegate);
    CHECK(combo.labelDrawingMode() == QComboBox::UseDelegate);
    // An editable combo box with a line edit, validator and completer of its own
    combo.setMaxCount(100);
    auto *edit = new QLineEdit;
    combo.setLineEdit(edit);
    CHECK(combo.isEditable() && combo.lineEdit() == edit);
    QIntValidator digits(0, 99);
    combo.setValidator(&digits);
    CHECK(combo.validator() == &digits && edit->validator() == &digits);
    QCompleter completer(QStringList{QStringLiteral("alpha")});
    combo.setCompleter(&completer);
    CHECK(combo.completer() == &completer);
    combo.setValidator(nullptr);
    combo.setDuplicatesEnabled(true);
    const int count = combo.count();
    edit->setText(combo.itemText(0));
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::sendEvent(edit, &enter);
    CHECK(combo.count() == count + 1 && textActivated.count() == 1);
    // Root model index
    QStandardItemModel tree2;
    auto *group = new QStandardItem(QStringLiteral("group"));
    group->appendRow(new QStandardItem(QStringLiteral("child 1")));
    group->appendRow(new QStandardItem(QStringLiteral("child 2")));
    tree2.appendRow(group);
    QComboBox rooted;
    rooted.setModel(&tree2);
    CHECK(rooted.count() == 1);
    rooted.setRootModelIndex(group->index());
    CHECK(rooted.rootModelIndex() == group->index() && rooted.count() == 2 &&
          rooted.currentText() == QStringLiteral("child 1"));
    rooted.addItem(QStringLiteral("child 3"));
    CHECK(group->rowCount() == 3);
    auto *list = new QListView;
    rooted.setView(list);
    CHECK(rooted.view() == list && list->rootIndex() == group->index());
}

static void testItemWidgetExtras() {
    // List widget
    QListWidget list;
    list.addItems({QStringLiteral("pear"), QStringLiteral("apple"), QStringLiteral("Plum")});
    CHECK(list.findItems(QStringLiteral("p"), Qt::MatchStartsWith).size() == 2);
    CHECK(list.findItems(QStringLiteral("p"), Qt::MatchStartsWith | Qt::MatchCaseSensitive).size() == 1);
    CHECK(list.findItems(QStringLiteral("*le"), Qt::MatchWildcard).size() == 1);
    list.setSortingEnabled(true);
    CHECK(list.isSortingEnabled() && list.item(0)->text() == QStringLiteral("Plum"));
    list.addItem(QStringLiteral("banana"));
    CHECK(list.item(1)->text() == QStringLiteral("apple") && list.item(2)->text() == QStringLiteral("banana"));
    list.setSortingEnabled(false);
    QSignalSpy changed(&list, &QListWidget::itemChanged);
    list.item(0)->setText(QStringLiteral("Plums"));
    CHECK(changed.count() == 1);
    list.resize(200, 200);
    list.show();
    settle();
    QListWidgetItem *banana = list.item(2);
    banana->setFlags(banana->flags() | Qt::ItemIsEditable);
    list.editItem(banana);
    auto *editor = list.viewport()->findChild<QLineEdit *>();
    CHECK(editor && editor->text() == QStringLiteral("banana"));
    if (editor) {
        editor->setText(QStringLiteral("cherry"));
        emit editor->editingFinished();
    }
    CHECK(banana->text() == QStringLiteral("cherry"));
    list.editItem(list.item(1)); // not editable
    QCoreApplication::processEvents();
    CHECK(!list.viewport()->findChild<QLineEdit *>() || list.viewport()->findChild<QLineEdit *>()->isHidden());
    QSignalSpy pressed(&list, &QListWidget::itemPressed);
    const QPoint at = list.visualItemRect(list.item(1)).center();
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(at), QPointF(at), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(list.viewport(), &press);
    CHECK(pressed.count() == 1 && pressed.last().at(0).value<QListWidgetItem *>() == list.item(1));
    list.setMouseTracking(true);
    QSignalSpy entered(&list, &QListWidget::itemEntered);
    const QPoint over = list.visualItemRect(list.item(0)).center();
    QMouseEvent move(QEvent::MouseMove, QPointF(over), QPointF(over), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(list.viewport(), &move);
    CHECK(entered.count() == 1);
    CHECK(!list.indexFromItem(list.item(0)).isValid());
    list.setSupportedDragActions(Qt::MoveAction);
    CHECK(list.supportedDragActions() == Qt::MoveAction);

    // Table widget
    QTableWidget table(3, 2);
    table.setItem(0, 0, new QTableWidgetItem(QStringLiteral("alpha")));
    table.setItem(2, 1, new QTableWidgetItem(QStringLiteral("beta")));
    QTableWidgetItem *beta = table.item(2, 1);
    CHECK(table.row(beta) == 2 && table.column(beta) == 1 && table.row(nullptr) == -1);
    CHECK(table.visualRow(1) == 1 && table.visualColumn(1) == 1);
    CHECK(table.findItems(QStringLiteral("a"), Qt::MatchContains).size() == 2);
    CHECK(table.findItems(QStringLiteral("beta"), Qt::MatchExactly).value(0) == beta);
    QSignalSpy current(&table, &QTableWidget::currentItemChanged);
    table.setCurrentCell(2, 1);
    CHECK(current.count() == 1 && current.last().at(0).value<QTableWidgetItem *>() == beta);
    QSignalSpy activated(&table, &QTableWidget::cellActivated);
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::sendEvent(&table, &enter);
    CHECK(activated.count() == 1 && activated.last().at(0).toInt() == 2);
    table.setHorizontalHeaderItem(1, new QTableWidgetItem(QStringLiteral("B")));
    QTableWidgetItem *header = table.takeHorizontalHeaderItem(1);
    CHECK(header && header->text() == QStringLiteral("B") && !table.horizontalHeaderItem(1));
    delete header;
    CHECK(!table.takeVerticalHeaderItem(0));
    table.setItemPrototype(new QTableWidgetItem(QStringLiteral("proto")));
    CHECK(table.itemPrototype() && table.itemPrototype()->text() == QStringLiteral("proto"));
    CHECK(table.itemFromIndex(QModelIndex()) == nullptr);
    table.resize(300, 200);
    table.show();
    settle();
    QSignalSpy cellPressed(&table, &QTableWidget::cellPressed);
    const QPoint cell = table.visualItemRect(beta).center();
    QMouseEvent pressCell(QEvent::MouseButtonPress, QPointF(cell), QPointF(cell), Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
    QCoreApplication::sendEvent(table.viewport(), &pressCell);
    CHECK(cellPressed.count() == 1 && cellPressed.last().at(1).toInt() == 1);
}

static void testButtonExtras() {
    // Auto-repeat
    QPushButton repeat(QStringLiteral("+"));
    repeat.setAutoRepeat(true);
    repeat.setAutoRepeatDelay(30);
    repeat.setAutoRepeatInterval(20);
    CHECK(repeat.autoRepeat() && repeat.autoRepeatDelay() == 30 && repeat.autoRepeatInterval() == 20);
    int clicks = 0;
    QObject::connect(&repeat, &QAbstractButton::clicked, [&] { ++clicks; });
    repeat.setDown(true);
    QTest::qWait(150);
    repeat.setDown(false);
    CHECK(clicks >= 3);
    const int after = clicks;
    QTest::qWait(60);
    CHECK(clicks == after);

    // Tristate check box
    QCheckBox box(QStringLiteral("x"));
    QSignalSpy states(&box, &QCheckBox::checkStateChanged);
    CHECK(!box.isTristate() && box.checkState() == Qt::Unchecked);
    box.setCheckState(Qt::PartiallyChecked);
    CHECK(box.isTristate() && box.checkState() == Qt::PartiallyChecked && !box.isChecked() && states.count() == 1);
    box.click();
    CHECK(box.checkState() == Qt::Checked && box.isChecked());
    box.click();
    CHECK(box.checkState() == Qt::Unchecked);
    box.click();
    CHECK(box.checkState() == Qt::PartiallyChecked && states.count() == 4);
    box.setChecked(true);
    CHECK(box.checkState() == Qt::Checked && states.last().at(0).value<Qt::CheckState>() == Qt::Checked);
    box.setTristate(false);
    box.click();
    CHECK(box.checkState() == Qt::Unchecked);
    QSignalSpy legacy(&box, &QCheckBox::stateChanged);
    box.setCheckState(Qt::Checked);
    CHECK(legacy.count() == 1 && legacy.last().at(0).toInt() == 2);

    // Push button and tool button menus
    QWidget top;
    top.resize(300, 200);
    auto *push = new QPushButton(QStringLiteral("Menu"), &top);
    QMenu menu(&top);
    QAction *copy = menu.addAction(QStringLiteral("Copy"));
    push->setMenu(&menu);
    CHECK(push->menu() == &menu);
    top.show();
    settle();
    push->showMenu();
    settle();
    CHECK(menu.isVisible() && push->isDown());
    menu.hide();
    settle();
    CHECK(!push->isDown());

    auto *tool = new QToolButton(&top);
    tool->setGeometry(100, 10, 40, 24);
    tool->setMenu(&menu);
    CHECK(tool->menu() == &menu && tool->popupMode() == QToolButton::DelayedPopup);
    tool->setPopupMode(QToolButton::InstantPopup);
    QSignalSpy triggered(tool, &QToolButton::triggered);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(5, 5), QPointF(5, 5), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(tool, &press);
    settle();
    CHECK(menu.isVisible());
    copy->trigger();
    menu.hide();
    settle();
    CHECK(!tool->isDown());
    tool->setPopupMode(QToolButton::MenuButtonPopup);
    int toolClicks = 0;
    QObject::connect(tool, &QAbstractButton::clicked, [&] { ++toolClicks; });
    QMouseEvent left(QEvent::MouseButtonPress, QPointF(3, 5), QPointF(3, 5), Qt::LeftButton, Qt::LeftButton,
                     Qt::NoModifier);
    QMouseEvent leftUp(QEvent::MouseButtonRelease, QPointF(3, 5), QPointF(3, 5), Qt::LeftButton, Qt::NoButton,
                       Qt::NoModifier);
    QCoreApplication::sendEvent(tool, &left);
    QCoreApplication::sendEvent(tool, &leftUp);
    CHECK(toolClicks == 1 && !menu.isVisible());
    QMouseEvent arrow(QEvent::MouseButtonPress, QPointF(37, 5), QPointF(37, 5), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(tool, &arrow);
    settle();
    CHECK(menu.isVisible());
    menu.hide();
}

// Reaches the protected parts of QAbstractSlider.
class RepeatSlider : public QSlider {
public:
    using QSlider::QSlider;
    using QAbstractSlider::repeatAction;
    using QAbstractSlider::setRepeatAction;
};

static void testSmallWidgetExtras() {
    // Slider: ticks, inverted appearance, repeat action
    RepeatSlider slider(Qt::Horizontal);
    slider.resize(200, 30);
    const int plain = slider.sizeHint().height();
    slider.setTickPosition(QSlider::TicksBothSides);
    slider.setTickInterval(10);
    CHECK(slider.tickPosition() == QSlider::TicksBothSides && slider.tickInterval() == 10 &&
          slider.sizeHint().height() == plain + 10);
    slider.setValue(0);
    const QImage normal = slider.grab().toImage();
    slider.setInvertedAppearance(true);
    CHECK(slider.invertedAppearance() && slider.grab().toImage() != normal);
    slider.setInvertedAppearance(false);
    slider.setRange(0, 100);
    slider.setSingleStep(1);
    slider.setRepeatAction(QAbstractSlider::SliderSingleStepAdd, 10, 10);
    CHECK(slider.repeatAction() == QAbstractSlider::SliderSingleStepAdd);
    QTest::qWait(80);
    slider.setRepeatAction(QAbstractSlider::SliderNoAction);
    const int reached = slider.value();
    CHECK(reached >= 2);
    QTest::qWait(40);
    CHECK(slider.value() == reached);

    // Dial
    QDial dial;
    dial.resize(100, 100);
    dial.setRange(0, 100);
    CHECK(dial.notchTarget() > 3.6 && dial.notchSize() >= 1);
    dial.setNotchTarget(20);
    CHECK(dial.notchTarget() == 20 && dial.notchSize() > 1);

    // Progress bar
    QProgressBar bar;
    bar.setValue(50);
    bar.setFormat(QStringLiteral("%v"));
    bar.resetFormat();
    CHECK(bar.format() == QStringLiteral("%p%") && bar.text() == QStringLiteral("50%"));
    const QSize horizontal = bar.sizeHint();
    bar.setOrientation(Qt::Vertical);
    CHECK(bar.orientation() == Qt::Vertical && bar.sizeHint() == horizontal.transposed());
    bar.setInvertedAppearance(true);
    bar.setTextDirection(QProgressBar::BottomToTop);
    bar.setAlignment(Qt::AlignCenter);
    CHECK(bar.invertedAppearance() && bar.textDirection() == QProgressBar::BottomToTop &&
          bar.alignment() == Qt::AlignCenter);
    bar.resize(20, 100);
    bar.setInvertedAppearance(false);
    const QImage up = bar.grab().toImage();
    const QColor highlight = bar.palette().color(QPalette::Active, QPalette::Highlight);
    CHECK(up.pixelColor(10, 90) == highlight && up.pixelColor(10, 10) != highlight);

    // Group box
    QGroupBox box(QStringLiteral("Title"));
    box.resize(200, 100);
    box.setFlat(true);
    box.setAlignment(Qt::AlignRight);
    CHECK(box.isFlat() && box.alignment() == Qt::AlignRight);

    // Frame
    QFrame frame;
    frame.resize(100, 100);
    frame.setFrameStyle(QFrame::Box | QFrame::Plain);
    frame.setFrameRect(QRect(10, 10, 50, 50));
    CHECK(frame.frameRect() == QRect(10, 10, 50, 50) && frame.contentsRect() == QRect(11, 11, 48, 48));
    frame.setFrameRect(QRect());
    CHECK(frame.frameRect() == frame.rect());

    // Scroll bar menu
    QScrollBar scroll(Qt::Vertical);
    scroll.setRange(0, 100);
    scroll.resize(16, 200);
    QMenu *menu = scroll.createStandardContextMenu(QPoint(8, 190));
    QStringList texts;
    for (QAction *a : menu->actions())
        if (!a->isSeparator())
            texts << a->text();
    CHECK(texts.size() == 7 && texts.first() == QStringLiteral("Scroll here"));
    menu->actions().at(3)->trigger(); // Bottom, after Scroll here, a separator and Top
    CHECK(scroll.value() == 100);
    menu->actions().first()->trigger();
    CHECK(scroll.value() > 80);
    delete menu;

    // Size policy
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    policy.setHorizontalStretch(2);
    policy.setControlType(QSizePolicy::PushButton);
    const QSizePolicy t = policy.transposed();
    CHECK(t.horizontalPolicy() == QSizePolicy::Fixed && t.verticalPolicy() == QSizePolicy::Expanding &&
          t.verticalStretch() == 2 && t.controlType() == QSizePolicy::PushButton);
    policy.transpose();
    CHECK(policy == t);
}

static void testSpinBoxExtras() {
    QSpinBox spin;
    spin.setRange(0, 5000);
    spin.setValue(255);
    spin.setDisplayIntegerBase(16);
    CHECK(spin.displayIntegerBase() == 16 && spin.text() == QStringLiteral("ff"));
    spin.setDisplayIntegerBase(10);
    spin.setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
    spin.setValue(1234);
    spin.setGroupSeparatorShown(true);
    CHECK(spin.isGroupSeparatorShown() && spin.text() == QStringLiteral("1,234"));
    QString input = QStringLiteral("2,000");
    int pos = 0;
    CHECK(spin.validate(input, pos) == QValidator::Acceptable);
    input = QStringLiteral("9999");
    CHECK(spin.validate(input, pos) == QValidator::Intermediate);
    input = QStringLiteral("abc");
    CHECK(spin.validate(input, pos) == QValidator::Invalid);
    input = QStringLiteral("3000");
    spin.fixup(input);
    CHECK(input == QStringLiteral("3,000"));
    spin.setGroupSeparatorShown(false);
    CHECK(spin.hasAcceptableInput());
    // Out-of-range typed values
    spin.findChild<QLineEdit *>()->setText(QStringLiteral("9000"));
    emit spin.findChild<QLineEdit *>()->editingFinished();
    CHECK(spin.value() == 1234);
    spin.setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
    spin.findChild<QLineEdit *>()->setText(QStringLiteral("9000"));
    emit spin.findChild<QLineEdit *>()->editingFinished();
    CHECK(spin.value() == 5000);
    // Adaptive steps
    spin.setValue(1500);
    spin.setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    spin.stepBy(1);
    CHECK(spin.value() == 1600);
    spin.setValue(150);
    spin.stepBy(1);
    CHECK(spin.value() == 160);
    // Frame, alignment, line edit, returnPressed
    spin.setFrame(false);
    spin.setAlignment(Qt::AlignRight);
    spin.setAccelerated(true);
    CHECK(!spin.hasFrame() && spin.alignment() == Qt::AlignRight && spin.isAccelerated());
    QSignalSpy returned(&spin, &QAbstractSpinBox::returnPressed);
    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::sendEvent(spin.findChild<QLineEdit *>(), &enter);
    CHECK(returned.count() == 1);

    QDoubleSpinBox dbl;
    dbl.setRange(0, 100);
    dbl.setDecimals(2);
    input = QStringLiteral("12.345");
    CHECK(dbl.validate(input, pos) == QValidator::Invalid);
    input = QStringLiteral("12.34");
    CHECK(dbl.validate(input, pos) == QValidator::Acceptable);
    input = QStringLiteral("7");
    dbl.fixup(input);
    CHECK(input == QStringLiteral("7.00"));
    dbl.setValue(50);
    dbl.setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
    dbl.stepBy(1);
    CHECK(qFuzzyCompare(dbl.value(), 51.0));
}

// Reaches the protected parts of QDialog and QSplitter.
class OpenDialog : public QDialog {
public:
    using QDialog::adjustPosition;
};
class OpenSplitter : public QSplitter {
public:
    using QSplitter::getRange;
    using QSplitter::setRubberBand;
};

static void testLabelAndSmallClassExtras() {
    // Label: indent, selection
    QLabel label(QStringLiteral("Hello world"));
    const QSize plain = label.sizeHint();
    label.setIndent(10);
    CHECK(label.indent() == 10 && label.sizeHint().width() == plain.width() + 10);
    label.setSelection(0, 5);
    CHECK(!label.hasSelectedText()); // not selectable by default
    label.setTextInteractionFlags(Qt::TextSelectableByMouse);
    label.setSelection(6, 5);
    CHECK(label.hasSelectedText() && label.selectedText() == QStringLiteral("world") && label.selectionStart() == 6);
    label.resize(label.sizeHint());
    const QImage selected = label.grab().toImage();
    label.setSelection(0, 0);
    CHECK(!label.hasSelectedText() && label.selectionStart() == -1 && label.grab().toImage() != selected);
    label.setText(QStringLiteral("<b>rich</b> text"));
    label.setSelection(0, 4);
    CHECK(label.selectedText() == QStringLiteral("rich"));
    label.setText(QStringLiteral("other"));
    CHECK(!label.hasSelectedText());
    int asked = 0;
    label.setResourceProvider([&](const QUrl &) {
        ++asked;
        return QVariant();
    });
    CHECK(label.resourceProvider() != nullptr);

    // Message box
    QMessageBox box(QMessageBox::Warning, QStringLiteral("t"), QStringLiteral("x"));
    QPixmap px(16, 16);
    px.fill(Qt::red);
    box.setIconPixmap(px);
    CHECK(box.icon() == QMessageBox::NoIcon && !box.iconPixmap().isNull());
    box.setTextInteractionFlags(Qt::TextSelectableByMouse);
    CHECK(box.textInteractionFlags() == Qt::TextSelectableByMouse);
    box.setOption(QMessageBox::Option::DontUseNativeDialog);
    CHECK(box.testOption(QMessageBox::Option::DontUseNativeDialog));

    // Tool tip font and palette
    QFont tipFont = QToolTip::font();
    tipFont.setPointSize(20);
    QToolTip::setFont(tipFont);
    CHECK(QToolTip::font().pointSize() == 20);
    QPalette tipPalette = QToolTip::palette();
    tipPalette.setColor(QPalette::ToolTipBase, Qt::yellow);
    QToolTip::setPalette(tipPalette);
    CHECK(QToolTip::palette().color(QPalette::ToolTipBase) == QColor(Qt::yellow));
    QToolTip::setFont(QGuiApplication::font());

    // Dialog
    OpenDialog dialog;
    dialog.resize(200, 150);
    dialog.setSizeGripEnabled(true);
    auto *grip = dialog.findChild<QSizeGrip *>();
    CHECK(dialog.isSizeGripEnabled() && grip && grip->geometry().bottomRight() == QPoint(199, 149));
    dialog.resize(300, 200);
    CHECK(grip->geometry().bottomRight() == QPoint(299, 199));
    dialog.setSizeGripEnabled(false);
    CHECK(!dialog.isSizeGripEnabled());
    QWidget anchor;
    anchor.setGeometry(100, 100, 400, 400);
    dialog.adjustPosition(&anchor);
    CHECK(dialog.geometry().center().x() >= 290 && dialog.geometry().center().x() <= 310);

    // Tool box, splitter, status bar, spacer, stacked widget
    QToolBox toolBox;
    toolBox.addItem(new QWidget, QStringLiteral("a"));
    toolBox.setItemIcon(0, QIcon(px));
    CHECK(!toolBox.itemIcon(0).isNull() && toolBox.itemIcon(5).isNull());
    OpenSplitter splitter;
    splitter.addWidget(new QWidget);
    splitter.addWidget(new QWidget);
    splitter.resize(200, 100);
    splitter.show();
    settle();
    int low = -1, high = -1;
    splitter.getRange(1, &low, &high);
    CHECK(low == 0 && high >= 190);
    splitter.setRubberBand(50);
    auto *band = splitter.findChild<QRubberBand *>();
    CHECK(band && band->isVisible() && band->x() == 50);
    splitter.setRubberBand(-1);
    CHECK(!band->isVisible());
    QSpacerItem spacer(10, 10);
    spacer.changeSize(30, 40, QSizePolicy::Expanding, QSizePolicy::Fixed);
    CHECK(spacer.sizeHint() == QSize(30, 40) && spacer.expandingDirections() == Qt::Horizontal);
    QStackedWidget stack;
    QSignalSpy added(&stack, &QStackedWidget::widgetAdded);
    stack.addWidget(new QWidget);
    stack.insertWidget(0, new QWidget);
    CHECK(added.count() == 2 && added.last().at(0).toInt() == 0);
    QDateEdit dateEdit;
    QSignalSpy userDate(&dateEdit, &QDateEdit::userDateChanged);
    dateEdit.setDate(QDate(2020, 5, 6));
    CHECK(userDate.count() == 1);
    QTimeEdit timeEdit;
    QSignalSpy userTime(&timeEdit, &QTimeEdit::userTimeChanged);
    timeEdit.setTime(QTime(10, 30));
    CHECK(userTime.count() == 1);
    QErrorMessage *handler = QErrorMessage::qtHandler();
    CHECK(handler && QErrorMessage::qtHandler() == handler);
}

// A scroll area whose contents have a size.
class SizedArea : public QAbstractScrollArea {
public:
    QWidget *setUp = nullptr;
    int painted = 0;

protected:
    void setupViewport(QWidget *viewport) override { setUp = viewport; }
    void paintEvent(QPaintEvent *) override { ++painted; }
};

static void testScrollAreaCompleterExtras() {
    SizedArea area;
    area.resize(200, 150);
    area.verticalScrollBar()->setRange(0, 100);
    area.horizontalScrollBar()->setRange(0, 100);
    area.show();
    settle();
    auto *corner = new QWidget;
    area.setCornerWidget(corner);
    CHECK(area.cornerWidget() == corner && corner->isVisible() && corner->geometry().right() == 198);
    auto *left = new QWidget;
    left->setFixedSize(20, 10);
    area.addScrollBarWidget(left, Qt::AlignLeft);
    CHECK(area.scrollBarWidgets(Qt::AlignLeft) == QWidgetList{left} && area.scrollBarWidgets(Qt::AlignTop).isEmpty());
    CHECK(area.horizontalScrollBar()->x() == left->geometry().right() + 1);
    auto *bar = new QScrollBar;
    area.setVerticalScrollBar(bar);
    CHECK(area.verticalScrollBar() == bar && bar->orientation() == Qt::Vertical && bar->maximum() == 100);
    auto *viewport = new QWidget;
    area.setViewport(viewport);
    CHECK(area.viewport() == viewport && area.setUp == viewport);
    area.painted = 0;
    viewport->repaint();
    CHECK(area.painted > 0);
    CHECK(area.maximumViewportSize().width() > 150);
    area.setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    CHECK(area.sizeAdjustPolicy() == QAbstractScrollArea::AdjustToContents);

    // Completer with a model
    QStringListModel words({QStringLiteral("apple"), QStringLiteral("apricot"), QStringLiteral("banana")});
    QCompleter completer(&words);
    CHECK(completer.model() == &words);
    completer.setCompletionPrefix(QStringLiteral("ap"));
    CHECK(completer.completionCount() == 2 && completer.completionModel()->rowCount() == 2);
    completer.setCurrentRow(1);
    CHECK(completer.currentIndex().data().toString() == QStringLiteral("apricot"));
    words.setStringList({QStringLiteral("apex")});
    completer.setCompletionPrefix(QStringLiteral("ap"));
    CHECK(completer.completionCount() == 1 && completer.currentCompletion() == QStringLiteral("apex"));
    CHECK(completer.pathFromIndex(words.index(0)) == QStringLiteral("apex") &&
          completer.splitPath(QStringLiteral("a/b")) == QStringList{QStringLiteral("a/b")});
    QStandardItemModel table(2, 2);
    table.setItem(0, 1, new QStandardItem(QStringLiteral("cherry")));
    table.setItem(1, 1, new QStandardItem(QStringLiteral("cranberry")));
    completer.setModel(&table);
    completer.setCompletionColumn(1);
    completer.setCompletionPrefix(QStringLiteral("cr"));
    CHECK(completer.completionColumn() == 1 && completer.completionCount() == 1);
    QCompleter plain(QStringList{QStringLiteral("x")});
    CHECK(plain.model() && plain.model()->rowCount() == 1);

    // Key sequence edit
    QKeySequenceEdit keys;
    keys.setFinishingKeyCombinations({QKeyCombination(Qt::Key_Tab)});
    keys.setMaximumSequenceLength(9);
    CHECK(keys.maximumSequenceLength() == 4 && keys.finishingKeyCombinations().size() == 1);
    QKeyEvent tab(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
    QCoreApplication::sendEvent(&keys, &tab);
    CHECK(keys.keySequence().isEmpty());

    // Progress dialog parts
    QProgressDialog progress(QStringLiteral("Working"), QStringLiteral("Cancel"), 0, 10);
    progress.setValue(3);
    auto *newBar = new QProgressBar;
    progress.setBar(newBar);
    CHECK(newBar->parentWidget() == &progress && newBar->value() == 3 && progress.value() == 3);
    auto *newLabel = new QLabel(QStringLiteral("Other"));
    progress.setLabel(newLabel);
    CHECK(newLabel->parentWidget() == &progress);
    auto *stop = new QPushButton(QStringLiteral("Stop"));
    progress.setCancelButton(stop);
    QSignalSpy canceled(&progress, &QProgressDialog::canceled);
    stop->click();
    CHECK(canceled.count() == 1 && progress.wasCanceled());
    progress.setCancelButton(nullptr);
    CHECK(!progress.findChild<QPushButton *>());
}

// Reaches the protected parts of QLayout.
class OpenBox : public QHBoxLayout {
public:
    using QHBoxLayout::QHBoxLayout;
    using QLayout::alignmentRect;
};

static void testLayoutExtras() {
    // Box layout: spacer items, struts, replaceWidget, size constraints
    QWidget host;
    auto *box = new OpenBox(&host);
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(0);
    auto *a = new QWidget;
    a->setMinimumSize(20, 10);
    box->addWidget(a);
    box->addSpacerItem(new QSpacerItem(30, 5));
    box->insertItem(0, new QSpacerItem(5, 5));
    CHECK(box->count() == 3 && box->itemAt(1)->widget() == a);
    box->addStrut(40);
    CHECK(box->sizeHint().height() >= 40 && box->minimumSize().height() >= 40);
    auto *b = new QWidget;
    QLayoutItem *old = box->replaceWidget(a, b);
    CHECK(old && old->widget() == a && box->itemAt(1)->widget() == b && b->parentWidget() == &host);
    delete old;
    delete a;
    CHECK(!box->replaceWidget(a, b));
    int l = -1, t = -1, r = -1, bm = -1;
    box->setContentsMargins(1, 2, 3, 4);
    box->getContentsMargins(&l, &t, &r, &bm);
    CHECK(l == 1 && t == 2 && r == 3 && bm == 4);
    box->unsetContentsMargins();
    CHECK(box->contentsMargins() != QMargins(1, 2, 3, 4));
    box->setSizeConstraints(QLayout::SetFixedSize, QLayout::SetNoConstraint);
    CHECK(box->horizontalSizeConstraint() == QLayout::SetFixedSize &&
          box->verticalSizeConstraint() == QLayout::SetNoConstraint);
    box->activate();
    CHECK(host.minimumWidth() == host.maximumWidth() && host.maximumHeight() == QWIDGETSIZE_MAX);
    CHECK(box->totalSizeHint().width() >= box->sizeHint().width());
    box->setEnabled(false);
    CHECK(!box->isEnabled() && !box->activate());
    box->setEnabled(true);
    box->setAlignment(Qt::AlignRight);
    const QRect aligned = box->alignmentRect(QRect(0, 0, 500, 100));
    CHECK(aligned.right() == 499 && aligned.width() <= 500);
    QPushButton pushed;
    pushed.setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed, QSizePolicy::PushButton));
    QWidgetItem item(&pushed);
    CHECK(item.controlTypes() == QSizePolicy::PushButton && !item.hasHeightForWidth() && item.heightForWidth(10) == -1);
    QWidget sized;
    sized.setMinimumSize(50, 50);
    sized.setMaximumSize(100, 100);
    CHECK(QLayout::closestAcceptableSize(&sized, QSize(10, 200)) == QSize(50, 100));

    // Grid: cellRect, origin corner, default positioning
    QWidget gridHost;
    auto *grid = new QGridLayout(&gridHost);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(0);
    grid->setDefaultPositioning(2, Qt::Horizontal);
    QList<QWidget *> cells;
    for (int i = 0; i < 4; ++i) {
        cells << new QWidget;
        cells.last()->setMinimumSize(10, 10);
        grid->addWidget(cells.last());
    }
    int row = -1, col = -1, rs = 0, cs = 0;
    grid->getItemPosition(3, &row, &col, &rs, &cs);
    CHECK(row == 1 && col == 1);
    gridHost.resize(100, 100);
    grid->setGeometry(QRect(0, 0, 100, 100));
    CHECK(grid->cellRect(0, 0).topLeft() == QPoint(0, 0) && grid->cellRect(1, 1).bottomRight() == QPoint(99, 99));
    CHECK(!grid->cellRect(5, 5).isValid());
    grid->setOriginCorner(Qt::BottomRightCorner);
    grid->setGeometry(QRect(0, 0, 100, 100));
    CHECK(grid->originCorner() == Qt::BottomRightCorner && grid->cellRect(0, 0).bottomRight() == QPoint(99, 99) &&
          cells[0]->geometry().bottomRight() == QPoint(99, 99));

    // Form layout
    QWidget formHost;
    auto *form = new QFormLayout(&formHost);
    form->addRow(QStringLiteral("Name"), new QLineEdit);
    auto *field = new QLineEdit;
    form->setItem(1, QFormLayout::FieldRole, new QWidgetItem(field));
    QFormLayout::ItemRole role = QFormLayout::LabelRole;
    form->getItemPosition(form->indexOf(field), &row, &role);
    CHECK(row == 1 && role == QFormLayout::FieldRole && field->parentWidget() == &formHost);

    // Stacked layout: StackAll
    QWidget stackHost;
    auto *stacked = new QStackedLayout(&stackHost);
    auto *p1 = new QWidget;
    auto *p2 = new QWidget;
    stacked->addWidget(p1);
    stacked->addWidget(p2);
    stackHost.show();
    settle();
    CHECK(p1->isVisible() && !p2->isVisible());
    stacked->setStackingMode(QStackedLayout::StackAll);
    CHECK(stacked->stackingMode() == QStackedLayout::StackAll && p1->isVisible() && p2->isVisible());
    stacked->setCurrentIndex(1);
    CHECK(p1->isVisible() && p2->isVisible());
    stacked->setStackingMode(QStackedLayout::StackOne);
    CHECK(!p1->isVisible() && p2->isVisible());
}

// An item that records what reaches it.
class RecordingItem : public QGraphicsRectItem {
public:
    using QGraphicsRectItem::QGraphicsRectItem;
    QString committed;
    QList<int> phases;
    int filtered = 0;
    void advance(int phase) override { phases << phase; }

protected:
    void inputMethodEvent(QInputMethodEvent *event) override {
        committed += event->commitString();
        event->accept();
    }
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override {
        return query == Qt::ImSurroundingText ? QVariant(committed) : QVariant();
    }
    bool sceneEventFilter(QGraphicsItem *, QEvent *) override {
        ++filtered;
        return false;
    }
};

static void testInputMethodAndGraphicsItemExtras() {
    // Line edit and plain text edit take committed text
    QLineEdit line(QStringLiteral("helo"));
    line.setCursorPosition(4);
    QInputMethodEvent im;
    im.setCommitString(QStringLiteral("llo"), -2, 2);
    QCoreApplication::sendEvent(&line, &im);
    CHECK(line.text() == QStringLiteral("hello") && im.isAccepted());
    QPlainTextEdit plain;
    QInputMethodEvent typed(QStringLiteral("pre"), {QInputMethodEvent::Attribute(QInputMethodEvent::Cursor, 0, 1)});
    typed.setCommitString(QStringLiteral("abc"));
    CHECK(typed.preeditString() == QStringLiteral("pre") && typed.attributes().size() == 1 &&
          typed.replacementStart() == 0);
    QCoreApplication::sendEvent(&plain, &typed);
    CHECK(plain.toPlainText() == QStringLiteral("abc"));
    QComboBox combo;
    combo.setEditable(true);
    QInputMethodEvent toCombo;
    toCombo.setCommitString(QStringLiteral("x"));
    QCoreApplication::sendEvent(&combo, &toCombo);
    CHECK(combo.currentText() == QStringLiteral("x"));
    QWidget plainWidget;
    QInputMethodEvent ignored;
    QCoreApplication::sendEvent(&plainWidget, &ignored);
    CHECK(!ignored.isAccepted());

    // Graphics items
    QGraphicsScene scene;
    auto *item = new RecordingItem(0, 0, 10, 10);
    item->setFlag(QGraphicsItem::ItemIsFocusable);
    scene.addItem(item);
    item->setFocus();
    QInputMethodEvent toScene;
    toScene.setCommitString(QStringLiteral("hi"));
    QCoreApplication::sendEvent(&scene, &toScene);
    CHECK(item->committed == QStringLiteral("hi") &&
          scene.inputMethodQuery(Qt::ImSurroundingText).toString() == QStringLiteral("hi"));
    scene.advance();
    CHECK(item->phases == (QList<int>{0, 1}));
    CHECK(!static_cast<QGraphicsItem *>(item)->isWidget()); // the extensions are protected, as in Qt
    item->setInputMethodHints(Qt::ImhDigitsOnly);
    CHECK(item->inputMethodHints() == Qt::ImhDigitsOnly);
    item->setBoundingRegionGranularity(2);
    CHECK(item->boundingRegionGranularity() == 1.0);
    CHECK(item->boundingRegion(QTransform::fromScale(2, 2)).boundingRect() == QRect(-1, -1, 22, 22) ||
          item->boundingRegion(QTransform::fromScale(2, 2)).boundingRect().contains(QRect(0, 0, 20, 20)));
    // Focus proxy
    auto *other = new QGraphicsRectItem(0, 0, 5, 5);
    other->setFlag(QGraphicsItem::ItemIsFocusable);
    scene.addItem(other);
    item->setFocusProxy(other);
    CHECK(item->focusProxy() == other);
    item->setFocus();
    CHECK(scene.focusItem() == other);
    other->setFocusProxy(item); // a loop is refused
    CHECK(!other->focusProxy());
    delete other;
    CHECK(!item->focusProxy());
    // Child event filtering
    auto *child = new QGraphicsRectItem(0, 0, 4, 4, item);
    item->setFiltersChildEvents(true);
    QEvent custom(QEvent::User);
    scene.sendEvent(child, &custom);
    CHECK(item->filtersChildEvents() && item->filtered == 1);
    // Panels
    auto *panel = new QGraphicsRectItem(0, 0, 50, 50);
    panel->setFlag(QGraphicsItem::ItemIsPanel);
    scene.addItem(panel);
    auto *inPanel = new QGraphicsRectItem(0, 0, 5, 5, panel);
    CHECK(inPanel->panel() == panel && !item->panel());
    panel->setPanelModality(QGraphicsItem::SceneModal);
    QGraphicsItem *blocker = nullptr;
    CHECK(item->isBlockedByModalPanel(&blocker) && blocker == panel && !inPanel->isBlockedByModalPanel());
    panel->setPanelModality(QGraphicsItem::NonModal);
    CHECK(!item->isBlockedByModalPanel());
    CHECK(!item->topLevelWidget() && !item->window() && !item->focusScopeItem());
}

static void testItemClassAndTableViewExtras() {
    // Item classes
    QListWidgetItem listItem(QStringLiteral("one"));
    listItem.setStatusTip(QStringLiteral("status"));
    listItem.setWhatsThis(QStringLiteral("what"));
    listItem.setCheckState(Qt::Checked);
    CHECK(listItem.statusTip() == QStringLiteral("status") && listItem.whatsThis() == QStringLiteral("what") &&
          listItem.checkState() == Qt::Checked);
    std::unique_ptr<QListWidgetItem> listCopy(listItem.clone());
    CHECK(listCopy->text() == QStringLiteral("one") && listCopy->statusTip() == QStringLiteral("status") &&
          !listCopy->listWidget());
    QByteArray bytes;
    {
        QDataStream out(&bytes, QIODevice::WriteOnly);
        listItem.write(out);
    }
    QListWidgetItem readBack;
    {
        QDataStream in(bytes);
        readBack.read(in);
    }
    CHECK(readBack.text() == QStringLiteral("one") && readBack.whatsThis() == QStringLiteral("what"));

    QTableWidgetItem tableItem(QStringLiteral("cell"), QTableWidgetItem::UserType + 1);
    tableItem.setSizeHint(QSize(40, 20));
    tableItem.setStatusTip(QStringLiteral("s"));
    tableItem.setWhatsThis(QStringLiteral("w"));
    CHECK(tableItem.type() == QTableWidgetItem::UserType + 1 && tableItem.sizeHint() == QSize(40, 20) &&
          tableItem.statusTip() == QStringLiteral("s") && tableItem.whatsThis() == QStringLiteral("w"));
    std::unique_ptr<QTableWidgetItem> tableCopy(tableItem.clone());
    CHECK(tableCopy->text() == QStringLiteral("cell") && tableCopy->type() == tableItem.type() &&
          tableCopy->whatsThis() == QStringLiteral("w"));
    bytes.clear();
    {
        QDataStream out(&bytes, QIODevice::WriteOnly);
        tableItem.write(out);
    }
    QTableWidgetItem tableBack;
    {
        QDataStream in(bytes);
        tableBack.read(in);
    }
    CHECK(tableBack.text() == QStringLiteral("cell") && tableBack.statusTip() == QStringLiteral("s"));

    QTreeWidget tree;
    auto *treeItem = new QTreeWidgetItem(&tree, {QStringLiteral("a"), QStringLiteral("b")});
    treeItem->setStatusTip(1, QStringLiteral("tip"));
    treeItem->setWhatsThis(0, QStringLiteral("help"));
    treeItem->setSizeHint(0, QSize(10, 30));
    CHECK(treeItem->statusTip(1) == QStringLiteral("tip") && treeItem->whatsThis(0) == QStringLiteral("help") &&
          treeItem->sizeHint(0) == QSize(10, 30));
    bytes.clear();
    {
        QDataStream out(&bytes, QIODevice::WriteOnly);
        treeItem->write(out);
    }
    QTreeWidgetItem treeBack;
    {
        QDataStream in(bytes);
        treeBack.read(in);
    }
    CHECK(treeBack.text(1) == QStringLiteral("b") && treeBack.statusTip(1) == QStringLiteral("tip"));

    // Table view spans and headers
    QStandardItemModel model(4, 4);
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            model.setItem(r, c, new QStandardItem(QStringLiteral("%1,%2").arg(r).arg(c)));
    QTableView view;
    view.setModel(&model);
    view.resize(400, 300);
    view.show();
    settle();
    view.setSpan(0, 0, 2, 3);
    CHECK(view.rowSpan(0, 0) == 2 && view.columnSpan(0, 0) == 3 && view.rowSpan(1, 1) == 1);
    const QRect spanned = view.visualRect(model.index(0, 0));
    CHECK(spanned.width() == 3 * view.columnWidth(0) && spanned.height() == 2 * view.rowHeight(0));
    CHECK(view.visualRect(model.index(1, 2)).isEmpty() && view.visualRect(model.index(2, 0)).isValid());
    CHECK(view.indexAt(view.visualRect(model.index(0, 0)).bottomRight() - QPoint(2, 2)) == model.index(0, 0));
    view.setSpan(0, 1, 1, 1); // overlaps: removes the span
    CHECK(view.rowSpan(0, 0) == 1);
    view.setSpan(2, 2, 2, 2);
    view.clearSpans();
    CHECK(view.columnSpan(2, 2) == 1);
    auto *header = new QHeaderView(Qt::Horizontal);
    view.setHorizontalHeader(header);
    CHECK(view.horizontalHeader() == header && header->count() == 4);
    auto *wrong = new QHeaderView(Qt::Horizontal);
    view.setVerticalHeader(wrong); // the wrong orientation is refused
    CHECK(view.verticalHeader() != wrong);
    delete wrong;
}

// Reaches the protected parts of QHeaderView.
class OpenHeader : public QHeaderView {
public:
    using QHeaderView::QHeaderView;
    using QHeaderView::sectionSizeFromContents;
};

static void testHeaderViewExtras() {
    QStandardItemModel model(2, 3);
    model.setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("A much longer label"), QStringLiteral("C")});
    OpenHeader header(Qt::Horizontal);
    QSignalSpy counts(&header, &QHeaderView::sectionCountChanged);
    header.setModel(&model);
    CHECK(header.model() == &model && header.count() == 3 && counts.count() == 1);
    model.setHorizontalHeaderItem(2, new QStandardItem(QStringLiteral("Changed")));
    CHECK(header.sectionSizeFromContents(2).width() > 0);
    model.insertColumn(3);
    CHECK(header.count() == 4);
    CHECK(header.sectionSizeHint(1) > header.sectionSizeHint(0));
    header.resize(400, 30);
    header.resizeSections(QHeaderView::ResizeToContents);
    CHECK(header.sectionSize(1) == header.sectionSizeHint(1));
    header.setMaximumSectionSize(50);
    CHECK(header.maximumSectionSize() == 50 && header.sectionSize(1) == 50);
    header.resizeSection(0, 500);
    CHECK(header.sectionSize(0) == 50);
    header.setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    CHECK(header.defaultAlignment() == (Qt::AlignLeft | Qt::AlignVCenter));
    CHECK(header.sectionViewportPosition(1) == header.sectionPosition(1) && header.visualIndexAt(5) == 0);
    header.swapSections(0, 1);
    CHECK(header.logicalIndex(0) == 1 && header.logicalIndex(1) == 0);
    header.setFirstSectionMovable(false);
    header.moveSection(0, 2);
    CHECK(header.logicalIndex(0) == 1);
    header.setFirstSectionMovable(true);
    header.setOffsetToSectionPosition(2);
    CHECK(header.offset() == header.sectionPosition(header.logicalIndex(2)));
    header.setOffset(0);
    header.hideSection(3);
    CHECK(header.sectionsHidden());
    // A clearable sort indicator goes off on the third click
    header.setSectionsClickable(true);
    header.setSortIndicatorShown(true);
    QSignalSpy clearable(&header, &QHeaderView::sortIndicatorClearableChanged);
    header.setSortIndicatorClearable(true);
    CHECK(header.isSortIndicatorClearable() && clearable.count() == 1);
    header.show();
    settle();
    const QPoint inFirst(header.sectionViewportPosition(header.logicalIndex(0)) + 5, 10);
    QSignalSpy pressed(&header, &QHeaderView::sectionPressed);
    for (int i = 0; i < 3; ++i) {
        QMouseEvent down(QEvent::MouseButtonPress, QPointF(inFirst), QPointF(inFirst), Qt::LeftButton, Qt::LeftButton,
                         Qt::NoModifier);
        QMouseEvent up(QEvent::MouseButtonRelease, QPointF(inFirst), QPointF(inFirst), Qt::LeftButton, Qt::NoButton,
                       Qt::NoModifier);
        QCoreApplication::sendEvent(&header, &down);
        QCoreApplication::sendEvent(&header, &up);
    }
    CHECK(pressed.count() == 3 && header.sortIndicatorSection() == -1);
    QSignalSpy doubled(&header, &QHeaderView::sectionDoubleClicked);
    QMouseEvent dbl(QEvent::MouseButtonDblClick, QPointF(inFirst), QPointF(inFirst), Qt::LeftButton, Qt::LeftButton,
                    Qt::NoModifier);
    QCoreApplication::sendEvent(&header, &dbl);
    CHECK(doubled.count() == 1);
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    testFileSystemModel();
    testGraphicsLayoutsAndTransforms();
    testSmallerClasses();
    testColumnView();
    testSystemTrayIcon();
    testDrawUtilAndAnimation();
    testWidgetExtras();
    testFocusExtras();
    testLineEditExtras();
    testTabsAndMainWindow();
    testMenuBarCornersAndTreeAnimation();
    testTextEditExtras();
    testApplicationAndMenuExtras();
    testItemViewExtras();
    testTreeMenuBarComboExtras();
    testItemWidgetExtras();
    testButtonExtras();
    testSmallWidgetExtras();
    testSpinBoxExtras();
    testScrollAreaCompleterExtras();
    testLayoutExtras();
    testInputMethodAndGraphicsItemExtras();
    testItemClassAndTableViewExtras();
    testHeaderViewExtras();
    testLabelAndSmallClassExtras();
    std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}

#include "test_widgets_more.moc" // NOLINT(bugprone-suspicious-include): generated by ntmoc
