// MIT License - see LICENSE
//
// Graphics View on the offscreen platform: the item tree and its transformations, the scene's hit tests, focus,
// selection and event dispatch, and what a QGraphicsView draws and passes on.
#include <QtTest>

#include <QtGui/qevent.h>
#include <QtGui/qfontmetrics.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qgraphicseffect.h>
#include <QtWidgets/qgraphicsitem.h>
#include <QtWidgets/qgraphicsproxywidget.h>
#include <QtWidgets/qgraphicsscene.h>
#include <QtWidgets/qgraphicssceneevent.h>
#include <QtWidgets/qgraphicsview.h>
#include <QtWidgets/qstyleoption.h>

#include "platforms/offscreen/qoffscreenintegration_p.h"

namespace {

// An item that records what happens to it.
class Probe : public QGraphicsObject {
public:
    explicit Probe(const QRectF &rect = QRectF(0, 0, 40, 40), QGraphicsItem *parent = nullptr)
        : QGraphicsObject(parent), m_rect(rect) {}

    QStringList log;
    QPointF lastPos;
    int paints = 0;
    QColor color = Qt::red;
    bool acceptPress = true;
    QList<GraphicsItemChange> changes;

    QRectF boundingRect() const override { return m_rect; }
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *) override {
        ++paints;
        lastState = option->state;
        painter->fillRect(m_rect, color);
    }
    QStyle::State lastState;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *e) override {
        log << QStringLiteral("press");
        lastPos = e->pos();
        if (acceptPress)
            e->accept();
        else
            e->ignore();
    }
    void mouseMoveEvent(QGraphicsSceneMouseEvent *e) override {
        log << QStringLiteral("move");
        lastPos = e->pos();
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override {
        log << QStringLiteral("release");
        lastPos = e->pos();
    }
    void hoverEnterEvent(QGraphicsSceneHoverEvent *) override { log << QStringLiteral("enter"); }
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *) override { log << QStringLiteral("leave"); }
    void keyPressEvent(QKeyEvent *e) override { log << QStringLiteral("key ") + e->text(); }
    void focusInEvent(QFocusEvent *) override { log << QStringLiteral("focusin"); }
    void focusOutEvent(QFocusEvent *) override { log << QStringLiteral("focusout"); }
    void wheelEvent(QGraphicsSceneWheelEvent *e) override {
        log << QStringLiteral("wheel %1").arg(e->delta());
        e->accept();
    }
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override {
        changes << change;
        if (change == ItemPositionChange && value.toPointF().x() < 0)
            return QPointF(0, value.toPointF().y()); // keep out of negative x
        return value;
    }

private:
    QRectF m_rect;
};

// A widget that fills itself blue and records presses.
class Pad : public QWidget {
public:
    QList<QPoint> presses;
    QColor fill = Qt::blue;

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(rect(), fill);
    }
    void mousePressEvent(QMouseEvent *e) override { presses << e->position().toPoint(); }
};

QImage grab(QGraphicsView &view) { return QOffscreenBackingStore::imageOf(view.windowHandle()); }

} // namespace

class TestGraphicsView : public QObject {
    Q_OBJECT
private slots:
    void itemTreeAndTransforms();
    void itemChangeAndSignals();
    void sceneItemsAndHitTests();
    void shapeItems();
    void focusSelectionAndKeys();
    void viewDrawsScene();
    void viewMouseHoverWheel();
    void fitInViewAndMapping();
    void deletionCleansUp();
    void textItems();
    void groupKeepsScenePositions();
    void proxyWidget();
    void dropShadowEffect();
    void touchEvent();
};

void TestGraphicsView::itemTreeAndTransforms() {
    QGraphicsRectItem parent(0, 0, 100, 100);
    parent.setPos(10, 20);
    auto *child = new QGraphicsRectItem(0, 0, 10, 10, &parent);
    child->setPos(5, 5);
    QCOMPARE(child->parentItem(), &parent);
    QCOMPARE(parent.childItems(), QList<QGraphicsItem *>{child});
    QCOMPARE(child->scenePos(), QPointF(15, 25));
    QCOMPARE(child->mapToScene(QPointF(1, 1)), QPointF(16, 26));
    QCOMPARE(child->mapFromScene(QPointF(16, 26)), QPointF(1, 1));
    QCOMPARE(child->sceneBoundingRect(), QRectF(14.5, 24.5, 11, 11)); // half a pen around it
    QCOMPARE(parent.topLevelItem(), &parent);
    QCOMPARE(child->topLevelItem(), &parent);
    QVERIFY(parent.isAncestorOf(child));
    QCOMPARE(qgraphicsitem_cast<QGraphicsRectItem *>(child), child);
    QCOMPARE(qgraphicsitem_cast<QGraphicsEllipseItem *>(child), nullptr);
    QCOMPARE(child->type(), int(QGraphicsRectItem::Type));

    // Rotation about the origin point, then scale.
    QGraphicsRectItem box(0, 0, 20, 10);
    box.setTransformOriginPoint(10, 5);
    box.setRotation(90);
    QCOMPARE(box.mapToScene(QPointF(10, 5)), QPointF(10, 5));
    const QPointF corner = box.mapToScene(QPointF(0, 0));
    QVERIFY(qAbs(corner.x() - 15) < 1e-9 && qAbs(corner.y() - (-5)) < 1e-9);
    box.setRotation(0);
    box.setScale(2);
    QCOMPARE(box.mapToScene(QPointF(0, 0)), QPointF(-10, -5));
    box.setScale(1);
    box.setTransform(QTransform::fromTranslate(3, 4));
    QCOMPARE(box.mapToScene(QPointF(0, 0)), QPointF(3, 4));

    // Visibility and enabled state follow the parent.
    parent.hide();
    QVERIFY(!child->isVisible());
    QVERIFY(child->isVisibleTo(&parent));
    parent.show();
    parent.setEnabled(false);
    QVERIFY(!child->isEnabled());
    parent.setEnabled(true);

    // Opacity multiplies down the tree unless a flag says otherwise.
    parent.setOpacity(0.5);
    child->setOpacity(0.5);
    QCOMPARE(child->effectiveOpacity(), 0.25);
    child->setFlag(QGraphicsItem::ItemIgnoresParentOpacity);
    QCOMPARE(child->effectiveOpacity(), 0.5);

    QCOMPARE(parent.childrenBoundingRect(), QRectF(4.5, 4.5, 11, 11));
    child->setParentItem(nullptr);
    QVERIFY(parent.childItems().isEmpty());
    delete child;
}

void TestGraphicsView::itemChangeAndSignals() {
    Probe probe;
    QSignalSpy xSpy(&probe, &QGraphicsObject::xChanged);
    QSignalSpy zSpy(&probe, &QGraphicsObject::zChanged);
    QSignalSpy opacitySpy(&probe, &QGraphicsObject::opacityChanged);
    probe.setPos(5, 0);
    QCOMPARE(xSpy.size(), 1);
    QVERIFY(!probe.changes.contains(QGraphicsItem::ItemPositionChange)); // only with ItemSendsGeometryChanges
    probe.setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    probe.setPos(-10, 3);
    QCOMPARE(probe.pos(), QPointF(0, 3)); // itemChange adjusted it
    QVERIFY(probe.changes.contains(QGraphicsItem::ItemPositionHasChanged));
    probe.setZValue(2);
    QCOMPARE(zSpy.size(), 1);
    QVERIFY(probe.changes.contains(QGraphicsItem::ItemZValueChange));
    probe.setOpacity(0.3);
    QCOMPARE(opacitySpy.size(), 1);
    probe.setData(7, QStringLiteral("seven"));
    QCOMPARE(probe.data(7).toString(), QStringLiteral("seven"));
    QVERIFY(probe.toGraphicsObject() == &probe);
}

void TestGraphicsView::sceneItemsAndHitTests() {
    QGraphicsScene scene;
    auto *low = scene.addRect(QRectF(0, 0, 100, 100), QPen(Qt::NoPen), QBrush(Qt::blue));
    auto *high = scene.addRect(QRectF(50, 50, 100, 100), QPen(Qt::NoPen), QBrush(Qt::green));
    QCOMPARE(scene.items().size(), 2);
    QCOMPARE(scene.items().first(), high); // added later, drawn on top
    QCOMPARE(scene.itemAt(QPointF(75, 75), QTransform()), high);
    high->setZValue(-1);
    QCOMPARE(scene.itemAt(QPointF(75, 75), QTransform()), low);
    QCOMPARE(scene.items(QPointF(75, 75)).size(), 2);
    QCOMPARE(scene.items(QPointF(10, 10)), QList<QGraphicsItem *>{low});
    QVERIFY(scene.items(QPointF(500, 500)).isEmpty());
    QCOMPARE(scene.itemsBoundingRect(), QRectF(0, 0, 150, 150));
    QCOMPARE(scene.sceneRect(), QRectF(0, 0, 150, 150));
    QCOMPARE(scene.items(QRectF(120, 120, 5, 5)), QList<QGraphicsItem *>{high});
    QCOMPARE(low->collidingItems(), QList<QGraphicsItem *>{high});

    // A parent that clips its children hides the parts outside it from hit tests.
    auto *clipper = scene.addRect(QRectF(200, 0, 50, 50));
    clipper->setFlag(QGraphicsItem::ItemClipsChildrenToShape);
    auto *inner = new QGraphicsRectItem(QRectF(200, 0, 100, 100), clipper);
    QCOMPARE(scene.itemAt(QPointF(210, 10), QTransform()), inner);
    QVERIFY(!scene.items(QPointF(280, 80)).contains(inner));

    QSignalSpy rectSpy(&scene, &QGraphicsScene::sceneRectChanged);
    scene.setSceneRect(0, 0, 1280, 720);
    QCOMPARE(scene.sceneRect(), QRectF(0, 0, 1280, 720));
    QCOMPARE(scene.width(), 1280.0);
    QCOMPARE(rectSpy.size(), 1);

    scene.removeItem(low);
    QCOMPARE(low->scene(), nullptr);
    QVERIFY(!scene.items().contains(low));
    delete low;
    scene.clear();
    QVERIFY(scene.items().isEmpty());
}

void TestGraphicsView::shapeItems() {
    QGraphicsEllipseItem ellipse(0, 0, 100, 50);
    QVERIFY(ellipse.contains(QPointF(50, 25)));
    QVERIFY(!ellipse.contains(QPointF(2, 2)));
    ellipse.setSpanAngle(90 * 16); // a quarter pie, top right
    QVERIFY(ellipse.contains(QPointF(70, 15)));
    QVERIFY(!ellipse.contains(QPointF(30, 35)));
    QCOMPARE(ellipse.startAngle(), 0);

    QGraphicsLineItem line(0, 0, 100, 0);
    line.setPen(QPen(Qt::black, 4));
    QVERIFY(line.contains(QPointF(50, 1)));
    QVERIFY(!line.contains(QPointF(50, 10)));
    QCOMPARE(line.boundingRect(), QRectF(-2, -2, 104, 4));

    QPolygonF triangle;
    triangle << QPointF(0, 0) << QPointF(100, 0) << QPointF(0, 100);
    QGraphicsPolygonItem polygon(triangle);
    QVERIFY(polygon.contains(QPointF(10, 10)));
    QVERIFY(!polygon.contains(QPointF(90, 90)));

    QPainterPath path;
    path.addEllipse(QRectF(0, 0, 20, 20));
    QGraphicsPathItem pathItem(path);
    QCOMPARE(pathItem.path().boundingRect(), QRectF(0, 0, 20, 20));

    QPixmap pixmap(30, 20);
    pixmap.fill(Qt::red);
    QGraphicsPixmapItem pixmapItem(pixmap);
    pixmapItem.setOffset(-15, -10);
    QCOMPARE(pixmapItem.boundingRect(), QRectF(-15, -10, 30, 20));
    QVERIFY(pixmapItem.contains(QPointF(0, 0)));

    QGraphicsSimpleTextItem text(QStringLiteral("Hello"));
    QVERIFY(text.boundingRect().width() > 0);
    QCOMPARE(text.brush().color(), QColor(Qt::black));
}

void TestGraphicsView::focusSelectionAndKeys() {
    QGraphicsScene scene;
    auto *a = new Probe;
    auto *b = new Probe(QRectF(50, 0, 40, 40));
    a->setFlag(QGraphicsItem::ItemIsFocusable);
    b->setFlags(QGraphicsItem::ItemIsFocusable | QGraphicsItem::ItemIsSelectable);
    scene.addItem(a);
    scene.addItem(b);
    QSignalSpy focusSpy(&scene, &QGraphicsScene::focusItemChanged);
    a->setFocus();
    QVERIFY(a->hasFocus());
    QCOMPARE(scene.focusItem(), a);
    QCOMPARE(focusSpy.size(), 1);
    QVERIFY(a->log.contains(QStringLiteral("focusin")));
    b->setFocus();
    QVERIFY(a->log.contains(QStringLiteral("focusout")));
    QCOMPARE(scene.focusItem(), b);

    QKeyEvent key(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, QStringLiteral("x"));
    QCoreApplication::sendEvent(&scene, &key);
    QVERIFY(b->log.contains(QStringLiteral("key x")));

    QSignalSpy selection(&scene, &QGraphicsScene::selectionChanged);
    a->setSelected(true); // not selectable
    QVERIFY(!a->isSelected());
    b->setSelected(true);
    QCOMPARE(scene.selectedItems(), QList<QGraphicsItem *>{b});
    QCOMPARE(selection.size(), 1);
    scene.clearSelection();
    QVERIFY(scene.selectedItems().isEmpty());

    b->hide(); // a hidden item loses the focus
    QCOMPARE(scene.focusItem(), nullptr);
}

void TestGraphicsView::viewDrawsScene() {
    QGraphicsScene scene(0, 0, 200, 100);
    scene.setBackgroundBrush(Qt::white);
    auto *probe = new Probe(QRectF(0, 0, 50, 50));
    probe->setPos(20, 10);
    scene.addItem(probe);
    auto *hidden = new Probe(QRectF(0, 0, 10, 10));
    hidden->hide();
    scene.addItem(hidden);

    QGraphicsView view(&scene);
    view.setFrameShape(QFrame::NoFrame);
    view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view.resize(200, 100);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(view.windowHandle()));
    QVERIFY(QTest::qWaitFor([&] { return probe->paints > 0; }));
    QCOMPARE(hidden->paints, 0);
    QImage image = grab(view);
    QCOMPARE(image.pixel(40, 30), qRgb(255, 0, 0));      // the item
    QCOMPARE(image.pixel(150, 80), qRgb(255, 255, 255)); // the scene background
    QCOMPARE(view.mapToScene(QPoint(40, 30)), QPointF(40, 30));

    // Moving the item redraws both places.
    probe->setPos(120, 10);
    QVERIFY(QTest::qWaitFor([&] { return grab(view).pixel(140, 30) == qRgb(255, 0, 0); }));
    QCOMPARE(grab(view).pixel(40, 30), qRgb(255, 255, 255));

    // Opacity and the selection state reach the painter and the option.
    probe->setOpacity(0.5);
    QVERIFY(QTest::qWaitFor([&] {
        const QRgb p = grab(view).pixel(140, 30);
        return qRed(p) == 255 && qGreen(p) > 100 && qGreen(p) < 160;
    }));
    QVERIFY(probe->lastState & QStyle::State_Enabled);
}

void TestGraphicsView::viewMouseHoverWheel() {
    QGraphicsScene scene(0, 0, 200, 100);
    auto *below = new Probe(QRectF(0, 0, 100, 100));
    auto *above = new Probe(QRectF(0, 0, 50, 50));
    above->setAcceptHoverEvents(true);
    above->setCursor(Qt::PointingHandCursor);
    scene.addItem(below);
    scene.addItem(above);

    QGraphicsView view(&scene);
    view.setFrameShape(QFrame::NoFrame);
    view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view.resize(200, 100);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(view.windowHandle()));

    // The topmost item takes the press, grabs the mouse, and gets the move and release even outside itself.
    QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(10, 12));
    QCOMPARE(above->log.last(), QStringLiteral("press"));
    QCOMPARE(above->lastPos, QPointF(10, 12));
    QCOMPARE(scene.mouseGrabberItem(), above);
    QTest::mouseMove(view.viewport(), QPoint(80, 60));
    QCOMPARE(above->log.last(), QStringLiteral("move"));
    QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(80, 60));
    QCOMPARE(above->log.last(), QStringLiteral("release"));
    QCOMPARE(scene.mouseGrabberItem(), nullptr);
    QVERIFY(below->log.isEmpty() || !below->log.contains(QStringLiteral("press")));

    // An item that ignores the press lets it through.
    above->acceptPress = false;
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(10, 12));
    QVERIFY(below->log.contains(QStringLiteral("press")));
    above->acceptPress = true;

    // Hover: enter over the item, leave when the mouse moves off it, cursor follows.
    QTest::mouseMove(view.viewport(), QPoint(150, 80)); // off every hover item first
    above->log.clear();
    QTest::mouseMove(view.viewport(), QPoint(20, 20));
    QVERIFY(above->log.contains(QStringLiteral("enter")));
    QVERIFY(above->isUnderMouse());
    QCOMPARE(view.viewport()->cursor().shape(), Qt::PointingHandCursor);
    QTest::mouseMove(view.viewport(), QPoint(150, 80));
    QVERIFY(above->log.contains(QStringLiteral("leave")));
    QVERIFY(!above->isUnderMouse());

    // The wheel goes to the items under the mouse.
    QTest::wheelEvent(view.viewport(), QPoint(20, 20), QPoint(0, 120));
    QCOMPARE(above->log.last(), QStringLiteral("wheel 120"));

    // A non-interactive view passes nothing.
    view.setInteractive(false);
    above->log.clear();
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(10, 12));
    QVERIFY(above->log.isEmpty());
}

void TestGraphicsView::fitInViewAndMapping() {
    QGraphicsScene scene(0, 0, 1280, 720);
    QGraphicsView view(&scene);
    view.setFrameShape(QFrame::NoFrame);
    view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view.resize(644, 364);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(view.windowHandle()));
    view.fitInView(scene.sceneRect(), Qt::IgnoreAspectRatio);
    QCOMPARE(view.transform().m11(), 0.5);
    QCOMPARE(view.transform().m22(), 0.5);
    QCOMPARE(view.mapFromScene(QPointF(0, 0)), QPoint(2, 2));
    QCOMPARE(view.mapFromScene(QPointF(1280, 720)), QPoint(642, 362));
    QCOMPARE(view.mapToScene(QPoint(322, 182)), QPointF(640, 360));
    view.fitInView(QRectF(0, 0, 1280, 720), Qt::KeepAspectRatio); // the same again: no drift
    QCOMPARE(view.transform().m11(), 0.5);

    // Larger than the viewport: the scroll bars range over it and centerOn() scrolls.
    view.resetTransform();
    view.centerOn(640, 360);
    QCOMPARE(view.mapToScene(QPoint(322, 182)), QPointF(640, 360));
    view.scale(2, 2);
    QVERIFY(view.isTransformed());
    view.centerOn(400, 300);
    QCOMPARE(view.mapToScene(QPoint(322, 182)), QPointF(400, 300));
    QCOMPARE(view.itemAt(QPoint(5, 5)), nullptr);
    view.rotate(90);
    view.resetTransform();
    QVERIFY(!view.isTransformed());
}

void TestGraphicsView::deletionCleansUp() {
    QGraphicsScene scene;
    auto *parent = new Probe;
    auto *child = new Probe(QRectF(0, 0, 10, 10), parent);
    child->setFlag(QGraphicsItem::ItemIsFocusable);
    scene.addItem(parent);
    QCOMPARE(child->scene(), &scene);
    child->setFocus();
    child->grabMouse();
    QCOMPARE(scene.mouseGrabberItem(), child);
    delete parent; // deletes the child too
    QCOMPARE(scene.focusItem(), nullptr);
    QCOMPARE(scene.mouseGrabberItem(), nullptr);
    QVERIFY(scene.items().isEmpty());

    // A view outlives its scene.
    auto *other = new QGraphicsScene;
    QGraphicsView view(other);
    delete other;
    QCOMPARE(view.scene(), nullptr);
}

void TestGraphicsView::textItems() {
    QGraphicsScene scene;
    QGraphicsTextItem *text = scene.addText(QStringLiteral("Status: OK"));
    QCOMPARE(text->toPlainText(), QStringLiteral("Status: OK"));
    QVERIFY(text->boundingRect().width() > 20);
    text->setDefaultTextColor(Qt::green);
    QCOMPARE(text->defaultTextColor(), QColor(Qt::green));
    text->setHtml(QStringLiteral("<b>Bold</b>"));
    QCOMPARE(text->toPlainText(), QStringLiteral("Bold"));
    text->setTextWidth(200);
    QCOMPARE(text->textWidth(), 200.0);
    QCOMPARE(text->type(), int(QGraphicsTextItem::Type));
    QGraphicsSimpleTextItem *simple = scene.addSimpleText(QStringLiteral("a\nb"));
    QVERIFY(simple->boundingRect().height() > QFontMetricsF(simple->font()).height());
}

void TestGraphicsView::groupKeepsScenePositions() {
    QGraphicsScene scene;
    auto *a = scene.addRect(QRectF(0, 0, 10, 10));
    a->setPos(100, 50);
    auto *b = scene.addRect(QRectF(0, 0, 10, 10));
    b->setPos(20, 30);
    QGraphicsItemGroup *group = scene.createItemGroup({a, b});
    QCOMPARE(a->group(), group);
    QCOMPARE(a->scenePos(), QPointF(100, 50));
    group->setPos(5, 5);
    QCOMPARE(a->scenePos(), QPointF(105, 55));
    scene.destroyItemGroup(group);
    QCOMPARE(a->group(), nullptr);
    QCOMPARE(a->scenePos(), QPointF(105, 55));
    QCOMPARE(a->parentItem(), nullptr);
}

void TestGraphicsView::proxyWidget() {
    QGraphicsScene scene(0, 0, 200, 100);
    scene.setBackgroundBrush(Qt::white);
    auto *pad = new Pad;
    pad->resize(60, 40);
    QGraphicsProxyWidget *proxy = scene.addWidget(pad);
    QCOMPARE(proxy->widget(), pad);
    QCOMPARE(proxy->type(), int(QGraphicsProxyWidget::Type));
    QCOMPARE(proxy->size(), QSizeF(60, 40));
    QVERIFY(pad->isVisible() && !pad->isHidden()); // a proxied top level counts as shown without a window
    proxy->setPos(100, 20);
    QCOMPARE(proxy->geometry(), QRectF(100, 20, 60, 40));
    proxy->resize(QSizeF(80, 50));
    QCOMPARE(pad->size(), QSize(80, 50));
    auto *child = new QWidget(pad);
    child->setGeometry(5, 6, 10, 10);
    QVERIFY(pad->isAncestorOf(child) && !child->isAncestorOf(pad) && !pad->isAncestorOf(pad));
    QCOMPARE(proxy->subWidgetRect(child), QRectF(5, 6, 10, 10));
    child->hide();

    QGraphicsView view(&scene);
    view.setFrameShape(QFrame::NoFrame);
    view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view.resize(200, 100);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(view.windowHandle()));
    QVERIFY(QTest::qWaitFor([&] { return grab(view).pixel(140, 40) == qRgb(0, 0, 255); }));
    QCOMPARE(grab(view).pixel(50, 40), qRgb(255, 255, 255));

    // A click on the proxy reaches the widget in its own coordinates.
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(110, 30));
    QCOMPARE(pad->presses.size(), 1);
    QCOMPARE(pad->presses.first(), QPoint(10, 10));

    // Repainting the widget repaints the proxy.
    pad->fill = Qt::green;
    pad->update();
    QVERIFY(QTest::qWaitFor([&] { return grab(view).pixel(140, 40) == qRgb(0, 255, 0); }));

    // Deleting the proxy deletes the widget.
    QPointer<QWidget> guard(pad);
    delete proxy;
    QVERIFY(guard.isNull());
}

void TestGraphicsView::dropShadowEffect() {
    auto *shadow = new QGraphicsDropShadowEffect;
    QCOMPARE(shadow->offset(), QPointF(8, 8));
    QCOMPARE(shadow->blurRadius(), 1.0);
    QSignalSpy offsets(shadow, &QGraphicsDropShadowEffect::offsetChanged);
    shadow->setOffset(6, 4);
    shadow->setBlurRadius(0);
    shadow->setColor(Qt::black);
    QCOMPARE(offsets.count(), 1);
    QCOMPARE(shadow->boundingRectFor(QRectF(0, 0, 10, 10)), QRectF(0, 0, 16, 14));

    QGraphicsScene scene(0, 0, 200, 100);
    scene.setBackgroundBrush(Qt::white);
    auto *probe = new Probe(QRectF(0, 0, 40, 40));
    probe->setPos(20, 20);
    QPointer<QGraphicsDropShadowEffect> guard(shadow);
    probe->setGraphicsEffect(shadow);
    QCOMPARE(probe->graphicsEffect(), shadow);
    scene.addItem(probe);

    QGraphicsView view(&scene);
    view.setFrameShape(QFrame::NoFrame);
    view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view.resize(200, 100);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(view.windowHandle()));
    QVERIFY(QTest::qWaitFor([&] { return grab(view).pixel(40, 40) == qRgb(255, 0, 0); }));
    // The shadow shows below and to the right of the item, the item covers it elsewhere.
    QCOMPARE(grab(view).pixel(63, 62), qRgb(0, 0, 0));
    QCOMPARE(grab(view).pixel(30, 30), qRgb(255, 0, 0));
    QCOMPARE(grab(view).pixel(90, 90), qRgb(255, 255, 255));

    // Disabling the effect removes the shadow; the item owns and deletes the effect.
    shadow->setEnabled(false);
    QVERIFY(QTest::qWaitFor([&] { return grab(view).pixel(63, 62) == qRgb(255, 255, 255); }));
    delete probe;
    QVERIFY(guard.isNull());
}

void TestGraphicsView::touchEvent() {
    QList<QEventPoint> points;
    points << QEventPoint(1, QEventPoint::Pressed, QPointF(5, 6), QPointF(105, 106));
    points[0].setPosition(QPointF(1, 2));
    QTouchEvent event(QEvent::TouchBegin, Qt::ShiftModifier, points);
    QCOMPARE(event.type(), QEvent::TouchBegin);
    QCOMPARE(event.modifiers(), Qt::ShiftModifier);
    QCOMPARE(event.pointCount(), 1);
    QCOMPARE(event.points()[0].id(), 1);
    QCOMPARE(event.points()[0].position(), QPointF(1, 2));
    QCOMPARE(event.points()[0].scenePosition(), QPointF(5, 6));
    QCOMPARE(event.points()[0].globalPosition(), QPointF(105, 106));
    QCOMPARE(event.touchPointStates(), Qt::TouchPointStates(Qt::TouchPointPressed));
    std::unique_ptr<QTouchEvent> copy(event.clone());
    QCOMPARE(copy->points().size(), 1);
}

QTEST_MAIN(TestGraphicsView)
#include "test_graphicsview.moc"
