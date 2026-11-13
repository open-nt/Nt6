// MIT License - see LICENSE
#pragma once

#include <utility>

#include <QtCore/qdatetime.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qflags.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qtimezone.h>

#include <cstddef>
#include <iterator>
#include <memory>

class QDirListing {
public:
    enum class IteratorFlag {
        Default = 0x000000,
        ExcludeFiles = 0x000004,
        ExcludeDirs = 0x000008,
        ExcludeSpecial = 0x000010,
        ExcludeOther = ExcludeSpecial,
        ResolveSymlinks = 0x000020,
        FilesOnly = ExcludeDirs | ExcludeSpecial,
        DirsOnly = ExcludeFiles | ExcludeSpecial,
        IncludeHidden = 0x000040,
        IncludeDotAndDotDot = 0x000080,
        CaseSensitive = 0x000100,
        Recursive = 0x000400,
        FollowDirSymlinks = 0x000800,
    };
    Q_DECLARE_FLAGS(IteratorFlags, IteratorFlag)

    explicit QDirListing(const QString &path, IteratorFlags flags = IteratorFlag::Default);
    explicit QDirListing(const QString &path, const QStringList &nameFilters,
                         IteratorFlags flags = IteratorFlag::Default);
    QDirListing(QDirListing &&other) noexcept;
    QDirListing &operator=(QDirListing &&other) noexcept;
    ~QDirListing();
    /*!
        \since 6.8
        Exchanges this listing with \a other.
    */
    void swap(QDirListing &other) noexcept { d.swap(other.d); }
    QDirListing(const QDirListing &) = delete;
    QDirListing &operator=(const QDirListing &) = delete;

    QString iteratorPath() const;
    IteratorFlags iteratorFlags() const;
    QStringList nameFilters() const;

    class DirEntry {
    public:
        /*!
            Returns the name of the entry, without the path.
        */
        QString fileName() const { return m_info.fileName(); }
        /*!
            Returns the name of the entry up to its first dot.
        */
        QString baseName() const { return m_info.baseName(); }
        /*!
            Returns the name of the entry up to its last dot.
        */
        QString completeBaseName() const { return m_info.completeBaseName(); }
        /*!
            Returns what follows the last dot of the name.
        */
        QString suffix() const { return m_info.suffix(); }
        /*!
            Returns the name of the bundle, which only macOS has: an empty string.
        */
        QString bundleName() const { return QString(); }
        /*!
            Returns what follows the first dot of the name.
        */
        QString completeSuffix() const { return m_info.completeSuffix(); }
        /*!
            Returns the path of the entry, which starts with the path given to the QDirListing.
        */
        QString filePath() const { return m_info.filePath(); }
        /*!
            Returns the absolute path of the entry with symbolic links resolved, or an empty string if it
            does not exist.
        */
        QString canonicalFilePath() const { return m_info.canonicalFilePath(); }
        /*!
            Returns the absolute path of the entry.
        */
        QString absoluteFilePath() const { return m_info.absoluteFilePath(); }
        /*!
            Returns the absolute path of the directory that holds the entry.
        */
        QString absolutePath() const { return m_info.absolutePath(); }
        /*!
            Returns \c true if the entry is a directory, or a symbolic link to one.
        */
        bool isDir() const { return m_info.isDir(); }
        /*!
            Returns \c true if the entry is a regular file, or a symbolic link to one.
        */
        bool isFile() const { return m_info.isFile(); }
        /*!
            Returns \c true if the entry is a symbolic link.
        */
        bool isSymLink() const { return m_info.isSymLink(); }
        /*!
            Returns \c true if the entry exists. A symbolic link whose target is missing does not.
        */
        bool exists() const { return m_info.exists(); }
        /*!
            Returns \c true if the entry is hidden: its name starts with a dot.
        */
        bool isHidden() const { return m_info.isHidden(); }
        /*!
            Returns \c true if the user can read the entry.
        */
        bool isReadable() const { return m_info.isReadable(); }
        /*!
            Returns \c true if the user can write to the entry.
        */
        bool isWritable() const { return m_info.isWritable(); }
        /*!
            Returns \c true if the user can execute the entry.
        */
        bool isExecutable() const { return m_info.isExecutable(); }
        /*!
            Returns a QFileInfo for the entry.
        */
        QFileInfo fileInfo() const { return m_info; }
        /*!
            Returns the size of the entry in bytes.
        */
        qint64 size() const { return m_info.size(); }
        /*!
            Returns the time stamp \a type of the entry, in the time zone \a tz.
        */
        QDateTime fileTime(QFile::FileTime type, const QTimeZone &tz) const {
            return m_info.fileTime(type).toTimeZone(tz);
        }
        /*!
            Returns when the entry was created, in the time zone \a tz, or an invalid date where the
            file system does not record it.
        */
        QDateTime birthTime(const QTimeZone &tz) const { return fileTime(QFile::FileBirthTime, tz); }
        /*!
            Returns when the metadata of the entry last changed, in the time zone \a tz.
        */
        QDateTime metadataChangeTime(const QTimeZone &tz) const { return fileTime(QFile::FileMetadataChangeTime, tz); }
        /*!
            Returns when the contents of the entry last changed, in the time zone \a tz.
        */
        QDateTime lastModified(const QTimeZone &tz) const { return fileTime(QFile::FileModificationTime, tz); }
        /*!
            Returns when the entry was last read, in the time zone \a tz.
        */
        QDateTime lastRead(const QTimeZone &tz) const { return fileTime(QFile::FileAccessTime, tz); }

    private:
        friend class QDirListing;
        QFileInfo m_info;
    };

    class sentinel {};

    class const_iterator {
    public:
        /*! The category of the iterator: it reads forward, once. */
        using iterator_category = std::input_iterator_tag;
        /*! The type the iterator points to. */
        using value_type = DirEntry;
        /*! The type of a distance between iterators. */
        using difference_type = std::ptrdiff_t;
        /*! A pointer to an entry. */
        using pointer = const DirEntry *;
        /*! A reference to an entry. */
        using reference = const DirEntry &;

        /*!
            Constructs an iterator that is at the end.
        */
        const_iterator() = default;
        /*!
            Returns the entry the iterator is at. The reference is valid until the iterator moves.
        */
        reference operator*() const { return m_entry; }
        /*!
            Returns a pointer to the entry the iterator is at.
        */
        pointer operator->() const { return &m_entry; }
        const_iterator &operator++();
        /*!
            \overload
            Moves the iterator to the next entry. As the listing is read once, there is no copy of
            the iterator at the old position to return.
        */
        void operator++(int) { ++*this; }

        /*!
            Returns \c true if the iterator \a it has passed the last entry.
        */
        friend bool operator==(const const_iterator &it, sentinel) noexcept { return it.m_listing == nullptr; }
        /*!
            Returns \c true if the iterator \a it is at an entry.
        */
        friend bool operator!=(const const_iterator &it, sentinel) noexcept { return it.m_listing != nullptr; }
        /*!
            \overload
            Returns \c true if the iterator \a it has passed the last entry.
        */
        friend bool operator==(sentinel, const const_iterator &it) noexcept { return it.m_listing == nullptr; }
        /*!
            \overload
            Returns \c true if the iterator \a it is at an entry.
        */
        friend bool operator!=(sentinel, const const_iterator &it) noexcept { return it.m_listing != nullptr; }

    private:
        friend class QDirListing;
        const QDirListing *m_listing = nullptr;
        DirEntry m_entry;
    };

    const_iterator begin() const;
    /*!
        Same as begin().
    */
    const_iterator cbegin() const { return begin(); }
    /*!
        Same as begin().
    */
    const_iterator constBegin() const { return begin(); }
    /*!
        Returns the value that an iterator compares equal to when it has passed the last entry.

        \sa begin()
    */
    sentinel end() const { return {}; }
    /*!
        Same as end().
    */
    sentinel cend() const { return {}; }
    /*!
        Same as end().
    */
    sentinel constEnd() const { return {}; }

private:
    struct Private;
    std::unique_ptr<Private> d;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QDirListing::IteratorFlags)
