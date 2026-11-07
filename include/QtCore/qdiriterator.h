// MIT License - see LICENSE
#pragma once

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qflags.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

#include <memory>

class QDirIterator {
public:
    /*!
        \enum QDirIterator::IteratorFlag

        This enum describes flags that you can combine to configure the
        behavior of QDirIterator.

        \value NoIteratorFlags The default value, representing no flags. The iterator will return entries for the assigned path.
        \value Subdirectories List entries inside all subdirectories as well.
        \value FollowSymlinks When combined with Subdirectories, this flag enables iterating through all subdirectories of the assigned path, following all symbolic links. Symbolic link loops (e.g., "link" => "." or "link" => "..") are automatically detected and ignored.
    */
    enum IteratorFlag { NoIteratorFlags = 0x0, FollowSymlinks = 0x1, Subdirectories = 0x2 };
    Q_DECLARE_FLAGS(IteratorFlags, IteratorFlag)

    explicit QDirIterator(const QDir &dir, IteratorFlags flags = NoIteratorFlags);
    explicit QDirIterator(const QString &path, IteratorFlags flags = NoIteratorFlags);
    QDirIterator(const QString &path, QDir::Filters filters, IteratorFlags flags = NoIteratorFlags);
    QDirIterator(const QString &path, const QStringList &nameFilters, QDir::Filters filters = QDir::NoFilter,
                 IteratorFlags flags = NoIteratorFlags);
    ~QDirIterator();
    QDirIterator(const QDirIterator &) = delete;
    QDirIterator &operator=(const QDirIterator &) = delete;

    QString next();
    QFileInfo nextFileInfo();
    bool hasNext() const;
    QString fileName() const;
    QString filePath() const;
    QFileInfo fileInfo() const;
    QString path() const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QDirIterator::IteratorFlags)
