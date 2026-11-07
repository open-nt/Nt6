// MIT License - see LICENSE
#pragma once

#include <QtCore/qiodevice.h>

class QFileDevice : public QIODevice {
    Q_OBJECT
public:
    /*!
        \enum QFileDevice::FileError

        Identifies the kind of error that last occurred on a file device.

        \value NoError No error occurred.
        \value ReadError An error occurred when reading from the file.
        \value WriteError An error occurred when writing to the file.
        \value FatalError A fatal error occurred.
        \value ResourceError Out of resources (for example, too many open files).
        \value OpenError The file could not be opened.
        \value AbortError The operation was aborted.
        \value TimeOutError A timeout occurred.
        \value UnspecifiedError An unspecified error occurred.
        \value RemoveError The file could not be removed.
        \value RenameError The file could not be renamed.
        \value PositionError The position in the file could not be changed.
        \value ResizeError The file could not be resized.
        \value PermissionsError The file could not be accessed.
        \value CopyError The file could not be copied.
    */
    enum FileError {
        NoError, ReadError, WriteError, FatalError, ResourceError, OpenError, AbortError, TimeOutError,
        UnspecifiedError, RemoveError, RenameError, PositionError, ResizeError, PermissionsError, CopyError
    };

    ~QFileDevice() override;

    /*!
        Returns the file error status. The returned error is \c NoError if no
        error has occurred since the file was opened or unsetError() was
        called.

        \sa unsetError(), errorString()
    */
    FileError error() const { return m_error; }
    /*!
        Sets the file's error to \c NoError. The error string is not cleared.

        \sa error()
    */
    void unsetError() { m_error = NoError; }
    /*!
        Returns the name set by QFile::setFileName() or to the QFile
        constructors.

        \sa QFile::setFileName()
    */
    QString fileName() const { return m_name; }
    /*!
        Returns the native file descriptor of the open file, or \c -1 if the
        file is not open.
    */
    int handle() const { return m_fd; }
    bool flush();

    void close() override;
    qint64 pos() const override;
    qint64 size() const override;
    bool seek(qint64 offset) override;
    bool atEnd() const override;
    qint64 bytesAvailable() const override;
    bool canReadLine() const override;
    bool resize(qint64 sz);

protected:
    QFileDevice();
    explicit QFileDevice(QObject *parent);
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;
    qint64 readLineData(char *data, qint64 maxSize) override;
    bool openPath(const QString &path, OpenMode mode);
    void setFileError(FileError e, const QString &text);

    /*! \internal */
    QString m_name;
    /*! \internal */
    int m_fd = -1;
    /*! \internal */
    FileError m_error = NoError;
};

class QFile : public QFileDevice {
    Q_OBJECT
public:
    QFile();
    explicit QFile(const QString &name);
    explicit QFile(QObject *parent);
    QFile(const QString &name, QObject *parent);
    ~QFile() override;

    /*!
        Sets the \a name of the file. The name has no path, a relative path,
        or an absolute path. Does nothing if the file is open.

        \sa fileName(), exists()
    */
    void setFileName(const QString &name) { if (!isOpen()) m_name = name; }
    bool open(OpenMode mode) override;
    /*!
        \overload

        Returns \c true if the file specified by fileName() exists; otherwise
        returns \c false.

        \sa fileName(), setFileName()
    */
    bool exists() const { return exists(m_name); }
    bool remove();
    bool rename(const QString &newName);
    bool copy(const QString &newName);

    static bool exists(const QString &fileName);
    static bool remove(const QString &fileName);
    static bool rename(const QString &oldName, const QString &newName);
    static bool copy(const QString &fileName, const QString &newName);
};
