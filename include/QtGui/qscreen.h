// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>

class QPlatformScreen;

namespace QtPrivate {
struct GuiAccess;
}

class QScreen : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(int depth READ depth)
    Q_PROPERTY(QSize size READ size)
    Q_PROPERTY(QRect geometry READ geometry NOTIFY geometryChanged)
    Q_PROPERTY(QRect availableGeometry READ availableGeometry)
    Q_PROPERTY(qreal logicalDotsPerInch READ logicalDotsPerInch)
    Q_PROPERTY(qreal devicePixelRatio READ devicePixelRatio)
public:
    ~QScreen() override;

    QString name() const;
    int depth() const;
    QSize size() const;
    QRect geometry() const;
    QRect availableGeometry() const;
    QSizeF physicalSize() const;
    qreal logicalDotsPerInchX() const;
    qreal logicalDotsPerInchY() const;
    qreal logicalDotsPerInch() const;
    qreal physicalDotsPerInch() const;
    qreal devicePixelRatio() const;

signals:
    /*!
        This signal is emitted when the geometry of the screen changes to
        \a geometry.
    */
    void geometryChanged(const QRect &geometry) { emitSignal(&QScreen::geometryChanged, geometry); }

private:
    friend struct QtPrivate::GuiAccess;
    explicit QScreen(QPlatformScreen *screen);
    QPlatformScreen *m_platform;
};
