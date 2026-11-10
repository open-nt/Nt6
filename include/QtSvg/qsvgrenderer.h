// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtSvg/qtsvgglobal.h>

#include <memory>

class QPainter;

class QSvgRenderer : public QObject {
    Q_OBJECT
    Q_PROPERTY(QRectF viewBox READ viewBoxF WRITE setViewBox)
    Q_PROPERTY(int framesPerSecond READ framesPerSecond WRITE setFramesPerSecond)
    Q_PROPERTY(int currentFrame READ currentFrame WRITE setCurrentFrame)
    Q_PROPERTY(Qt::AspectRatioMode aspectRatioMode READ aspectRatioMode WRITE setAspectRatioMode)
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled)
public:
    explicit QSvgRenderer(QObject *parent = nullptr);
    explicit QSvgRenderer(const QString &filename, QObject *parent = nullptr);
    explicit QSvgRenderer(const QByteArray &contents, QObject *parent = nullptr);
    ~QSvgRenderer() override;

    bool isValid() const;
    QSize defaultSize() const;

    QRect viewBox() const;
    QRectF viewBoxF() const;
    void setViewBox(const QRect &viewbox);
    void setViewBox(const QRectF &viewbox);

    Qt::AspectRatioMode aspectRatioMode() const;
    void setAspectRatioMode(Qt::AspectRatioMode mode);

    QtSvg::Options options() const;
    void setOptions(QtSvg::Options flags);

    bool animated() const;
    int framesPerSecond() const;
    void setFramesPerSecond(int num);
    int currentFrame() const;
    void setCurrentFrame(int frame);
    int animationDuration() const;
    bool isAnimationEnabled() const;
    void setAnimationEnabled(bool enable);

    QRectF boundsOnElement(const QString &id) const;
    bool elementExists(const QString &id) const;

public slots:
    bool load(const QString &filename);
    bool load(const QByteArray &contents);
    void render(QPainter *painter);
    void render(QPainter *painter, const QRectF &bounds);
    void render(QPainter *painter, const QString &elementId, const QRectF &bounds = QRectF());

signals:
    void repaintNeeded();

private:
    struct Data;
    std::unique_ptr<Data> d;
};
