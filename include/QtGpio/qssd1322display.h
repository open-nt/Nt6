// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qobject.h>
#include <QtCore/qrect.h>
#include <QtCore/qstring.h>

#include <memory>

class QSsd1322DisplayPrivate;

class QSsd1322Display : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString spiDevice READ spiDevice WRITE setSpiDevice)
    Q_PROPERTY(quint32 speed READ speed WRITE setSpeed)
    Q_PROPERTY(QString dataCommandLine READ dataCommandLine WRITE setDataCommandLine)
    Q_PROPERTY(QString resetLine READ resetLine WRITE setResetLine)
    Q_PROPERTY(int width READ width)
    Q_PROPERTY(int height READ height)
    Q_PROPERTY(int columnOffset READ columnOffset WRITE setColumnOffset)
    Q_PROPERTY(bool rotated READ isRotated WRITE setRotated)
    Q_PROPERTY(int contrast READ contrast WRITE setContrast)
    Q_PROPERTY(DisplayError error READ error RESET clearError NOTIFY errorOccurred)
public:
    enum DisplayError { NoError, DeviceError, NotOpenError, UnsupportedOperationError };
    Q_ENUM(DisplayError)

    explicit QSsd1322Display(QObject *parent = nullptr);
    ~QSsd1322Display() override;

    void setSpiDevice(const QString &name);
    QString spiDevice() const;
    void setSpeed(quint32 hertz);
    quint32 speed() const;
    void setDataCommandLine(const QString &line);
    QString dataCommandLine() const;
    void setResetLine(const QString &line);
    QString resetLine() const;
    void setSize(int width, int height);
    int width() const;
    int height() const;
    void setColumnOffset(int offset);
    int columnOffset() const;
    void setRotated(bool rotated);
    bool isRotated() const;

    bool open();
    void close();
    bool isOpen() const;

    bool setContrast(int contrast);
    int contrast() const;
    bool setDisplayEnabled(bool enabled);
    bool setInverted(bool inverted);
    bool clear();
    bool writePixels(const uchar *pixels, qsizetype bytesPerLine, const QRect &area = QRect());
    bool writeCommand(quint8 command, const QByteArray &arguments = QByteArray());

    DisplayError error() const;
    QString errorString() const;
    void clearError();

signals:
    /*!
        This signal is emitted when the error \a displayError occurs. It is not emitted when the
        error is cleared.

        \sa error(), errorString()
    */
    void errorOccurred(QSsd1322Display::DisplayError displayError) {
        emitSignal(&QSsd1322Display::errorOccurred, displayError);
    }

private:
    friend class QSsd1322DisplayPrivate;
    std::unique_ptr<QSsd1322DisplayPrivate> d;
};
