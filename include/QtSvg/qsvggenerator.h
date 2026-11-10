// MIT License - see LICENSE
#pragma once

#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtGui/qpaintdevice.h>

#include <memory>

class QIODevice;

class QSvgGenerator : public QPaintDevice {
public:
    enum class SvgVersion { SvgTiny12, Svg11 };

    QSvgGenerator();
    explicit QSvgGenerator(SvgVersion version);
    ~QSvgGenerator() override;

    QString title() const;
    void setTitle(const QString &title);
    QString description() const;
    void setDescription(const QString &description);

    QSize size() const;
    void setSize(const QSize &size);
    QRect viewBox() const;
    QRectF viewBoxF() const;
    void setViewBox(const QRect &viewBox);
    void setViewBox(const QRectF &viewBox);

    QString fileName() const;
    void setFileName(const QString &fileName);
    QIODevice *outputDevice() const;
    void setOutputDevice(QIODevice *outputDevice);

    int resolution() const;
    void setResolution(int dpi);
    SvgVersion svgVersion() const;

    QPaintEngine *paintEngine() const override;

protected:
    int metric(PaintDeviceMetric metric) const override;

private:
    struct Data;
    std::unique_ptr<Data> d;
};
