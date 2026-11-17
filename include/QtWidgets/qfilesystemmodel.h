// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qstringlist.h>
#include <QtGui/qicon.h>

#include <memory>

class QAbstractFileIconProvider;
class QFileSystemWatcher;

class QFileSystemModel : public QAbstractItemModel {
    Q_OBJECT
    Q_PROPERTY(bool resolveSymlinks READ resolveSymlinks WRITE setResolveSymlinks)
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
    Q_PROPERTY(bool nameFilterDisables READ nameFilterDisables WRITE setNameFilterDisables)
public:
    enum Roles {
        FileIconRole = Qt::DecorationRole,
        FileInfoRole = Qt::UserRole - 4,
        QFilePermissions = Qt::UserRole - 3,
        FilePathRole = Qt::UserRole + 1,
        FileNameRole = Qt::UserRole + 2,
        FilePermissions = Qt::UserRole + 3
    };
    enum Option { DontWatchForChanges = 0x00000001, DontResolveSymlinks = 0x00000002, DontUseCustomDirectoryIcons = 0x00000004 };
    Q_DECLARE_FLAGS(Options, Option)

    explicit QFileSystemModel(QObject *parent = nullptr);
    ~QFileSystemModel() override;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex index(const QString &path, int column = 0) const;
    QModelIndex parent(const QModelIndex &child) const override;
    using QObject::parent;
    QModelIndex sibling(int row, int column, const QModelIndex &idx) const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant myComputer(int role = Qt::DisplayRole) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    Qt::DropActions supportedDropActions() const override;
    QHash<int, QByteArray> roleNames() const override;

    QModelIndex setRootPath(const QString &path);
    QString rootPath() const { return m_rootPath; }
    QDir rootDirectory() const;

    void setIconProvider(QAbstractFileIconProvider *provider);
    QAbstractFileIconProvider *iconProvider() const;
    void setFilter(QDir::Filters filters);
    QDir::Filters filter() const { return m_filters; }
    void setResolveSymlinks(bool enable) { setOption(DontResolveSymlinks, !enable); }
    bool resolveSymlinks() const { return !testOption(DontResolveSymlinks); }
    void setReadOnly(bool enable) { m_readOnly = enable; }
    bool isReadOnly() const { return m_readOnly; }
    void setNameFilterDisables(bool enable);
    bool nameFilterDisables() const { return m_nameFilterDisables; }
    void setNameFilters(const QStringList &filters);
    QStringList nameFilters() const { return m_nameFilters; }
    void setOption(Option option, bool on = true);
    bool testOption(Option option) const { return m_options.testFlag(option); }
    void setOptions(Options options);
    Options options() const { return m_options; }

    QString filePath(const QModelIndex &index) const;
    bool isDir(const QModelIndex &index) const;
    qint64 size(const QModelIndex &index) const;
    QString type(const QModelIndex &index) const;
    QDateTime lastModified(const QModelIndex &index) const;
    QModelIndex mkdir(const QModelIndex &parent, const QString &name);
    bool rmdir(const QModelIndex &index);
    QString fileName(const QModelIndex &index) const { return index.data(FileNameRole).toString(); }
    QIcon fileIcon(const QModelIndex &index) const { return index.data(Qt::DecorationRole).value<QIcon>(); }
    QFile::Permissions permissions(const QModelIndex &index) const;
    QFileInfo fileInfo(const QModelIndex &index) const;
    bool remove(const QModelIndex &index);

signals:
    /*!
        This signal is emitted when the root path changes to \a newPath.
    */
    void rootPathChanged(const QString &newPath) { emitSignal(&QFileSystemModel::rootPathChanged, newPath); }
    /*!
        This signal is emitted when the file \a oldName in the folder \a path is renamed to \a newName with setData().
    */
    void fileRenamed(const QString &path, const QString &oldName, const QString &newName) {
        emitSignal(&QFileSystemModel::fileRenamed, path, oldName, newName);
    }
    /*!
        This signal is emitted when the entries of the folder \a path were read.
    */
    void directoryLoaded(const QString &path) { emitSignal(&QFileSystemModel::directoryLoaded, path); }

private:
    struct Node;
    Q_DISABLE_COPY(QFileSystemModel)
    Node *nodeOf(const QModelIndex &index) const;
    QModelIndex indexOf(const Node *node, int column = 0) const;
    Node *nodeOfPath(const QString &path, bool fetch) const;
    void populate(Node *node) const;
    std::vector<QFileInfo> entries(const QString &dir) const;
    bool visible(const QFileInfo &info) const;
    bool passesNameFilters(const QFileInfo &info) const;
    void sortChildren(Node *node) const;
    bool lessThan(const Node *a, const Node *b) const;
    void refresh(Node *node);
    void refreshAll();
    void directoryChanged(const QString &path);
    void watch(const Node *node) const;

    std::unique_ptr<Node> m_root; // the root of the file system, "/"
    QString m_rootPath;
    QDir::Filters m_filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs;
    QStringList m_nameFilters;
    bool m_nameFilterDisables = true;
    bool m_readOnly = true;
    Options m_options;
    int m_sortColumn = 0;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
    QAbstractFileIconProvider *m_iconProvider = nullptr;
    std::unique_ptr<QAbstractFileIconProvider> m_ownProvider;
    QFileSystemWatcher *m_watcher = nullptr;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QFileSystemModel::Options)
