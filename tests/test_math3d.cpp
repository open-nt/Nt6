// MIT License - see LICENSE
//
// Tests for the 3D math classes of Nt GUI: QVector2D, QVector3D, QVector4D, QQuaternion and QMatrix4x4.
#include <QtGui/QMatrix4x4>
#include <QtGui/QQuaternion>
#include <QtGui/QTransform>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <QtGui/QVector4D>

#include <cmath>
#include <cstdio>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static bool near(float a, float b, float tolerance = 1e-5f) { return std::fabs(a - b) <= tolerance; }

static bool nearVec(const QVector3D &a, const QVector3D &b, float tolerance = 1e-5f) {
    const bool ok = near(a.x(), b.x(), tolerance) && near(a.y(), b.y(), tolerance) && near(a.z(), b.z(), tolerance);
    if (!ok)
        std::printf("  vector is (%g, %g, %g), expected (%g, %g, %g)\n", double(a.x()), double(a.y()), double(a.z()),
                    double(b.x()), double(b.y()), double(b.z()));
    return ok;
}

static bool nearMatrix(const QMatrix4x4 &a, const QMatrix4x4 &b, float tolerance = 1e-5f) {
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            if (!near(a(r, c), b(r, c), tolerance)) {
                std::printf("  element (%d, %d) is %g, expected %g\n", r, c, double(a(r, c)), double(b(r, c)));
                return false;
            }
    return true;
}

static void testVector2D() {
    constexpr QVector2D a(3.0f, 4.0f);
    static_assert(a.lengthSquared() == 25.0f, "constexpr lengthSquared");
    CHECK(a.length() == 5.0f);
    CHECK(near(a.normalized().x(), 0.6f) && near(a.normalized().y(), 0.8f));
    CHECK(QVector2D::dotProduct(a, QVector2D(1, 0)) == 3.0f);
    CHECK(a + QVector2D(1, 1) == QVector2D(4, 5));
    CHECK(a - QVector2D(1, 1) == QVector2D(2, 3));
    CHECK(a * 2.0f == QVector2D(6, 8));
    CHECK(2.0f * a == QVector2D(6, 8));
    CHECK(a * QVector2D(2, 3) == QVector2D(6, 12));
    CHECK(a / 2.0f == QVector2D(1.5f, 2.0f));
    CHECK(-a == QVector2D(-3, -4));
    CHECK(QVector2D().isNull());
    CHECK(!a.isNull());
    CHECK(QVector2D().normalized().isNull());
    CHECK(QVector2D(QPoint(2, 5)) == QVector2D(2, 5));
    CHECK(QVector2D(2.6f, -1.4f).toPoint() == QPoint(3, -1));
    CHECK(a.toPointF() == QPointF(3, 4));
    CHECK(near(QVector2D(0, 5).distanceToLine(QVector2D(0, 0), QVector2D(1, 0)), 5.0f));
    CHECK(near(a.distanceToPoint(QVector2D(0, 0)), 5.0f));
    CHECK(qFuzzyCompare(QVector2D(1.0f, 0.0f), QVector2D(1.000001f, 0.0f)));
    CHECK(!qFuzzyCompare(QVector2D(1.0f, 0.0f), QVector2D(1.1f, 0.0f)));
    QVector2D b(1, 2);
    b[1] = 7;
    b.setX(9);
    CHECK(b.x() == 9.0f && b[1] == 7.0f);
    CHECK(a.toVector3D() == QVector3D(3, 4, 0));
    CHECK(a.toVector4D() == QVector4D(3, 4, 0, 0));
}

static void testVector3D() {
    const QVector3D x(1, 0, 0), y(0, 1, 0), z(0, 0, 1);
    CHECK(QVector3D::crossProduct(x, y) == z);
    CHECK(QVector3D::crossProduct(y, z) == x);
    CHECK(QVector3D::dotProduct(QVector3D(1, 2, 3), QVector3D(4, 5, 6)) == 32.0f);
    CHECK(QVector3D::normal(QVector3D(0, 0, 0), QVector3D(2, 0, 0), QVector3D(0, 3, 0)) == z);
    CHECK(QVector3D::normal(QVector3D(2, 0, 0), QVector3D(0, 3, 0)) == z);
    CHECK(near(QVector3D(2, 3, 6).length(), 7.0f));
    QVector3D v(0, 0, 4);
    v.normalize();
    CHECK(v == z);
    CHECK(near(QVector3D(1, 2, 5).distanceToPlane(QVector3D(0, 0, 1), z), 4.0f));
    CHECK(near(QVector3D(1, 2, -5).distanceToPlane(QVector3D(0, 0, 0), QVector3D(1, 0, 0), QVector3D(0, 1, 0)), -5.0f));
    CHECK(near(QVector3D(0, 3, 4).distanceToLine(QVector3D(0, 0, 0), x), 5.0f));
    CHECK(near(QVector3D(1, 1, 1).distanceToPoint(QVector3D(1, 1, 4)), 3.0f));
    CHECK(QVector3D(QVector2D(1, 2), 3) == QVector3D(1, 2, 3));
    CHECK(QVector3D(QVector4D(1, 2, 3, 4)) == QVector3D(1, 2, 3));
    CHECK(QVector3D(1, 2, 3).toVector4D() == QVector4D(1, 2, 3, 0));
    CHECK(QVector3D(1, 2, 3) * QVector3D(2, 2, 2) == QVector3D(2, 4, 6));
    CHECK(QVector3D(2, 4, 6) / QVector3D(2, 2, 2) == QVector3D(1, 2, 3));
    CHECK(qFuzzyCompare(QVector3D(0, 0, 1), QVector3D(1e-7f, 0, 1)));

    // project() and unproject() are inverses.
    QMatrix4x4 projection;
    projection.perspective(60.0f, 4.0f / 3.0f, 0.1f, 100.0f);
    QMatrix4x4 modelView;
    modelView.lookAt(QVector3D(0, 0, 5), QVector3D(0, 0, 0), QVector3D(0, 1, 0));
    const QRect viewport(0, 0, 640, 480);
    const QVector3D origin = QVector3D(0, 0, 0).project(modelView, projection, viewport);
    CHECK(nearVec(origin.toVector2D().toVector3D(), QVector3D(320, 240, 0), 1e-3f));
    CHECK(origin.z() > 0.0f && origin.z() < 1.0f);
    const QVector3D p(0.5f, -0.25f, 1.0f);
    CHECK(nearVec(p.project(modelView, projection, viewport).unproject(modelView, projection, viewport), p, 1e-3f));
}

static void testVector4D() {
    const QVector4D v(2, 4, 6, 2);
    CHECK(v.toVector3DAffine() == QVector3D(1, 2, 3));
    CHECK(v.toVector2DAffine() == QVector2D(1, 2));
    CHECK(QVector4D(1, 2, 3, 0).toVector3DAffine().isNull());
    CHECK(v.toVector3D() == QVector3D(2, 4, 6));
    CHECK(QVector4D(QVector3D(1, 2, 3), 1) == QVector4D(1, 2, 3, 1));
    CHECK(QVector4D(QVector2D(1, 2), 3, 4) == QVector4D(1, 2, 3, 4));
    CHECK(QVector4D::dotProduct(v, QVector4D(1, 1, 1, 1)) == 14.0f);
    CHECK(near(QVector4D(1, 1, 1, 1).length(), 2.0f));
    CHECK(near(QVector4D(1, 1, 1, 1).normalized().w(), 0.5f));
    CHECK(v.w() == 2.0f);
}

static void testQuaternion() {
    const QQuaternion identity;
    CHECK(identity.isIdentity());
    CHECK(!identity.isNull());
    CHECK(QQuaternion(0, 0, 0, 0).isNull());

    const QQuaternion yaw = QQuaternion::fromAxisAndAngle(0, 1, 0, 90);
    CHECK(nearVec(yaw * QVector3D(1, 0, 0), QVector3D(0, 0, -1)));
    CHECK(nearVec(yaw.rotatedVector(QVector3D(0, 0, 1)), QVector3D(1, 0, 0)));
    CHECK(near(yaw.length(), 1.0f));

    QVector3D axis;
    float angle = 0.0f;
    yaw.getAxisAndAngle(&axis, &angle);
    CHECK(nearVec(axis, QVector3D(0, 1, 0)));
    CHECK(near(angle, 90.0f, 1e-4f));
    identity.getAxisAndAngle(&axis, &angle);
    CHECK(axis.isNull() && angle == 0.0f);

    // Rotations compose right to left.
    const QQuaternion pitch = QQuaternion::fromAxisAndAngle(1, 0, 0, 90);
    CHECK(nearVec((yaw * pitch) * QVector3D(0, 1, 0), yaw * (pitch * QVector3D(0, 1, 0))));
    CHECK(qFuzzyCompare(yaw * yaw.inverted(), identity));
    CHECK(qFuzzyCompare(yaw * yaw.conjugated(), identity));

    // Euler angles round trip.
    const QQuaternion e = QQuaternion::fromEulerAngles(10, 20, 30);
    const QVector3D angles = e.toEulerAngles();
    CHECK(nearVec(angles, QVector3D(10, 20, 30), 1e-3f));
    CHECK(qFuzzyCompare(QQuaternion::fromEulerAngles(angles), e));
    // fromEulerAngles applies roll, then pitch, then yaw.
    const QQuaternion composed = QQuaternion::fromAxisAndAngle(0, 1, 0, 20) *
                                 QQuaternion::fromAxisAndAngle(1, 0, 0, 10) *
                                 QQuaternion::fromAxisAndAngle(0, 0, 1, 30);
    CHECK(nearVec(composed * QVector3D(1, 2, 3), e * QVector3D(1, 2, 3), 1e-4f));
    float p = 0, y = 0, r = 0;
    QQuaternion::fromEulerAngles(90, 30, 0).getEulerAngles(&p, &y, &r);
    CHECK(near(p, 90.0f, 1e-3f) && near(y, 30.0f, 1e-3f) && near(r, 0.0f));
    QQuaternion::fromEulerAngles(-90, 30, 0).getEulerAngles(&p, &y, &r);
    CHECK(near(p, -90.0f, 1e-3f) && near(y, 30.0f, 1e-3f) && near(r, 0.0f));
    QQuaternion::fromEulerAngles(-45, -120, 170).getEulerAngles(&p, &y, &r);
    CHECK(near(p, -45.0f, 1e-3f) && near(y, -120.0f, 1e-3f) && near(r, 170.0f, 1e-3f));

    // Axes.
    QVector3D xa, ya, za;
    yaw.getAxes(&xa, &ya, &za);
    CHECK(nearVec(xa, QVector3D(0, 0, -1)) && nearVec(ya, QVector3D(0, 1, 0)) && nearVec(za, QVector3D(1, 0, 0)));
    CHECK(qFuzzyCompare(QQuaternion::fromAxes(xa, ya, za), yaw));
    const QQuaternion turned = QQuaternion::fromAxes(QVector3D(0, 1, 0), QVector3D(0, 0, 1), QVector3D(1, 0, 0));
    CHECK(nearVec(turned * QVector3D(1, 0, 0), QVector3D(0, 1, 0)));

    const QQuaternion dir = QQuaternion::fromDirection(QVector3D(1, 0, 0), QVector3D(0, 1, 0));
    CHECK(nearVec(dir * QVector3D(0, 0, 1), QVector3D(1, 0, 0)));
    CHECK(nearVec(dir * QVector3D(0, 1, 0), QVector3D(0, 1, 0)));
    CHECK(QQuaternion::fromDirection(QVector3D(), QVector3D(0, 1, 0)).isIdentity());

    const QQuaternion to = QQuaternion::rotationTo(QVector3D(1, 0, 0), QVector3D(0, 2, 0));
    CHECK(nearVec(to * QVector3D(1, 0, 0), QVector3D(0, 1, 0)));
    const QQuaternion opposite = QQuaternion::rotationTo(QVector3D(1, 0, 0), QVector3D(-1, 0, 0));
    CHECK(nearVec(opposite * QVector3D(1, 0, 0), QVector3D(-1, 0, 0)));

    // Interpolation.
    const QQuaternion half = QQuaternion::slerp(identity, yaw, 0.5f);
    CHECK(qFuzzyCompare(half, QQuaternion::fromAxisAndAngle(0, 1, 0, 45)));
    CHECK(QQuaternion::slerp(identity, yaw, 0.0f) == identity);
    CHECK(QQuaternion::slerp(identity, yaw, 1.0f) == yaw);
    CHECK(qFuzzyCompare(QQuaternion::nlerp(identity, yaw, 0.5f), half));
    // The shorter arc: -yaw is the same rotation.
    CHECK(qFuzzyCompare(QQuaternion::slerp(identity, -yaw, 0.5f), half));

    QQuaternion q(2, 0, 0, 0);
    q.normalize();
    CHECK(q.isIdentity());
    CHECK(QQuaternion(QVector4D(1, 2, 3, 4)) == QQuaternion(4, 1, 2, 3));
    CHECK(QQuaternion(4, 1, 2, 3).toVector4D() == QVector4D(1, 2, 3, 4));
    CHECK(QQuaternion::dotProduct(QQuaternion(1, 2, 3, 4), QQuaternion(1, 1, 1, 1)) == 10.0f);
}

static void testMatrix() {
    QMatrix4x4 m;
    CHECK(m.isIdentity());
    CHECK(m.isAffine());
    CHECK(m.determinant() == 1.0);

    // Row-major constructors, column-major storage.
    const QMatrix4x4 a(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
    CHECK(a(0, 1) == 2.0f && a(1, 0) == 5.0f);
    CHECK(a.constData()[1] == 5.0f && a.constData()[4] == 2.0f);
    CHECK(a.row(1) == QVector4D(5, 6, 7, 8));
    CHECK(a.column(1) == QVector4D(2, 6, 10, 14));
    const float values[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    CHECK(QMatrix4x4(values) == a);
    float copy[16] = {};
    a.copyDataTo(copy);
    CHECK(copy[1] == 2.0f && copy[4] == 5.0f);
    CHECK(a.transposed()(0, 1) == 5.0f);
    CHECK(a.determinant() == 0.0);
    bool invertible = true;
    CHECK(a.inverted(&invertible).isIdentity() && !invertible);

    // glTF stores matrices column-major; QMatrix4x4(const float *) is row-major, hence transposed().
    const float gltf[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, 6, 7, 1};
    const QMatrix4x4 fromGltf = QMatrix4x4(gltf).transposed();
    CHECK(fromGltf * QVector3D(0, 0, 0) == QVector3D(5, 6, 7));

    // translate/rotate/scale apply right to left.
    m.translate(10, 0, 0);
    m.rotate(90, 0, 0, 1);
    m.scale(2);
    CHECK(nearVec(m * QVector3D(1, 0, 0), QVector3D(10, 2, 0)));
    CHECK(nearVec(m.mapVector(QVector3D(1, 0, 0)), QVector3D(0, 2, 0)));
    QMatrix4x4 r;
    r.rotate(90, 0, 0, 1);
    CHECK(r(0, 0) == 0.0f && r(0, 1) == -1.0f && r(1, 0) == 1.0f); // exact for multiples of 90

    QMatrix4x4 inv = m.inverted(&invertible);
    CHECK(invertible);
    CHECK(nearMatrix(m * inv, QMatrix4x4()));
    CHECK(near(float(m.determinant()), 8.0f));
    CHECK(!a.isIdentity());

    // Quaternion rotation matches the axis-angle one.
    QMatrix4x4 qr;
    qr.rotate(QQuaternion::fromAxisAndAngle(QVector3D(1, 1, 0), 33));
    QMatrix4x4 ar;
    ar.rotate(33, QVector3D(1, 1, 0));
    CHECK(nearMatrix(qr, ar));

    // Projections.
    QMatrix4x4 proj;
    proj.perspective(90.0f, 1.0f, 1.0f, 10.0f);
    CHECK(nearVec(proj * QVector3D(0, 0, -1), QVector3D(0, 0, -1)));
    CHECK(nearVec(proj * QVector3D(0, 0, -10), QVector3D(0, 0, 1)));
    CHECK(nearVec(proj * QVector3D(1, 1, -1), QVector3D(1, 1, -1)));
    CHECK(!proj.isAffine());
    QMatrix4x4 fr;
    fr.frustum(-1, 1, -1, 1, 1, 10);
    CHECK(nearMatrix(fr, proj));

    QMatrix4x4 ortho;
    ortho.ortho(QRect(0, 0, 200, 100));
    CHECK(nearVec(ortho * QVector3D(0, 0, 0), QVector3D(-1, 1, 0)));
    CHECK(nearVec(ortho * QVector3D(200, 100, 0), QVector3D(1, -1, 0)));
    QMatrix4x4 orthoF;
    orthoF.ortho(QRectF(0, 0, 200, 100));
    CHECK(nearMatrix(ortho, orthoF));

    QMatrix4x4 view;
    view.lookAt(QVector3D(0, 0, 5), QVector3D(0, 0, 0), QVector3D(0, 1, 0));
    CHECK(nearVec(view * QVector3D(0, 0, 0), QVector3D(0, 0, -5)));
    QMatrix4x4 side;
    side.lookAt(QVector3D(5, 0, 0), QVector3D(0, 0, 0), QVector3D(0, 1, 0));
    CHECK(nearVec(side * QVector3D(0, 0, 0), QVector3D(0, 0, -5)));
    CHECK(nearVec(side * QVector3D(0, 0, 1), QVector3D(-1, 0, -5))); // +z is on the left looking down -x

    QMatrix4x4 vp;
    vp.viewport(QRectF(0, 0, 640, 480));
    CHECK(nearVec(vp * QVector3D(-1, -1, -1), QVector3D(0, 0, 0)));
    CHECK(nearVec(vp * QVector3D(1, 1, 1), QVector3D(640, 480, 1)));

    QMatrix4x4 flip;
    flip.flipCoordinates();
    CHECK(flip * QVector3D(1, 2, 3) == QVector3D(1, -2, -3));

    // Points, rects and QTransform.
    QMatrix4x4 t;
    t.translate(5, 6);
    t.scale(2, 3);
    CHECK(t.map(QPointF(1, 1)) == QPointF(7, 9));
    CHECK(t * QPoint(1, 1) == QPoint(7, 9));
    CHECK(t.mapRect(QRectF(0, 0, 10, 10)) == QRectF(5, 6, 20, 30));
    CHECK(t.mapRect(QRect(0, 0, 10, 10)) == QRect(5, 6, 20, 30));
    const QTransform tr = t.toTransform();
    CHECK(tr.map(QPointF(1, 1)) == QPointF(7, 9));
    CHECK(QMatrix4x4(tr) == t);
    CHECK(t.toTransform(0.0f).map(QPointF(1, 1)) == QPointF(7, 9));
    const QPointF rotated = QMatrix4x4(QTransform().rotate(30)).map(QPointF(1, 0));
    CHECK(near(float(rotated.x()), std::sqrt(3.0f) / 2.0f) && near(float(rotated.y()), 0.5f));

    // Vector times matrix uses the transpose.
    CHECK(QVector4D(1, 0, 0, 0) * a == QVector4D(1, 2, 3, 4));
    CHECK(a * QVector4D(1, 0, 0, 0) == QVector4D(1, 5, 9, 13));

    // Arithmetic and element access.
    QMatrix4x4 s = a + a;
    CHECK(s(3, 3) == 32.0f);
    CHECK((s - a) == a);
    CHECK((a * 2.0f) == s && (2.0f * a) == s && (s / 2.0f) == a);
    CHECK((-a)(0, 0) == -1.0f);
    s.fill(1.0f);
    CHECK(s(2, 1) == 1.0f && s != a);
    s.setRow(0, QVector4D(9, 8, 7, 6));
    CHECK(s(0, 3) == 6.0f);
    s.setColumn(0, QVector4D(0, 0, 0, 0));
    CHECK(s(0, 0) == 0.0f && s(0, 1) == 8.0f);
    s(3, 3) = 42.0f;
    CHECK(s.data()[15] == 42.0f);
    s.setToIdentity();
    s.optimize();
    CHECK(s.isIdentity());
    CHECK(qFuzzyCompare(QMatrix4x4(), m * inv));
}

int main() {
    testVector2D();
    testVector3D();
    testVector4D();
    testQuaternion();
    testMatrix();
    if (g_failures) {
        std::printf("%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("all math3d tests passed\n");
    return 0;
}
