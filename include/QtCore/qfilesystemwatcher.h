// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

#include <memory>

class QFileSystemWatcher : public QObject {
    Q_OBJECT
public:
    explicit QFileSystemWatcher(QObject *parent = nullptr);
    explicit QFileSystemWatcher(const QStringList &paths, QObject *parent = nullptr);
    ~QFileSystemWatcher() override;

    bool addPath(const QString &file);
    QStringList addPaths(const QStringList &files);
    bool removePath(const QString &file);
    QStringList removePaths(const QStringList &files);
    QStringList files() const;
    QStringList directories() const;

signals:
    /*!
        This signal is emitted when the file at the specified \a path is
        modified, renamed or removed from disk.

        A removed or renamed file is no longer watched: it is dropped from
        files(), and has to be added again with addPath() if it is created
        again.

        \sa directoryChanged(), addPath()
    */
    void fileChanged(const QString &path) { emitSignal(&QFileSystemWatcher::fileChanged, path); }
    /*!
        This signal is emitted when the directory at a specified \a path is
        modified (e.g., when a file is added, deleted or renamed in it) or
        removed from disk. Note that if there are several changes during a
        short period of time, some of the changes might not emit this signal.
        However, the last change in the sequence of changes will always
        generate this signal.

        A removed directory is no longer watched.

        \sa fileChanged(), addPath()
    */
    void directoryChanged(const QString &path) { emitSignal(&QFileSystemWatcher::directoryChanged, path); }

private:
    struct Private;
    std::unique_ptr<Private> d;
};
