// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtGui/qpagedpaintdevice.h>

#include <memory>

class QIODevice;

class QPdfWriter : public QObject, public QPagedPaintDevice {
    Q_OBJECT
public:
    explicit QPdfWriter(const QString &filename);
    explicit QPdfWriter(QIODevice *device);
    ~QPdfWriter() override;

    void setPdfVersion(QPagedPaintDevice::PdfVersion version);
    QPagedPaintDevice::PdfVersion pdfVersion() const;

    QString title() const;
    void setTitle(const QString &title);

    QString creator() const;
    void setCreator(const QString &creator);

    bool newPage() override;

    void setResolution(int resolution);
    int resolution() const;

protected:
    QPaintEngine *paintEngine() const override;
    int metric(PaintDeviceMetric id) const override;

private:
    Q_DISABLE_COPY(QPdfWriter)
    struct Data;
    std::unique_ptr<Data> m_d;
};
