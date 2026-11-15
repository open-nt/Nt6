// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtGui/qimage.h>

#include <memory>

class QIODevice;

class QImageReader {
public:
    enum ImageReaderError { UnknownError, FileNotFoundError, DeviceError, UnsupportedFormatError, InvalidDataError };

    QImageReader();
    explicit QImageReader(QIODevice *device, const QByteArray &format = QByteArray());
    explicit QImageReader(const QString &fileName, const QByteArray &format = QByteArray());
    ~QImageReader();
    QImageReader(const QImageReader &) = delete;
    QImageReader &operator=(const QImageReader &) = delete;

    void setFormat(const QByteArray &format);
    QByteArray format() const;
    void setDevice(QIODevice *device);
    QIODevice *device() const;
    void setFileName(const QString &fileName);
    QString fileName() const;

    QSize size() const;
    QImage::Format imageFormat() const;
    void setScaledSize(const QSize &size);
    QSize scaledSize() const;
    void setAutoTransform(bool enabled);
    bool autoTransform() const;

    bool canRead() const;
    QImage read();
    bool read(QImage *image);

    ImageReaderError error() const;
    QString errorString() const;

    static QByteArray imageFormat(const QString &fileName);
    static QByteArray imageFormat(QIODevice *device);
    static QList<QByteArray> supportedImageFormats();

private:
    struct Data;
    std::unique_ptr<Data> d;
};
