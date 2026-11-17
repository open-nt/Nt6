// MIT License - see LICENSE
//
// Tests for more of Nt Widgets: the file system model and its icon provider, graphics layouts and transforms, and
// the smaller classes. The widgets are on the offscreen platform.
#include <QtCore/QtCore>
#include <QtGui/QtGui>
#include <QtTest/qsignalspy.h>
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
    std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}

#include "test_widgets_more.moc" // NOLINT(bugprone-suspicious-include): generated by ntmoc
