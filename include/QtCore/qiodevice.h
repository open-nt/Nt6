// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qflags.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

class QIODeviceBase {
public:
    enum OpenModeFlag {
        NotOpen = 0x0000,
        ReadOnly = 0x0001,
        WriteOnly = 0x0002,
        ReadWrite = ReadOnly | WriteOnly,
        Append = 0x0004,
        Truncate = 0x0008,
        Text = 0x0010,
        Unbuffered = 0x0020,
        NewOnly = 0x0040,
        ExistingOnly = 0x0080
    };
    Q_DECLARE_FLAGS(OpenMode, OpenModeFlag)
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QIODeviceBase::OpenMode)

class QIODevice : public QObject, public QIODeviceBase {
    Q_OBJECT
public:
    QIODevice();
    explicit QIODevice(QObject *parent);
    ~QIODevice() override;

    OpenMode openMode() const { return m_mode; }
    void setTextModeEnabled(bool enabled);
    bool isTextModeEnabled() const { return m_mode.testFlag(Text); }
    bool isOpen() const { return m_mode != NotOpen; }
    bool isReadable() const { return m_mode.testFlag(ReadOnly); }
    bool isWritable() const { return m_mode.testFlag(WriteOnly); }
    virtual bool isSequential() const { return false; }

    virtual bool open(OpenMode mode);
    virtual void close();
    virtual qint64 pos() const { return 0; }
    virtual qint64 size() const { return isSequential() ? bytesAvailable() : 0; }
    virtual bool seek(qint64 pos);
    virtual bool atEnd() const;
    virtual bool reset() { return seek(0); }
    virtual qint64 bytesAvailable() const;
    virtual qint64 bytesToWrite() const { return 0; }
    virtual bool canReadLine() const;
    virtual bool waitForReadyRead(int msecs) { Q_UNUSED(msecs) return false; }
    virtual bool waitForBytesWritten(int msecs) { Q_UNUSED(msecs) return false; }

    qint64 read(char *data, qint64 maxSize);
    QByteArray read(qint64 maxSize);
    QByteArray readAll();
    qint64 readLine(char *data, qint64 maxSize);
    QByteArray readLine(qint64 maxSize = 0);
    qint64 write(const char *data, qint64 maxSize);
    qint64 write(const char *data) { return write(data, static_cast<qint64>(std::char_traits<char>::length(data))); }
    qint64 write(const QByteArray &data) { return write(data.constData(), data.size()); }
    QByteArray peek(qint64 maxSize);
    qint64 peek(char *data, qint64 maxSize);
    void ungetChar(char c);
    bool getChar(char *c);
    bool putChar(char c) { return write(&c, 1) == 1; }

    QString errorString() const;

signals:
    void aboutToClose() { emitSignal(&QIODevice::aboutToClose); }
    void bytesWritten(qint64 bytes) { emitSignal(&QIODevice::bytesWritten, bytes); }
    void readChannelFinished() { emitSignal(&QIODevice::readChannelFinished); }
    void readyRead() { emitSignal(&QIODevice::readyRead); }

protected:
    virtual qint64 readData(char *data, qint64 maxSize) = 0;
    virtual qint64 writeData(const char *data, qint64 maxSize) = 0;
    virtual qint64 readLineData(char *data, qint64 maxSize);
    void setOpenMode(OpenMode mode) { m_mode = mode; }
    void setErrorString(const QString &str) { m_error = str; }

private:
    qint64 bufferedBytes() const { return m_buf.size() - m_bufPos; }
    void compactBuffer();

    OpenMode m_mode = NotOpen;
    QString m_error;
    QByteArray m_buf; // look-ahead data for sequential devices
    qint64 m_bufPos = 0;
};
