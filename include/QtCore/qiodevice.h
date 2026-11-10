// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qflags.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <utility>

/*!
    \class QIODeviceBase
    \brief The QIODeviceBase class provides the open mode flags shared by I/O devices.

    QIODeviceBase is the base class of QIODevice. It holds the OpenModeFlag
    enum and the OpenMode flags type, so that they can be named through any
    device class, for example as \c QFile::ReadOnly.

    \differences
    QIODeviceBase has no other members.
    \enddifferences

    \sa QIODevice
*/
class QIODeviceBase {
public:
    /*!
        \enum QIODeviceBase::OpenModeFlag

        Describes how a device is opened. The OpenMode type is a typedef for
        \c{QFlags<OpenModeFlag>}.

        \value NotOpen The device is not open.
        \value ReadOnly The device is open for reading.
        \value WriteOnly The device is open for writing. Note that this mode
               implies \c Truncate for files unless \c Append or \c ReadOnly
               is also given.
        \value ReadWrite The device is open for reading and writing.
        \value Append The device is opened in append mode, so that all data is
               written to the end of the file. Implies \c WriteOnly for files.
        \value Truncate If possible, the device is truncated before it is
               opened. All earlier contents are lost. Implies \c WriteOnly for
               files.
        \value Text Marks the device as a text device. QFile does not
               translate line endings.
        \value Unbuffered Accepted for compatibility; devices in Nt do not
               change their behavior.
        \value NewOnly Fail if the file to be opened already exists. Create
               and open the file only if it does not exist.
        \value ExistingOnly Fail if the file to be opened does not exist.
    */
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

class QProcess;

class QIODevice : public QObject, public QIODeviceBase {
    Q_OBJECT
public:
    QIODevice();
    explicit QIODevice(QObject *parent);
    ~QIODevice() override;

    /*!
        Returns the mode in which the device has been opened, or \c NotOpen.

        \sa open(), setOpenMode()
    */
    OpenMode openMode() const { return m_mode; }
    void setTextModeEnabled(bool enabled);
    /*!
        Returns \c true if the \c Text flag is set on the open mode; otherwise
        returns \c false.

        \sa setTextModeEnabled()
    */
    bool isTextModeEnabled() const { return m_mode.testFlag(Text); }
    /*!
        Returns \c true if the device is open; otherwise returns \c false. A
        device is open if its open mode is not \c NotOpen.

        \sa openMode(), open(), close()
    */
    bool isOpen() const { return m_mode != NotOpen; }
    /*!
        Returns \c true if data can be read from the device; otherwise
        returns \c false.

        \sa isWritable(), openMode()
    */
    bool isReadable() const { return m_mode.testFlag(ReadOnly); }
    /*!
        Returns \c true if data can be written to the device; otherwise
        returns \c false.

        \sa isReadable(), openMode()
    */
    bool isWritable() const { return m_mode.testFlag(WriteOnly); }
    /*!
        Returns \c true if the device is a sequential device, which has no
        notion of position and cannot seek; otherwise returns \c false. The
        base implementation returns \c false.

        \sa seek(), pos()
    */
    virtual bool isSequential() const { return false; }

    virtual bool open(OpenMode mode);
    virtual void close();
    /*!
        Returns the current read/write position of the device. The base
        implementation returns \c 0.

        \sa seek(), size()
    */
    virtual qint64 pos() const { return 0; }
    /*!
        Returns the size of the device. For a sequential device this is
        bytesAvailable(); otherwise the base implementation returns \c 0.

        \sa pos(), bytesAvailable()
    */
    virtual qint64 size() const { return isSequential() ? bytesAvailable() : 0; }
    virtual bool seek(qint64 pos);
    virtual bool atEnd() const;
    /*!
        Seeks to the start of the device. Returns \c true on success;
        otherwise returns \c false.

        \sa seek()
    */
    virtual bool reset() { return seek(0); }
    virtual qint64 bytesAvailable() const;
    /*!
        For buffered devices, returns the number of bytes waiting to be
        written. The base implementation returns \c 0.

        \sa bytesAvailable()
    */
    virtual qint64 bytesToWrite() const { return 0; }
    virtual bool canReadLine() const;
    /*!
        Waits until data is available for reading and readyRead() has been
        emitted, or until \a msecs milliseconds have passed. The base
        implementation does nothing and returns \c false.

        \sa waitForBytesWritten()
    */
    virtual bool waitForReadyRead(int msecs) { Q_UNUSED(msecs) return false; }
    /*!
        Waits until buffered data has been written to the device and
        bytesWritten() has been emitted, or until \a msecs milliseconds have
        passed. The base implementation does nothing and returns \c false.

        \sa waitForReadyRead()
    */
    virtual bool waitForBytesWritten(int msecs) { Q_UNUSED(msecs) return false; }

    qint64 read(char *data, qint64 maxSize);
    QByteArray read(qint64 maxSize);
    QByteArray readAll();
    qint64 readLine(char *data, qint64 maxSize);
    QByteArray readLine(qint64 maxSize = 0);
    bool readLineInto(QByteArray *line, qint64 maxSize = 0);
    qint64 skip(qint64 maxSize);

    void startTransaction();
    void commitTransaction();
    void rollbackTransaction();
    /*!
        \since 5.7
        Returns \c true if a transaction is in progress on the device;
        otherwise returns \c false.

        \sa startTransaction()
    */
    bool isTransactionStarted() const { return m_transaction; }

    int readChannelCount() const;
    int writeChannelCount() const;
    /*!
        \since 5.7
        Returns the index of the current read channel, the one that read(),
        readLine(), peek() and bytesAvailable() work on.

        \sa setCurrentReadChannel(), readChannelCount()
    */
    int currentReadChannel() const { return m_readChannel; }
    void setCurrentReadChannel(int channel);
    /*!
        \since 5.7
        Returns the index of the current write channel, the one that write()
        sends to.

        \sa setCurrentWriteChannel(), writeChannelCount()
    */
    int currentWriteChannel() const { return m_writeChannel; }
    void setCurrentWriteChannel(int channel);
    qint64 write(const char *data, qint64 maxSize);
    /*!
        \overload

        Writes the zero-terminated string \a data to the device, not
        including the terminating null byte, and returns the number of bytes
        written.
    */
    qint64 write(const char *data) { return write(data, static_cast<qint64>(std::char_traits<char>::length(data))); }
    /*!
        \overload

        Writes the contents of \a data to the device and returns the number
        of bytes written.
    */
    qint64 write(const QByteArray &data) { return write(data.constData(), data.size()); }
    QByteArray peek(qint64 maxSize);
    qint64 peek(char *data, qint64 maxSize);
    void ungetChar(char c);
    bool getChar(char *c);
    /*!
        Writes the character \a c to the device. Returns \c true on success;
        otherwise returns \c false.

        \sa getChar(), write()
    */
    bool putChar(char c) { return write(&c, 1) == 1; }

    QString errorString() const;

signals:
    /*!
        This signal is emitted when the device is about to close, before
        close() releases any resources.

        \sa close()
    */
    void aboutToClose() { emitSignal(&QIODevice::aboutToClose); }
    /*!
        This signal is emitted every time a payload of data has been written
        to the device. The \a bytes argument is set to the number of bytes
        that were written. It is only emitted by subclasses that buffer
        writes.

        \sa readyRead()
    */
    void bytesWritten(qint64 bytes) { emitSignal(&QIODevice::bytesWritten, bytes); }
    /*!
        \since 5.7
        This signal is emitted every time a payload of data has been written
        to the device. The \a bytes argument is the number of bytes that were
        written, and \a channel is the index of the channel they went to.
        Unlike bytesWritten(), it is emitted whatever the current write
        channel is.

        \sa bytesWritten(), channelReadyRead()
    */
    void channelBytesWritten(int channel, qint64 bytes) { emitSignal(&QIODevice::channelBytesWritten, channel, bytes); }
    /*!
        \since 5.7
        This signal is emitted when new data is available for reading on the
        channel with the index \a channel. Unlike readyRead(), it is emitted
        whatever the current read channel is.

        \sa readyRead(), channelBytesWritten()
    */
    void channelReadyRead(int channel) { emitSignal(&QIODevice::channelReadyRead, channel); }
    /*!
        This signal is emitted when the input (reading) stream is closed in
        this device. It is only emitted by subclasses that support it.

        \sa atEnd()
    */
    void readChannelFinished() { emitSignal(&QIODevice::readChannelFinished); }
    /*!
        This signal is emitted once every time new data is available for
        reading from the device. It is only emitted by subclasses that
        receive data asynchronously.

        \sa bytesWritten(), bytesAvailable()
    */
    void readyRead() { emitSignal(&QIODevice::readyRead); }

protected:
    /*!
        Reads up to \a maxSize bytes from the device into \a data, and
        returns the number of bytes read, or \c -1 if an error occurred.
        Subclasses must implement this function.

        \sa read(), writeData()
    */
    virtual qint64 readData(char *data, qint64 maxSize) = 0;
    /*!
        Writes up to \a maxSize bytes from \a data to the device. Returns the
        number of bytes written, or \c -1 if an error occurred. Subclasses
        must implement this function.

        \sa write(), readData()
    */
    virtual qint64 writeData(const char *data, qint64 maxSize) = 0;
    virtual qint64 readLineData(char *data, qint64 maxSize);
    virtual qint64 skipData(qint64 maxSize);
    /*!
        Sets the open mode of the device to \a mode without any other effect.
        Subclasses call this from their open() reimplementations.

        \sa openMode(), open()
    */
    void setOpenMode(OpenMode mode) { m_mode = mode; }
    /*!
        Sets the human-readable description of the last device error to
        \a str.

        \sa errorString()
    */
    void setErrorString(const QString &str) { m_error = str; }

private:
    friend class QProcess; // has two read channels
    qint64 bufferedBytes() const { return m_buf.size() - m_bufPos; }
    void compactBuffer();
    void keepForTransaction(const char *data, qint64 n);

    OpenMode m_mode = NotOpen;
    QString m_error;
    QByteArray m_buf;                           // look-ahead data for sequential devices, of the current read channel
    QList<std::pair<int, QByteArray>> m_parked; // ...and of the other channels that have some
    int m_readChannels = 1;                     // while open for reading
    int m_writeChannels = 1;                    // while open for writing
    int m_readChannel = 0;
    int m_writeChannel = 0;
    qint64 m_bufPos = 0;
    qint64 m_transactionPos = 0; // buffer index (sequential) or device position (random access)
    bool m_transaction = false;
};
