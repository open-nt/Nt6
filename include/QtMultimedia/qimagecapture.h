// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtGui/qimage.h>
#include <QtMultimedia/qvideoframe.h>

#include <memory>

class QMediaCaptureSession;
class QImageCapturePrivate;

class QImageCapture : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool readyForCapture READ isReadyForCapture NOTIFY readyForCaptureChanged)
    Q_PROPERTY(QImageCapture::Error error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(FileFormat fileFormat READ fileFormat WRITE setFileFormat NOTIFY fileFormatChanged)
    Q_PROPERTY(Quality quality READ quality WRITE setQuality NOTIFY qualityChanged)
    Q_PROPERTY(QSize resolution READ resolution WRITE setResolution NOTIFY resolutionChanged)
public:
    enum Error { NoError, NotReadyError, ResourceError, OutOfSpaceError, NotSupportedFeatureError, FormatError };
    Q_ENUM(Error)

    enum Quality { VeryLowQuality, LowQuality, NormalQuality, HighQuality, VeryHighQuality };
    Q_ENUM(Quality)

    enum FileFormat { UnspecifiedFormat, JPEG, PNG, WebP, Tiff, LastFileFormat = Tiff };
    Q_ENUM(FileFormat)

    explicit QImageCapture(QObject *parent = nullptr);
    ~QImageCapture() override;

    bool isAvailable() const;
    QMediaCaptureSession *captureSession() const;

    Error error() const;
    QString errorString() const;

    bool isReadyForCapture() const;

    FileFormat fileFormat() const;
    void setFileFormat(FileFormat format);

    static QList<FileFormat> supportedFormats();
    static QString fileFormatName(FileFormat c);
    static QString fileFormatDescription(FileFormat c);

    QSize resolution() const;
    void setResolution(const QSize &resolution);
    void setResolution(int width, int height) { setResolution(QSize(width, height)); }

    Quality quality() const;
    void setQuality(Quality quality);

public slots:
    int captureToFile(const QString &location = QString());
    int capture();

signals:
    /*!
        This signal is emitted when error() or errorString() changes.
    */
    void errorChanged() { emitSignal(&QImageCapture::errorChanged); }
    /*!
        This signal is emitted when the capture with the given \a id fails with \a error, which
        \a errorString describes.
    */
    void errorOccurred(int id, QImageCapture::Error error, const QString &errorString) {
        emitSignal(&QImageCapture::errorOccurred, id, error, errorString);
    }
    /*!
        This signal is emitted when the capture becomes ready or stops being ready; \a ready is the
        new state.
    */
    void readyForCaptureChanged(bool ready) { emitSignal(&QImageCapture::readyForCaptureChanged, ready); }
    /*!
        This signal is emitted when the image of capture \a id has been exposed.
    */
    void imageExposed(int id) { emitSignal(&QImageCapture::imageExposed, id); }
    /*!
        This signal is emitted when capture \a id has taken \a preview.
    */
    void imageCaptured(int id, const QImage &preview) { emitSignal(&QImageCapture::imageCaptured, id, preview); }
    /*!
        This signal is emitted when the \a frame of capture \a id is available.
    */
    void imageAvailable(int id, const QVideoFrame &frame) { emitSignal(&QImageCapture::imageAvailable, id, frame); }
    /*!
        This signal is emitted when capture \a id has been saved to \a fileName.
    */
    void imageSaved(int id, const QString &fileName) { emitSignal(&QImageCapture::imageSaved, id, fileName); }
    /*!
        This signal is emitted when the file format changes.
    */
    void fileFormatChanged() { emitSignal(&QImageCapture::fileFormatChanged); }
    /*!
        This signal is emitted when the quality changes.
    */
    void qualityChanged() { emitSignal(&QImageCapture::qualityChanged); }
    /*!
        This signal is emitted when the resolution changes.
    */
    void resolutionChanged() { emitSignal(&QImageCapture::resolutionChanged); }

private:
    friend class QMediaCaptureSession;
    friend class QMediaCaptureSessionPrivate;
    friend class QImageCapturePrivate;
    std::unique_ptr<QImageCapturePrivate> d;
};
