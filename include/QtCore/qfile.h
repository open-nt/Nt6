// MIT License - see LICENSE
#pragma once

#include <QtCore/qiodevice.h>

class QFileDevice : public QIODevice {
    Q_OBJECT
public:
    enum FileError {
        NoError, ReadError, WriteError, FatalError, ResourceError, OpenError, AbortError, TimeOutError,
        UnspecifiedError, RemoveError, RenameError, PositionError, ResizeError, PermissionsError, CopyError
    };

    ~QFileDevice() override;

    FileError error() const { return m_error; }
    void unsetError() { m_error = NoError; }
    QString fileName() const { return m_name; }
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

    QString m_name;
    int m_fd = -1;
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

    void setFileName(const QString &name) { if (!isOpen()) m_name = name; }
    bool open(OpenMode mode) override;
    bool exists() const { return exists(m_name); }
    bool remove();
    bool rename(const QString &newName);
    bool copy(const QString &newName);

    static bool exists(const QString &fileName);
    static bool remove(const QString &fileName);
    static bool rename(const QString &oldName, const QString &newName);
    static bool copy(const QString &fileName, const QString &newName);
};
