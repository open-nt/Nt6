// MIT License - see LICENSE
#include <QtWidgets/qfilesystemmodel.h>

#include <QtCore/qfilesystemwatcher.h>
#include <QtCore/qlocale.h>
#include <QtCore/qmimedata.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qurl.h>
#include <QtWidgets/qfileiconprovider.h>

#include <algorithm>

struct QFileSystemModel::Node {
    QString name;
    QFileInfo info;
    Node *parent = nullptr;
    std::vector<std::unique_ptr<Node>> children;
    bool populated = false;
    bool disabled = false; // a file that the name filters do not match, shown disabled

    QString path() const {
        if (!parent)
            return QStringLiteral("/");
        const QString base = parent->path();
        return base.endsWith(QChar('/')) ? base + name : base + QChar('/') + name;
    }
    int row() const {
        if (!parent)
            return 0;
        for (std::size_t i = 0; i < parent->children.size(); ++i)
            if (parent->children[i].get() == this)
                return static_cast<int>(i);
        return -1;
    }
};

namespace {

bool removeTree(const QString &path) {
    const QFileInfo info(path);
    if (info.isDir() && !info.isSymLink()) {
        const QDir dir(path);
        for (const QFileInfo &entry : dir.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System |
                                                        QDir::NoDotAndDotDot))
            if (!removeTree(entry.absoluteFilePath()))
                return false;
        return QDir().rmdir(path);
    }
    return QFile::remove(path);
}

} // namespace

/*!
    \class QFileSystemModel
    \inmodule NtWidgets
    \since 6.0
    \brief The QFileSystemModel class is a model of the files and folders of the local file system.

    The model has the whole file system, from \c{/}. A view shows a part of it with QAbstractItemView::setRootIndex()
    and the index of a folder:

    \code
    auto *model = new QFileSystemModel(this);
    model->setRootPath(QDir::homePath());
    model->setNameFilters({"*.png", "*.jpg"});
    tree->setModel(model);
    tree->setRootIndex(model->index(QDir::homePath()));
    \endcode

    It has four columns: the name, the size, the type and the date of the last change. A folder is read when its
    rows are first asked for, and folders that were read are watched with QFileSystemWatcher, so that the model
    follows the changes. Folders come before files, and both are sorted by name.

    \differences
    The folders are read at once, not in a thread, so a large folder blocks for the time it takes; directoryLoaded()
    is emitted after it is read. Drives are not listed apart: the root is \c{/}. The file system is the only one:
    there is no QDir::Drives on Windows or other platforms yet.
    \enddifferences

    \sa QFileIconProvider, QTreeView
*/

/*!
    \enum QFileSystemModel::Roles

    The roles of the data of the model.

    \value FileIconRole The icon of the file.
    \value FileInfoRole The QFileInfo of the file.
    \value QFilePermissions The permissions of the file as QFile::Permissions.
    \value FilePathRole The absolute path of the file.
    \value FileNameRole The name of the file.
    \value FilePermissions The permissions of the file as an \c int.
*/

/*!
    \enum QFileSystemModel::Option

    \value DontWatchForChanges The folders are not watched for changes.
    \value DontResolveSymlinks Symbolic links are shown as links, not as what they point to.
    \value DontUseCustomDirectoryIcons Folders have the folder icon; Nt has no others.
*/

/*!
    Constructs a model with the given \a parent.
*/
QFileSystemModel::QFileSystemModel(QObject *parent)
    : QAbstractItemModel(parent), m_root(std::make_unique<Node>()), m_ownProvider(std::make_unique<QFileIconProvider>()) {
    m_root->name = QStringLiteral("/");
    m_root->info = QFileInfo(QStringLiteral("/"));
    m_iconProvider = m_ownProvider.get();
    m_watcher = new QFileSystemWatcher(this);
    QObject::connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
                     [this](const QString &path) { directoryChanged(path); });
}

/*!
    Destroys the model.
*/
QFileSystemModel::~QFileSystemModel() = default;

QFileSystemModel::Node *QFileSystemModel::nodeOf(const QModelIndex &index) const {
    if (!index.isValid() || index.model() != this)
        return m_root.get();
    return static_cast<Node *>(index.internalPointer());
}

QModelIndex QFileSystemModel::indexOf(const Node *node, int column) const {
    if (!node)
        return QModelIndex();
    const int row = node->row();
    return row < 0 ? QModelIndex() : createIndex(row, column, node);
}

bool QFileSystemModel::passesNameFilters(const QFileInfo &info) const {
    if (m_nameFilters.isEmpty() || info.isDir())
        return true;
    for (const QString &filter : m_nameFilters) {
        const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(filter),
                                    QRegularExpression::CaseInsensitiveOption);
        if (re.match(info.fileName()).hasMatch())
            return true;
    }
    return false;
}

bool QFileSystemModel::visible(const QFileInfo &info) const {
    const QString name = info.fileName();
    if (name == QStringLiteral(".") || name == QStringLiteral(".."))
        return false;
    if (info.isDir()) {
        if (!(m_filters & (QDir::Dirs | QDir::AllDirs)))
            return false;
    } else if (!(m_filters & QDir::Files)) {
        return false;
    }
    if (info.isHidden() && !(m_filters & QDir::Hidden))
        return false;
    if (info.isSymLink() && (m_filters & QDir::NoSymLinks))
        return false;
    // Permission filters apply to files; folders are kept with AllDirs.
    const bool permissionDir = info.isDir() && (m_filters & QDir::AllDirs);
    if (!permissionDir) {
        if ((m_filters & QDir::Readable) && !info.isReadable())
            return false;
        if ((m_filters & QDir::Writable) && !info.isWritable())
            return false;
        if ((m_filters & QDir::Executable) && !info.isExecutable())
            return false;
    }
    return m_nameFilterDisables || passesNameFilters(info);
}

std::vector<QFileInfo> QFileSystemModel::entries(const QString &dir) const {
    std::vector<QFileInfo> out;
    const QDir directory(dir);
    for (const QFileInfo &info :
         directory.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot))
        if (visible(info))
            out.push_back(info);
    return out;
}

bool QFileSystemModel::lessThan(const Node *a, const Node *b) const {
    // Folders come first, whatever the column.
    const bool dirA = a->info.isDir(), dirB = b->info.isDir();
    if (dirA != dirB)
        return dirA;
    int c = 0;
    switch (m_sortColumn) {
    case 1:
        c = a->info.size() < b->info.size() ? -1 : (a->info.size() > b->info.size() ? 1 : 0);
        break;
    case 2:
        c = QString::compare(m_iconProvider->type(a->info), m_iconProvider->type(b->info), Qt::CaseInsensitive);
        break;
    case 3: {
        const QDateTime x = a->info.lastModified(), y = b->info.lastModified();
        c = x < y ? -1 : (y < x ? 1 : 0);
        break;
    }
    default:
        break;
    }
    if (c == 0)
        c = QString::compare(a->name, b->name, Qt::CaseInsensitive);
    if (c == 0)
        c = QString::compare(a->name, b->name, Qt::CaseSensitive);
    return m_sortOrder == Qt::AscendingOrder ? c < 0 : c > 0;
}

void QFileSystemModel::sortChildren(Node *node) const {
    std::stable_sort(node->children.begin(), node->children.end(),
                     [this](const std::unique_ptr<Node> &a, const std::unique_ptr<Node> &b) {
                         return lessThan(a.get(), b.get());
                     });
}

void QFileSystemModel::watch(const Node *node) const {
    if (!testOption(DontWatchForChanges) && node->info.isDir())
        m_watcher->addPath(node->path());
}

void QFileSystemModel::populate(Node *node) const {
    if (node->populated || !node->info.isDir())
        return;
    node->populated = true;
    for (const QFileInfo &info : entries(node->path())) {
        auto child = std::make_unique<Node>();
        child->name = info.fileName();
        child->info = info;
        child->parent = node;
        child->disabled = !passesNameFilters(info);
        node->children.push_back(std::move(child));
    }
    sortChildren(node);
    watch(node);
    const QString path = node->path();
    auto *self = const_cast<QFileSystemModel *>(this); // NOLINT(cppcoreguidelines-pro-type-const-cast): a cache
    QMetaObject::invokeMethod(
        self, [self, path] { emit self->directoryLoaded(path); }, Qt::QueuedConnection);
}

QFileSystemModel::Node *QFileSystemModel::nodeOfPath(const QString &path, bool fetch) const {
    const QString clean = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    Node *node = m_root.get();
    for (const QString &part : clean.split(QChar('/'), Qt::SkipEmptyParts)) {
        if (fetch)
            populate(node);
        Node *next = nullptr;
        for (const auto &child : node->children) {
            if (child->name == part) {
                next = child.get();
                break;
            }
        }
        if (!next)
            return nullptr;
        node = next;
    }
    return node;
}

/*!
    \reimp
*/
QModelIndex QFileSystemModel::index(int row, int column, const QModelIndex &parent) const {
    if (row < 0 || column < 0 || column >= 4)
        return QModelIndex();
    Node *node = nodeOf(parent);
    if (parent.isValid() && parent.column() != 0)
        return QModelIndex();
    if (!parent.isValid()) // the one top-level item: the root of the file system
        return row == 0 ? createIndex(0, column, m_root.get()) : QModelIndex();
    populate(node);
    if (row >= static_cast<int>(node->children.size()))
        return QModelIndex();
    return createIndex(row, column, node->children[static_cast<std::size_t>(row)].get());
}

/*!
    \overload

    Returns the index of the file or folder \a path in \a column, or an invalid index if it is not in the model,
    because it does not exist or a filter leaves it out. The folders on the way are read.
*/
QModelIndex QFileSystemModel::index(const QString &path, int column) const {
    if (path.isEmpty())
        return QModelIndex();
    return indexOf(nodeOfPath(path, true), column);
}

/*!
    \reimp
*/
QModelIndex QFileSystemModel::parent(const QModelIndex &child) const {
    if (!child.isValid())
        return QModelIndex();
    const Node *node = nodeOf(child);
    return node->parent ? indexOf(node->parent) : QModelIndex();
}

/*!
    \reimp
*/
QModelIndex QFileSystemModel::sibling(int row, int column, const QModelIndex &idx) const {
    if (!idx.isValid())
        return QModelIndex();
    if (row == idx.row())
        return column >= 0 && column < 4 ? createIndex(row, column, idx.internalPointer()) : QModelIndex();
    return index(row, column, parent(idx));
}

/*!
    \reimp

    Returns \c true for a folder, which may have entries, without reading it.
*/
bool QFileSystemModel::hasChildren(const QModelIndex &parent) const {
    if (parent.isValid() && parent.column() != 0)
        return false;
    const Node *node = nodeOf(parent);
    if (!parent.isValid())
        return true;
    return node->info.isDir() && (!node->populated || !node->children.empty());
}

/*!
    \reimp
*/
bool QFileSystemModel::canFetchMore(const QModelIndex &parent) const {
    const Node *node = nodeOf(parent);
    return parent.isValid() && node->info.isDir() && !node->populated;
}

/*!
    \reimp
*/
void QFileSystemModel::fetchMore(const QModelIndex &parent) {
    if (parent.isValid())
        populate(nodeOf(parent));
}

/*!
    \reimp

    Returns the number of entries of the folder \a parent, which is read if it was not.
*/
int QFileSystemModel::rowCount(const QModelIndex &parent) const {
    if (!parent.isValid())
        return 1;
    if (parent.column() != 0)
        return 0;
    Node *node = nodeOf(parent);
    populate(node);
    return static_cast<int>(node->children.size());
}

/*!
    \reimp
*/
int QFileSystemModel::columnCount(const QModelIndex &parent) const {
    return parent.isValid() && parent.column() != 0 ? 0 : 4;
}

/*!
    Returns the data of "My Computer" for \a role: its name for Qt::DisplayRole and its icon for Qt::DecorationRole.
*/
QVariant QFileSystemModel::myComputer(int role) const {
    if (role == Qt::DisplayRole)
        return QStringLiteral("Computer");
    if (role == Qt::DecorationRole)
        return QVariant::fromValue(m_iconProvider->icon(QAbstractFileIconProvider::Computer));
    return QVariant();
}

/*!
    \reimp
*/
QVariant QFileSystemModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this)
        return QVariant();
    const Node *node = nodeOf(index);
    const QFileInfo &info = node->info;
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        switch (index.column()) {
        case 0:
            return node->name;
        case 1:
            return info.isDir() ? QString() : QLocale().formattedDataSize(info.size());
        case 2:
            return m_iconProvider->type(info);
        case 3:
            return QLocale().toString(info.lastModified(), QLocale::ShortFormat);
        default:
            return QVariant();
        }
    case Qt::DecorationRole:
        if (index.column() == 0)
            return QVariant::fromValue(m_iconProvider->icon(info));
        return QVariant();
    case Qt::TextAlignmentRole:
        if (index.column() == 1)
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        return QVariant();
    case FilePathRole:
        return node->path();
    case FileNameRole:
        return node->name;
    case FilePermissions:
    case QFilePermissions:
        return static_cast<int>(info.permissions());
    case FileInfoRole:
        return QVariant::fromValue(info);
    default:
        return QVariant();
    }
}

/*!
    \reimp

    Renames the file of \a index to \a value, if the model is not read-only, and emits fileRenamed(). Returns
    \c true if the file was renamed.
*/
bool QFileSystemModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (m_readOnly || !index.isValid() || index.column() != 0 || role != Qt::EditRole)
        return false;
    Node *node = nodeOf(index);
    const QString newName = value.toString();
    if (!node->parent || newName.isEmpty() || newName.contains(QChar('/')) || newName == node->name)
        return false;
    const QString dir = node->parent->path();
    const QString oldName = node->name;
    if (!QFile::rename(node->path(), QDir(dir).filePath(newName)))
        return false;
    node->name = newName;
    node->info = QFileInfo(QDir(dir).filePath(newName));
    emit dataChanged(index, index.siblingAtColumn(3));
    emit layoutAboutToBeChanged();
    const QModelIndexList from = persistentIndexList();
    std::vector<std::pair<Node *, int>> nodes;
    for (const QModelIndex &i : from)
        nodes.emplace_back(static_cast<Node *>(i.internalPointer()), i.column());
    sortChildren(node->parent);
    QModelIndexList to;
    for (const auto &[n, column] : nodes)
        to.append(indexOf(n, column));
    changePersistentIndexList(from, to);
    emit layoutChanged();
    emit fileRenamed(dir, oldName, newName);
    return true;
}

/*!
    \reimp
*/
QVariant QFileSystemModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractItemModel::headerData(section, orientation, role);
    switch (section) {
    case 0:
        return QStringLiteral("Name");
    case 1:
        return QStringLiteral("Size");
    case 2:
        return QStringLiteral("Type");
    case 3:
        return QStringLiteral("Date Modified");
    default:
        return QVariant();
    }
}

/*!
    \reimp
*/
Qt::ItemFlags QFileSystemModel::flags(const QModelIndex &index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;
    const Node *node = nodeOf(index);
    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
    if (!node->disabled)
        flags |= Qt::ItemIsEnabled;
    if (!m_readOnly && index.column() == 0 && node->parent)
        flags |= Qt::ItemIsEditable;
    if (!m_readOnly && node->info.isDir())
        flags |= Qt::ItemIsDropEnabled;
    if (!node->info.isDir())
        flags |= Qt::ItemNeverHasChildren;
    return flags;
}

/*!
    \reimp

    Sorts the entries of every folder that was read by \a column in \a order; folders stay before files.
*/
void QFileSystemModel::sort(int column, Qt::SortOrder order) {
    m_sortColumn = column;
    m_sortOrder = order;
    emit layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);
    const QModelIndexList from = persistentIndexList();
    std::vector<std::pair<Node *, int>> nodes;
    for (const QModelIndex &i : from)
        nodes.emplace_back(static_cast<Node *>(i.internalPointer()), i.column());
    std::vector<Node *> pending{m_root.get()};
    while (!pending.empty()) {
        Node *node = pending.back();
        pending.pop_back();
        sortChildren(node);
        for (const auto &child : node->children)
            if (child->populated)
                pending.push_back(child.get());
    }
    QModelIndexList to;
    for (const auto &[n, c] : nodes)
        to.append(indexOf(n, c));
    changePersistentIndexList(from, to);
    emit layoutChanged({}, QAbstractItemModel::VerticalSortHint);
}

/*!
    \reimp
*/
QStringList QFileSystemModel::mimeTypes() const { return {QStringLiteral("text/uri-list")}; }

/*!
    \reimp

    Returns the files of \a indexes as \c file: URLs.
*/
QMimeData *QFileSystemModel::mimeData(const QModelIndexList &indexes) const {
    QList<QUrl> urls;
    for (const QModelIndex &index : indexes)
        if (index.column() == 0)
            urls.append(QUrl::fromLocalFile(filePath(index)));
    auto *data = new QMimeData;
    data->setUrls(urls);
    return data;
}

/*!
    \reimp
*/
Qt::DropActions QFileSystemModel::supportedDropActions() const {
    return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
}

/*!
    \reimp
*/
QHash<int, QByteArray> QFileSystemModel::roleNames() const {
    QHash<int, QByteArray> names = QAbstractItemModel::roleNames();
    names.insert(FileIconRole, "fileIcon");
    names.insert(FilePathRole, "filePath");
    names.insert(FileNameRole, "fileName");
    names.insert(FilePermissions, "filePermissions");
    names.insert(FileInfoRole, "fileInfo");
    return names;
}

/*!
    Sets the folder that the model is about to \a path, reads it and watches it, and returns its index. A view
    shows it with QAbstractItemView::setRootIndex(); the model has the whole file system in any case.
*/
QModelIndex QFileSystemModel::setRootPath(const QString &path) {
    const QString clean = path.isEmpty() ? QString() : QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (clean != m_rootPath) {
        m_rootPath = clean;
        emit rootPathChanged(m_rootPath);
    }
    if (m_rootPath.isEmpty())
        return QModelIndex();
    Node *node = nodeOfPath(m_rootPath, true);
    if (node)
        populate(node);
    return indexOf(node);
}

/*!
    \fn QString QFileSystemModel::rootPath() const

    Returns the path that setRootPath() set.
*/

/*!
    Returns the folder of rootPath().
*/
QDir QFileSystemModel::rootDirectory() const {
    QDir dir(m_rootPath.isEmpty() ? QStringLiteral("/") : m_rootPath);
    dir.setNameFilters(m_nameFilters);
    dir.setFilter(m_filters);
    return dir;
}

/*!
    Sets the provider of the icons and the type names to \a provider, which the model does not take ownership of;
    \c nullptr brings back the QFileIconProvider of the model.
*/
void QFileSystemModel::setIconProvider(QAbstractFileIconProvider *provider) {
    m_iconProvider = provider ? provider : m_ownProvider.get();
    refreshAll();
}

/*!
    Returns the provider of the icons and the type names.
*/
QAbstractFileIconProvider *QFileSystemModel::iconProvider() const { return m_iconProvider; }

/*!
    Sets which entries the model has to \a filters: QDir::AllEntries, QDir::NoDotAndDotDot and QDir::AllDirs by
    default. The folders that were read are read again.
*/
void QFileSystemModel::setFilter(QDir::Filters filters) {
    if (filters == m_filters)
        return;
    m_filters = filters;
    refreshAll();
}

/*!
    \fn QDir::Filters QFileSystemModel::filter() const

    Returns the filters of the entries.
*/

/*!
    \fn void QFileSystemModel::setResolveSymlinks(bool enable)

    Sets whether symbolic links are shown as what they point to to \a enable.
*/

/*!
    \fn bool QFileSystemModel::resolveSymlinks() const

    Returns whether symbolic links are shown as what they point to.
*/

/*!
    \fn void QFileSystemModel::setReadOnly(bool enable)

    Sets whether the model refuses to change the file system to \a enable; it is read-only by default.
*/

/*!
    \fn bool QFileSystemModel::isReadOnly() const

    Returns whether the model refuses to change the file system.
*/

/*!
    Sets whether the files that the name filters leave out are shown disabled, \a enable \c true, the default, or
    not at all.
*/
void QFileSystemModel::setNameFilterDisables(bool enable) {
    if (enable == m_nameFilterDisables)
        return;
    m_nameFilterDisables = enable;
    refreshAll();
}

/*!
    \fn bool QFileSystemModel::nameFilterDisables() const

    Returns whether the files that the name filters leave out are shown disabled.
*/

/*!
    Sets the wildcard patterns that the names of the files must match to \a filters, such as \c{*.png}; folders are
    not filtered.
*/
void QFileSystemModel::setNameFilters(const QStringList &filters) {
    m_nameFilters = filters;
    refreshAll();
}

/*!
    \fn QStringList QFileSystemModel::nameFilters() const

    Returns the patterns that the names of the files must match.
*/

/*!
    Turns \a option on, if \a on is \c true, or off.
*/
void QFileSystemModel::setOption(Option option, bool on) {
    Options options = m_options;
    options.setFlag(option, on);
    setOptions(options);
}

/*!
    \fn bool QFileSystemModel::testOption(Option option) const

    Returns \c true if \a option is on.
*/

/*!
    Sets the options of the model to \a options.
*/
void QFileSystemModel::setOptions(Options options) {
    const bool unwatch = options.testFlag(DontWatchForChanges) && !m_options.testFlag(DontWatchForChanges);
    m_options = options;
    if (unwatch) {
        const QStringList dirs = m_watcher->directories();
        if (!dirs.isEmpty())
            m_watcher->removePaths(dirs);
    }
}

/*!
    \fn QFileSystemModel::Options QFileSystemModel::options() const

    Returns the options of the model.
*/

/*!
    Returns the absolute path of the file of \a index.
*/
QString QFileSystemModel::filePath(const QModelIndex &index) const {
    return index.isValid() ? nodeOf(index)->path() : QString();
}

/*!
    Returns \c true if \a index is a folder.
*/
bool QFileSystemModel::isDir(const QModelIndex &index) const { return !index.isValid() || nodeOf(index)->info.isDir(); }

/*!
    Returns the size of the file of \a index in bytes.
*/
qint64 QFileSystemModel::size(const QModelIndex &index) const {
    return index.isValid() ? nodeOf(index)->info.size() : 0;
}

/*!
    Returns the name of the type of the file of \a index.
*/
QString QFileSystemModel::type(const QModelIndex &index) const {
    return index.isValid() ? m_iconProvider->type(nodeOf(index)->info) : QString();
}

/*!
    Returns when the file of \a index last changed.
*/
QDateTime QFileSystemModel::lastModified(const QModelIndex &index) const {
    return index.isValid() ? nodeOf(index)->info.lastModified() : QDateTime();
}

/*!
    Makes the folder \a name in the folder \a parent and returns its index, or an invalid index if the model is
    read-only or the folder could not be made.
*/
QModelIndex QFileSystemModel::mkdir(const QModelIndex &parent, const QString &name) {
    if (m_readOnly || !parent.isValid())
        return QModelIndex();
    Node *node = nodeOf(parent);
    if (!QDir(node->path()).mkdir(name))
        return QModelIndex();
    populate(node);
    refresh(node);
    return index(QDir(node->path()).filePath(name));
}

/*!
    Removes the empty folder of \a index, if the model is not read-only. Returns \c true on success.
*/
bool QFileSystemModel::rmdir(const QModelIndex &index) {
    if (m_readOnly || !index.isValid())
        return false;
    Node *node = nodeOf(index);
    if (!node->parent || !QDir().rmdir(node->path()))
        return false;
    refresh(node->parent);
    return true;
}

/*!
    \fn QString QFileSystemModel::fileName(const QModelIndex &index) const

    Returns the name of the file of \a index.
*/

/*!
    \fn QIcon QFileSystemModel::fileIcon(const QModelIndex &index) const

    Returns the icon of the file of \a index.
*/

/*!
    Returns the permissions of the file of \a index.
*/
QFile::Permissions QFileSystemModel::permissions(const QModelIndex &index) const {
    return index.isValid() ? nodeOf(index)->info.permissions() : QFile::Permissions();
}

/*!
    Returns the QFileInfo of the file of \a index.
*/
QFileInfo QFileSystemModel::fileInfo(const QModelIndex &index) const {
    return index.isValid() ? nodeOf(index)->info : QFileInfo();
}

/*!
    Removes the file of \a index, or the folder with all it holds, if the model is not read-only. Returns \c true on
    success.
*/
bool QFileSystemModel::remove(const QModelIndex &index) {
    if (m_readOnly || !index.isValid())
        return false;
    Node *node = nodeOf(index);
    if (!node->parent)
        return false;
    Node *parent = node->parent;
    const bool ok = removeTree(node->path());
    refresh(parent);
    return ok;
}

// Reads the folder of `node` again and makes the model follow: entries that went are removed, new ones inserted,
// and changed ones updated.
void QFileSystemModel::refresh(Node *node) {
    if (!node->populated)
        return;
    const QModelIndex parent = indexOf(node);
    if (!QFileInfo::exists(node->path())) {
        if (!node->children.empty()) {
            beginRemoveRows(parent, 0, static_cast<int>(node->children.size()) - 1);
            node->children.clear();
            endRemoveRows();
        }
        return;
    }
    const std::vector<QFileInfo> now = entries(node->path());
    // Removed.
    for (int row = static_cast<int>(node->children.size()) - 1; row >= 0; --row) {
        const QString name = node->children[static_cast<std::size_t>(row)]->name;
        const bool still = std::any_of(now.begin(), now.end(), [&](const QFileInfo &i) { return i.fileName() == name; });
        if (!still) {
            beginRemoveRows(parent, row, row);
            node->children.erase(node->children.begin() + row);
            endRemoveRows();
        }
    }
    for (const QFileInfo &info : now) {
        const auto it = std::find_if(node->children.begin(), node->children.end(),
                                     [&](const std::unique_ptr<Node> &c) { return c->name == info.fileName(); });
        if (it != node->children.end()) {
            // Changed.
            Node *child = it->get();
            const bool disabled = !passesNameFilters(info);
            if (child->info.lastModified() != info.lastModified() || child->info.size() != info.size() ||
                child->disabled != disabled) {
                child->info = info;
                child->disabled = disabled;
                emit dataChanged(indexOf(child, 0), indexOf(child, 3));
            }
            continue;
        }
        // New: inserted where it sorts.
        auto child = std::make_unique<Node>();
        child->name = info.fileName();
        child->info = info;
        child->parent = node;
        child->disabled = !passesNameFilters(info);
        const auto at = std::find_if(node->children.begin(), node->children.end(),
                                     [&](const std::unique_ptr<Node> &c) { return lessThan(child.get(), c.get()); });
        const int row = static_cast<int>(at - node->children.begin());
        beginInsertRows(parent, row, row);
        node->children.insert(node->children.begin() + row, std::move(child));
        endInsertRows();
    }
}

void QFileSystemModel::refreshAll() {
    std::vector<Node *> pending{m_root.get()};
    while (!pending.empty()) {
        Node *node = pending.back();
        pending.pop_back();
        refresh(node);
        for (const auto &child : node->children)
            if (child->populated)
                pending.push_back(child.get());
    }
}

void QFileSystemModel::directoryChanged(const QString &path) {
    if (Node *node = nodeOfPath(path, false))
        refresh(node);
}

#include "moc_qfilesystemmodel.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
