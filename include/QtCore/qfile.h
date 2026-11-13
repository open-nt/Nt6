// MIT License - see LICENSE
#pragma once

#include <QtCore/qdatetime.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qiodevice.h>

#include <cstdio>
#include <map>
#include <memory>
#include <utility>

class QFileDevice : public QIODevice {
    Q_OBJECT
public:
    /*!
        \typedef QFileDevice::Permission

        The permission bits of a file, the same type as QFileInfo::Permission,
        so that values can be passed between the two classes.
    */
    using Permission = QFileInfo::Permission;
    /*!
        \typedef QFileDevice::Permissions

        A QFlags combination of Permission values, the same type as
        QFileInfo::Permissions.
    */
    using Permissions = QFileInfo::Permissions;
    /*! The file is readable by its owner. */
    static constexpr Permission ReadOwner = QFileInfo::ReadOwner;
    /*! The file is writable by its owner. */
    static constexpr Permission WriteOwner = QFileInfo::WriteOwner;
    /*! The file is executable by its owner. */
    static constexpr Permission ExeOwner = QFileInfo::ExeOwner;
    /*! The file is readable by the user. On POSIX systems this is the owner. */
    static constexpr Permission ReadUser = QFileInfo::ReadUser;
    /*! The file is writable by the user. On POSIX systems this is the owner. */
    static constexpr Permission WriteUser = QFileInfo::WriteUser;
    /*! The file is executable by the user. On POSIX systems this is the owner. */
    static constexpr Permission ExeUser = QFileInfo::ExeUser;
    /*! The file is readable by its group. */
    static constexpr Permission ReadGroup = QFileInfo::ReadGroup;
    /*! The file is writable by its group. */
    static constexpr Permission WriteGroup = QFileInfo::WriteGroup;
    /*! The file is executable by its group. */
    static constexpr Permission ExeGroup = QFileInfo::ExeGroup;
    /*! The file is readable by anyone. */
    static constexpr Permission ReadOther = QFileInfo::ReadOther;
    /*! The file is writable by anyone. */
    static constexpr Permission WriteOther = QFileInfo::WriteOther;
    /*! The file is executable by anyone. */
    static constexpr Permission ExeOther = QFileInfo::ExeOther;

    /*!
        \enum QFileDevice::FileTime
        \since 5.10

        Selects one of the time stamps of a file.

        \value FileAccessTime When the file was last read.
        \value FileBirthTime When the file was created.
        \value FileMetadataChangeTime When the metadata of the file, such as
               its permissions, last changed.
        \value FileModificationTime When the contents of the file last changed.

        \sa fileTime(), setFileTime()
    */
    enum FileTime { FileAccessTime, FileBirthTime, FileMetadataChangeTime, FileModificationTime };

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
    /*!
        \enum QFileDevice::FileHandleFlag

        How QFile::open() treats a file handle or descriptor it is given.

        \value AutoCloseHandle close() closes the handle too.
        \value DontCloseHandle The handle stays open after close(); this is the default.
    */
    enum FileHandleFlag { AutoCloseHandle = 0x0001, DontCloseHandle = 0 };
    /*!
        \typedef QFileDevice::FileHandleFlags

        A QFlags of FileHandleFlag values.
    */
    Q_DECLARE_FLAGS(FileHandleFlags, FileHandleFlag)
    /*!
        \enum QFileDevice::MemoryMapFlag
        \since 4.4

        Options of map().

        \value NoOptions Changes to the mapped memory are written to the file.
        \value MapPrivateOption Changes to the mapped memory stay private to the mapping and are not written to the file (copy-on-write).
    */
    enum MemoryMapFlag { NoOptions = 0, MapPrivateOption = 0x0001 };
    /*!
        \typedef QFileDevice::MemoryMapFlags

        A QFlags of MemoryMapFlag values.
    */
    Q_DECLARE_FLAGS(MemoryMapFlags, MemoryMapFlag)

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
    virtual Permissions permissions() const;
    virtual bool setPermissions(Permissions permissions);
    QDateTime fileTime(FileTime time) const;
    bool setFileTime(const QDateTime &newDate, FileTime fileTime);
    uchar *map(qint64 offset, qint64 size, MemoryMapFlags flags = NoOptions);
    bool unmap(uchar *address);

protected:
    QFileDevice();
    explicit QFileDevice(QObject *parent);
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;
    qint64 readLineData(char *data, qint64 maxSize) override;
    bool openPath(const QString &path, OpenMode mode);
    bool openPath(const QString &path, OpenMode mode, Permissions createPermissions);
    bool openHandle(int fd, std::FILE *fh, OpenMode mode, FileHandleFlags handleFlags);
    void setFileError(FileError e, const QString &text);

    /*! \internal */
    QString m_name;
    /*! \internal */
    int m_fd = -1;
    /*! \internal */
    FileError m_error = NoError;
    /*! \internal */
    struct ResourceData;
    /*! \internal */
    std::unique_ptr<ResourceData> m_resource;

private:
    bool m_closeHandle = true;                                // close() closes m_fd (or m_stdio)
    std::FILE *m_stdio = nullptr;                             // the stream given to QFile::open(FILE *)
    std::map<uchar *, std::pair<void *, std::size_t>> m_maps; // address returned by map() -> mmap() region
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QFileDevice::FileHandleFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QFileDevice::MemoryMapFlags)

class QFile : public QFileDevice {
    Q_OBJECT
public:
    QFile();
    explicit QFile(const QString &name);
    explicit QFile(QObject *parent);
    QFile(const QString &name, QObject *parent);
#ifdef NT_HAS_STD_FILESYSTEM
    /*!
        \since 6.0
        Constructs a QFile for the file \a name.
    */
    template <typename T, QtPrivate::ForFilesystemPath<T> = true>
    explicit QFile(const T &name) : QFile(QtPrivate::fromFilesystemPath(name)) {}
    /*!
        \since 6.0
        Constructs a QFile for the file \a name, with the given \a parent.
    */
    template <typename T, QtPrivate::ForFilesystemPath<T> = true>
    QFile(const T &name, QObject *parent) : QFile(QtPrivate::fromFilesystemPath(name), parent) {}
    /*!
        \since 6.0
        \overload
        Sets the name of the file to \a name. Does nothing if the file is open.
    */
    template <typename T, QtPrivate::ForFilesystemPath<T> = true> void setFileName(const T &name) {
        setFileName(QtPrivate::fromFilesystemPath(name));
    }
    /*!
        \since 6.0
        Returns fileName() as a \c std::filesystem::path.
    */
    std::filesystem::path filesystemFileName() const { return QtPrivate::toFilesystemPath(fileName()); }
    /*!
        \since 6.0
        Returns symLinkTarget() as a \c std::filesystem::path.
    */
    std::filesystem::path filesystemSymLinkTarget() const { return QtPrivate::toFilesystemPath(symLinkTarget()); }
    /*!
        \since 6.0
        Returns symLinkTarget(\a fileName) as a \c std::filesystem::path.
    */
    template <typename T, QtPrivate::ForFilesystemPath<T> = true>
    static std::filesystem::path filesystemSymLinkTarget(const T &fileName) {
        return QtPrivate::toFilesystemPath(symLinkTarget(QtPrivate::fromFilesystemPath(fileName)));
    }
    /*!
        \since 6.0
        \overload
        Returns \c true if the file \a fileName exists; otherwise returns \c false.
    */
    template <typename T, QtPrivate::ForFilesystemPath<T> = true> static bool exists(const T &fileName) {
        return exists(QtPrivate::fromFilesystemPath(fileName));
    }
    /*!
        \since 6.0
        \overload
        Renames the file to \a newName. Returns \c true on success.
    */
    template <typename T, QtPrivate::ForFilesystemPath<T> = true> bool rename(const T &newName) {
        return rename(QtPrivate::fromFilesystemPath(newName));
    }
    /*!
        \since 6.0
        \overload
        Copies the file to \a newName. Returns \c true on success.
    */
    template <typename T, QtPrivate::ForFilesystemPath<T> = true> bool copy(const T &newName) {
        return copy(QtPrivate::fromFilesystemPath(newName));
    }
    /*!
        \since 6.0
        \overload
        Makes a symbolic link \a newName to the file. Returns \c true on success.
    */
    template <typename T, QtPrivate::ForFilesystemPath<T> = true> bool link(const T &newName) {
        return link(QtPrivate::fromFilesystemPath(newName));
    }
#endif
    ~QFile() override;

    /*!
        Sets the \a name of the file. The name has no path, a relative path,
        or an absolute path. Does nothing if the file is open.

        \sa fileName(), exists()
    */
    void setFileName(const QString &name) { if (!isOpen()) m_name = name; }
    bool open(OpenMode mode) override;
    bool open(OpenMode mode, Permissions permissions);
    bool open(std::FILE *fh, OpenMode mode, FileHandleFlags handleFlags = DontCloseHandle);
    bool open(int fd, OpenMode mode, FileHandleFlags handleFlags = DontCloseHandle);
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
    bool moveToTrash();
    static bool moveToTrash(const QString &fileName, QString *pathInTrash = nullptr);
    QString readLink() const;
    static QString readLink(const QString &fileName);
    /*!
        Returns \a fileName converted to the 8-bit encoding of file names,
        which is UTF-8.

        \sa decodeName()
    */
    static QByteArray encodeName(const QString &fileName) { return fileName.toUtf8(); }
    /*!
        Returns the file name \a localFileName, given in the 8-bit encoding of
        file names (UTF-8), as a QString.

        \sa encodeName()
    */
    static QString decodeName(const QByteArray &localFileName) { return QString::fromUtf8(localFileName); }
    /*!
        \overload
        Returns the file name \a localFileName, given in UTF-8, as a QString.
    */
    static QString decodeName(const char *localFileName) { return QString::fromUtf8(localFileName); }

    static bool exists(const QString &fileName);
    static bool remove(const QString &fileName);
    static bool rename(const QString &oldName, const QString &newName);
    static bool copy(const QString &fileName, const QString &newName);

    Permissions permissions() const override;
    bool setPermissions(Permissions permissions) override;
    static Permissions permissions(const QString &fileName);
    static bool setPermissions(const QString &fileName, Permissions permissions);
    QString symLinkTarget() const;
    static QString symLinkTarget(const QString &fileName);
    bool link(const QString &linkName);
    static bool link(const QString &fileName, const QString &linkName);
};
