// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtGui/qimage.h>

#include <memory>

class QIODevice;

class QImageWriter {
public:
    enum ImageWriterError { UnknownError, DeviceError, UnsupportedFormatError, InvalidImageError };

    QImageWriter();
    QImageWriter(QIODevice *device, const QByteArray &format);
    explicit QImageWriter(const QString &fileName, const QByteArray &format = QByteArray());
    ~QImageWriter();
    QImageWriter(const QImageWriter &) = delete;
    QImageWriter &operator=(const QImageWriter &) = delete;

    void setFormat(const QByteArray &format);
    QByteArray format() const;
    void setDevice(QIODevice *device);
    QIODevice *device() const;
    void setFileName(const QString &fileName);
    QString fileName() const;

    void setQuality(int quality);
    int quality() const;
    void setCompression(int compression);
    int compression() const;

    bool canWrite() const;
    bool write(const QImage &image);

    ImageWriterError error() const;
    QString errorString() const;

    static QList<QByteArray> supportedImageFormats();
    static QList<QByteArray> supportedMimeTypes();

private:
    struct Data;
    std::unique_ptr<Data> d;
};
