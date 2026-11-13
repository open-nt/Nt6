// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtMultimedia/qcameradevice.h>

#include <memory>

class QMediaCaptureSession;
class QCameraPrivate;

class QCamera : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QCameraDevice cameraDevice READ cameraDevice WRITE setCameraDevice NOTIFY cameraDeviceChanged)
    Q_PROPERTY(QCamera::Error error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(QCameraFormat cameraFormat READ cameraFormat WRITE setCameraFormat NOTIFY cameraFormatChanged)
public:
    enum Error { NoError, CameraError };
    Q_ENUM(Error)

    explicit QCamera(QObject *parent = nullptr);
    explicit QCamera(const QCameraDevice &cameraDevice, QObject *parent = nullptr);
    explicit QCamera(QCameraDevice::Position position, QObject *parent = nullptr);
    ~QCamera() override;

    bool isAvailable() const;
    bool isActive() const;

    QMediaCaptureSession *captureSession() const;

    QCameraDevice cameraDevice() const;
    void setCameraDevice(const QCameraDevice &cameraDevice);

    QCameraFormat cameraFormat() const;
    void setCameraFormat(const QCameraFormat &format);

    Error error() const;
    QString errorString() const;

public slots:
    void setActive(bool active);
    void start() { setActive(true); }
    void stop() { setActive(false); }

signals:
    /*!
        This signal is emitted when the camera starts or stops; \a active is the new state.
    */
    void activeChanged(bool active) { emitSignal(&QCamera::activeChanged, active); }
    /*!
        This signal is emitted when error() or errorString() changes.
    */
    void errorChanged() { emitSignal(&QCamera::errorChanged); }
    /*!
        This signal is emitted when an \a error occurs, with \a errorString describing it.
    */
    void errorOccurred(QCamera::Error error, const QString &errorString) {
        emitSignal(&QCamera::errorOccurred, error, errorString);
    }
    /*!
        This signal is emitted when the camera device changes.
    */
    void cameraDeviceChanged() { emitSignal(&QCamera::cameraDeviceChanged); }
    /*!
        This signal is emitted when the camera format changes.
    */
    void cameraFormatChanged() { emitSignal(&QCamera::cameraFormatChanged); }

private:
    friend class QMediaCaptureSession;
    friend class QMediaCaptureSessionPrivate;
    friend class QCameraPrivate;
    std::unique_ptr<QCameraPrivate> d;
};
