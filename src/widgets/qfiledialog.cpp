// MIT License - see LICENSE
#include <QtWidgets/qfiledialog.h>

#include <QtCore/qfileinfo.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qdialogbuttonbox.h>
#include <QtWidgets/qgridlayout.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qlistwidget.h>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qtoolbutton.h>

#include <algorithm>

namespace {

// The patterns of a name filter such as "Images (*.png *.jpg)", or "*" for none.
QStringList patternsOf(const QString &filter) {
    QString inner = filter;
    const qsizetype open = filter.lastIndexOf(QLatin1Char('('));
    const qsizetype close = filter.lastIndexOf(QLatin1Char(')'));
    if (open >= 0 && close > open)
        inner = filter.mid(open + 1, close - open - 1);
    QStringList patterns;
    for (const QString &part : inner.split(QLatin1Char(' ')))
        if (!part.trimmed().isEmpty())
            patterns.append(part.trimmed());
    if (patterns.isEmpty())
        patterns.append(QStringLiteral("*"));
    return patterns;
}

// The name of a filter without its patterns: "Images" for "Images (*.png *.jpg)".
QString shortName(const QString &filter) {
    const qsizetype open = filter.lastIndexOf(QLatin1Char('('));
    return open > 0 ? filter.left(open).trimmed() : filter;
}

// The filters of a string such as "Images (*.png);;Text (*.txt)".
QStringList splitFilters(const QString &filter) {
    QStringList out;
    for (const QString &part : filter.split(QStringLiteral(";;")))
        if (!part.trimmed().isEmpty())
            out.append(part.trimmed());
    return out;
}

QString withSlash(const QString &dir) { return dir.endsWith(QLatin1Char('/')) ? dir : dir + QLatin1Char('/'); }

} // namespace

/*!
    \class QFileDialog
    \brief The QFileDialog class lets the user choose files or a directory.

    The static functions show a file dialog and wait for the user:

    \code
    const QString name = QFileDialog::getOpenFileName(window, "Open Image", QDir::homePath(),
                                                      "Images (*.png *.jpg);;All files (*)");
    if (!name.isEmpty())
        open(name);
    \endcode

    The dialog lists the directories and then the files of a directory that match the chosen name filter. The user
    enters a directory by double-clicking it, goes up with the Up button, or types a path. The file mode says what may
    be chosen: any file name (for saving), an existing file, several existing files, or a directory. In AcceptSave
    mode the dialog asks before it replaces an existing file, and adds defaultSuffix() to a name without one.

    \differences
    The dialog is always drawn by Nt, as a plain list of names: there is no detail view, no icons, no sidebar of places,
    no history and no file system watching (the list is read when a directory is entered). There are no URL variants
    (\c getOpenFileUrl() and the others), no \c setFilter(), \c setProxyModel(), \c setIconProvider() or
    \c saveState(), and no \c open() with a receiver. The texts are not translated.
    \enddifferences

    \sa QDir, QFileInfo
*/

/*!
    \enum QFileDialog::ViewMode

    How the files are shown; recorded only, the dialog shows a list.

    \value Detail Names with their size, type and date.
    \value List Names only.
*/

/*!
    \enum QFileDialog::FileMode

    What the user may choose.

    \value AnyFile Any file name, whether the file exists or not, as for saving.
    \value ExistingFile One existing file.
    \value Directory A directory.
    \value ExistingFiles One or more existing files.
*/

/*!
    \enum QFileDialog::AcceptMode

    Whether the dialog opens or saves.

    \value AcceptOpen The accept button is Open.
    \value AcceptSave The accept button is Save, and replacing a file is confirmed.
*/

/*!
    \enum QFileDialog::DialogLabel

    The labels that setLabelText() changes.

    \value LookIn The label of the directory.
    \value FileName The label of the file name.
    \value FileType The label of the name filters.
    \value Accept The accept button.
    \value Reject The reject button.
*/

/*!
    \enum QFileDialog::Option

    Options of the dialog. Options is a typedef for \c{QFlags<Option>}.

    \value ShowDirsOnly Show only directories, in Directory mode.
    \value DontResolveSymlinks Accepted for compatibility; links are shown as what they point to.
    \value DontConfirmOverwrite Do not ask before replacing a file in AcceptSave mode.
    \value DontUseNativeDialog Accepted for compatibility; the dialog is never native.
    \value ReadOnly Accepted for compatibility; the dialog never changes the file system.
    \value HideNameFilterDetails Show the names of the filters without their patterns.
    \value DontUseCustomDirectoryIcons Accepted for compatibility; there are no icons.
*/

/*!
    Constructs a file dialog with the given \a parent and window \a flags, showing the current directory.
*/
QFileDialog::QFileDialog(QWidget *parent, Qt::WindowFlags flags) : QDialog(parent, flags) {
    init(QString(), QString(), QString());
}

/*!
    Constructs a file dialog with the given \a parent and window \a caption, showing \a directory, with the name
    filters of \a filter separated by ";;".
*/
QFileDialog::QFileDialog(QWidget *parent, const QString &caption, const QString &directory, const QString &filter)
    : QDialog(parent) {
    init(caption, directory, filter);
}

/*!
    Destroys the file dialog.
*/
QFileDialog::~QFileDialog() = default;

void QFileDialog::init(const QString &caption, const QString &directory, const QString &filter) {
    setWindowTitle(caption);
    auto *grid = new QGridLayout(this);
    m_lookInLabel = new QLabel(QStringLiteral("Look in:"), this);
    m_lookIn = new QLineEdit(this);
    m_up = new QToolButton(this);
    m_up->setText(QStringLiteral("Up"));
    m_up->setToolTip(QStringLiteral("Parent directory"));
    auto *top = new QHBoxLayout;
    top->addWidget(m_lookIn);
    top->addWidget(m_up);
    grid->addWidget(m_lookInLabel, 0, 0);
    grid->addLayout(top, 0, 1);
    m_list = new QListWidget(this);
    grid->addWidget(m_list, 1, 0, 1, 2);
    m_fileNameLabel = new QLabel(QStringLiteral("File name:"), this);
    m_fileName = new QLineEdit(this);
    grid->addWidget(m_fileNameLabel, 2, 0);
    grid->addWidget(m_fileName, 2, 1);
    m_typeLabel = new QLabel(QStringLiteral("Files of type:"), this);
    m_types = new QComboBox(this);
    grid->addWidget(m_typeLabel, 3, 0);
    grid->addWidget(m_types, 3, 1);
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel, this);
    grid->addWidget(m_buttons, 4, 0, 1, 2);
    grid->setRowStretch(1, 1);
    grid->setColumnStretch(1, 1);

    QObject::connect(m_buttons, &QDialogButtonBox::accepted, this, &QFileDialog::accept);
    QObject::connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(m_up, &QToolButton::clicked, this, [this] {
        QDir dir(m_directory);
        if (dir.cdUp())
            setDirectory(dir.absolutePath());
    });
    QObject::connect(m_lookIn, &QLineEdit::returnPressed, this, [this] {
        const QFileInfo info(m_lookIn->text());
        if (info.isDir())
            setDirectory(info.absoluteFilePath());
        else
            m_lookIn->setText(m_directory);
    });
    QObject::connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) { itemActivated(item); });
    QObject::connect(m_list, &QListWidget::itemSelectionChanged, this, [this] { selectionChanged(); });
    QObject::connect(m_types, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_updating || index < 0 || index >= m_nameFilters.size())
            return;
        refresh();
        emit filterSelected(m_nameFilters.at(index));
    });

    setNameFilter(filter);
    updateMode();
    QString start = directory;
    if (!start.isEmpty() && !QFileInfo(start).isDir()) {
        // A file name: show its directory and select it.
        const QFileInfo info(start);
        start = info.absolutePath();
        m_fileName->setText(info.fileName());
    }
    setDirectory(start.isEmpty() ? QDir::currentPath() : start);
}

/*!
    Shows \a directory, which becomes the directory of relative file names. Emits directoryEntered().

    \sa directory()
*/
void QFileDialog::setDirectory(const QString &directory) {
    const QString path = QDir::cleanPath(QDir(directory).absolutePath());
    m_directory = path;
    m_lookIn->setText(path);
    m_up->setEnabled(!QDir(path).isRoot());
    refresh();
    emit directoryEntered(path);
}

/*!
    \fn void QFileDialog::setDirectory(const QDir &directory)

    \overload

    Shows \a directory.
*/

/*!
    \fn QDir QFileDialog::directory() const

    Returns the directory that the dialog shows.

    \sa setDirectory()
*/

/*!
    Puts \a filename in the file name field and selects it in the list. A path with a directory shows that directory.

    \sa selectedFiles()
*/
void QFileDialog::selectFile(const QString &filename) {
    QString name = filename;
    if (name.contains(QLatin1Char('/'))) {
        const QFileInfo info(resolve(name));
        if (info.absolutePath() != m_directory)
            setDirectory(info.absolutePath());
        name = info.fileName();
    }
    m_fileName->setText(name);
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->text() == name) {
            m_list->setCurrentRow(i);
            break;
        }
    }
}

/*!
    Returns the files that were chosen when the dialog was accepted, as absolute paths. Before that, returns the files
    that the file name field names.
*/
QStringList QFileDialog::selectedFiles() const {
    if (!m_selected.isEmpty())
        return m_selected;
    QStringList files;
    for (const QString &name : typedNames())
        files.append(resolve(name));
    if (files.isEmpty() && m_fileMode == Directory)
        files.append(m_directory);
    return files;
}

/*!
    Sets the name filters to the ones in \a filter, separated by ";;", such as "Images (*.png *.jpg);;Text (*.txt)".
    An empty filter shows all files.

    \sa setNameFilters()
*/
void QFileDialog::setNameFilter(const QString &filter) { setNameFilters(splitFilters(filter)); }

/*!
    Sets the name filters to \a filters. The first one is chosen.

    \sa nameFilters(), selectNameFilter()
*/
void QFileDialog::setNameFilters(const QStringList &filters) {
    m_nameFilters = filters.isEmpty() ? QStringList{QStringLiteral("All Files (*)")} : filters;
    m_updating = true;
    m_types->clear();
    for (const QString &f : m_nameFilters)
        m_types->addItem(testOption(HideNameFilterDetails) ? shortName(f) : f);
    m_types->setCurrentIndex(0);
    m_updating = false;
    refresh();
}

/*!
    \fn QStringList QFileDialog::nameFilters() const

    Returns the name filters.

    \sa setNameFilters()
*/

/*!
    Chooses the name filter \a filter, if it is one of nameFilters().

    \sa selectedNameFilter()
*/
void QFileDialog::selectNameFilter(const QString &filter) {
    const int index = static_cast<int>(m_nameFilters.indexOf(filter));
    if (index >= 0)
        m_types->setCurrentIndex(index);
}

/*!
    Returns the name filter that is chosen.

    \sa selectNameFilter()
*/
QString QFileDialog::selectedNameFilter() const {
    const int index = m_types->currentIndex();
    return index >= 0 && index < m_nameFilters.size() ? m_nameFilters.at(index) : QString();
}

/*!
    \fn void QFileDialog::setViewMode(ViewMode mode)

    Sets how files are shown to \a mode; recorded only.

    \sa viewMode()
*/

/*!
    \fn QFileDialog::ViewMode QFileDialog::viewMode() const

    Returns how files are shown, Detail by default.
*/

/*!
    Sets what the user may choose to \a mode.

    \sa fileMode()
*/
void QFileDialog::setFileMode(FileMode mode) {
    m_fileMode = mode;
    updateMode();
    refresh();
}

/*!
    \fn QFileDialog::FileMode QFileDialog::fileMode() const

    Returns what the user may choose, AnyFile by default.

    \sa setFileMode()
*/

/*!
    Sets whether the dialog opens or saves to \a mode.

    \sa acceptMode()
*/
void QFileDialog::setAcceptMode(AcceptMode mode) {
    m_acceptMode = mode;
    updateMode();
}

/*!
    \fn QFileDialog::AcceptMode QFileDialog::acceptMode() const

    Returns whether the dialog opens or saves, AcceptOpen by default.

    \sa setAcceptMode()
*/

/*!
    Sets the suffix that is added to a file name without one in AcceptSave mode to \a suffix, without its dot.

    \sa defaultSuffix()
*/
void QFileDialog::setDefaultSuffix(const QString &suffix) {
    m_defaultSuffix = suffix.startsWith(QLatin1Char('.')) ? suffix.mid(1) : suffix;
}

/*!
    \fn QString QFileDialog::defaultSuffix() const

    Returns the suffix that is added to a file name without one.

    \sa setDefaultSuffix()
*/

/*!
    Sets the text of \a label to \a text.

    \sa labelText()
*/
void QFileDialog::setLabelText(DialogLabel label, const QString &text) {
    switch (label) {
    case LookIn:
        m_lookInLabel->setText(text);
        break;
    case FileName:
        m_fileNameLabel->setText(text);
        break;
    case FileType:
        m_typeLabel->setText(text);
        break;
    case Accept:
        m_acceptText = text;
        break;
    case Reject:
        m_rejectText = text;
        break;
    }
    updateMode();
}

/*!
    Returns the text of \a label.

    \sa setLabelText()
*/
QString QFileDialog::labelText(DialogLabel label) const {
    switch (label) {
    case LookIn:
        return m_lookInLabel->text();
    case FileName:
        return m_fileNameLabel->text();
    case FileType:
        return m_typeLabel->text();
    case Accept:
        for (QAbstractButton *b : m_buttons->buttons())
            if (m_buttons->buttonRole(b) == QDialogButtonBox::AcceptRole)
                return b->text();
        return QString();
    case Reject:
        return m_buttons->button(QDialogButtonBox::Cancel)->text();
    }
    return QString();
}

/*!
    Sets \a option if \a on is \c true, and clears it otherwise.

    \sa testOption()
*/
void QFileDialog::setOption(Option option, bool on) {
    Options options = m_options;
    options.setFlag(option, on);
    setOptions(options);
}

/*!
    \fn bool QFileDialog::testOption(Option option) const

    Returns whether \a option is set.
*/

/*!
    Sets the options of the dialog to \a options.

    \sa options()
*/
void QFileDialog::setOptions(Options options) {
    const bool details = m_options.testFlag(HideNameFilterDetails) != options.testFlag(HideNameFilterDetails);
    m_options = options;
    if (details)
        setNameFilters(m_nameFilters);
    refresh();
}

/*!
    \fn QFileDialog::Options QFileDialog::options() const

    Returns the options of the dialog.
*/

/*!
    \reimp

    Checks the file name field for the file mode: a directory that the user typed is entered instead, missing files
    are refused, and replacing a file is confirmed in AcceptSave mode. When the names are good the dialog emits
    filesSelected() and fileSelected() and closes.
*/
void QFileDialog::accept() {
    QStringList names = typedNames();
    QStringList files;
    if (m_fileMode == Directory) {
        const QString path = names.isEmpty() ? m_directory : resolve(names.first());
        if (!QFileInfo(path).isDir())
            return;
        files.append(QDir::cleanPath(path));
    } else {
        if (names.isEmpty())
            return;
        for (const QString &name : names) {
            QString path = resolve(name);
            const QFileInfo info(path);
            if (info.isDir()) {
                // A directory was typed: enter it.
                setDirectory(path);
                m_fileName->clear();
                return;
            }
            if (m_fileMode == AnyFile) {
                if (m_acceptMode == AcceptSave && !m_defaultSuffix.isEmpty() && info.suffix().isEmpty())
                    path += QLatin1Char('.') + m_defaultSuffix;
                if (!QFileInfo(QFileInfo(path).absolutePath()).isDir())
                    return;
            } else if (!info.exists()) {
                return;
            }
            files.append(path);
        }
        if (m_fileMode == AnyFile && m_acceptMode == AcceptSave && !testOption(DontConfirmOverwrite) &&
            QFileInfo(files.first()).exists()) {
            const QString question = QStringLiteral("%1 already exists.\nDo you want to replace it?")
                                         .arg(QFileInfo(files.first()).fileName());
            if (QMessageBox::warning(this, windowTitle(), question, QMessageBox::Yes | QMessageBox::No,
                                     QMessageBox::No) != QMessageBox::Yes)
                return;
        }
    }
    m_selected = files;
    emit filesSelected(files);
    if (m_fileMode != ExistingFiles)
        emit fileSelected(files.first());
    QDialog::accept();
}

// Reads the directory and lists its directories, then the files that match the filter.
void QFileDialog::refresh() {
    if (!m_list)
        return;
    m_updating = true;
    m_list->clear();
    const QDir dir(m_directory);
    const QFileInfoList entries =
        dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
    const bool dirsOnly = m_fileMode == Directory && testOption(ShowDirsOnly);
    const QStringList patterns = currentPatterns();
    for (const QFileInfo &info : entries)
        if (info.isDir())
            m_list->addItem(info.fileName() + QLatin1Char('/'));
    if (!dirsOnly) {
        for (const QFileInfo &info : entries)
            if (!info.isDir() && QDir::match(patterns, info.fileName()))
                m_list->addItem(info.fileName());
    }
    m_updating = false;
}

void QFileDialog::updateMode() {
    if (!m_buttons)
        return;
    m_list->setSelectionMode(m_fileMode == ExistingFiles ? QAbstractItemView::ExtendedSelection
                                                         : QAbstractItemView::SingleSelection);
    const bool save = m_acceptMode == AcceptSave;
    m_buttons->setStandardButtons((save ? QDialogButtonBox::Save : QDialogButtonBox::Open) | QDialogButtonBox::Cancel);
    QPushButton *acceptButton = m_buttons->button(save ? QDialogButtonBox::Save : QDialogButtonBox::Open);
    if (!m_acceptText.isEmpty())
        acceptButton->setText(m_acceptText);
    else if (m_fileMode == Directory)
        acceptButton->setText(QStringLiteral("Choose"));
    acceptButton->setDefault(true);
    if (!m_rejectText.isEmpty())
        m_buttons->button(QDialogButtonBox::Cancel)->setText(m_rejectText);
    const bool directories = m_fileMode == Directory;
    m_typeLabel->setVisible(!directories);
    m_types->setVisible(!directories);
    if (directories)
        m_fileNameLabel->setText(m_fileNameLabel->text() == QStringLiteral("File name:") ? QStringLiteral("Directory:")
                                                                                         : m_fileNameLabel->text());
}

QStringList QFileDialog::currentPatterns() const { return patternsOf(selectedNameFilter()); }

// The names in the file name field: one name, or several in quotes in ExistingFiles mode.
QStringList QFileDialog::typedNames() const {
    const QString text = m_fileName->text().trimmed();
    QStringList names;
    if (m_fileMode == ExistingFiles && text.contains(QLatin1Char('"'))) {
        const QStringList parts = text.split(QLatin1Char('"'));
        for (qsizetype i = 1; i < parts.size(); i += 2)
            if (!parts.at(i).isEmpty())
                names.append(parts.at(i));
    } else if (!text.isEmpty()) {
        names.append(text);
    }
    return names;
}

// The absolute path for `name`, relative to the directory unless it is absolute; "~" is the home directory.
QString QFileDialog::resolve(const QString &name) const {
    QString path = name;
    if (path == QStringLiteral("~") || path.startsWith(QStringLiteral("~/")))
        path = QDir::homePath() + path.mid(1);
    if (!QDir::isAbsolutePath(path))
        path = withSlash(m_directory) + path;
    return QDir::cleanPath(path);
}

void QFileDialog::itemActivated(QListWidgetItem *item) {
    const QString text = item->text();
    if (text.endsWith(QLatin1Char('/'))) {
        setDirectory(withSlash(m_directory) + text.left(text.size() - 1));
        if (m_fileMode != AnyFile)
            m_fileName->clear();
        return;
    }
    m_fileName->setText(text);
    accept();
}

void QFileDialog::selectionChanged() {
    if (m_updating)
        return;
    QStringList names;
    for (QListWidgetItem *item : m_list->selectedItems()) {
        QString text = item->text();
        const bool dir = text.endsWith(QLatin1Char('/'));
        if (dir)
            text.chop(1);
        if (dir != (m_fileMode == Directory))
            continue;
        names.append(text);
    }
    if (QListWidgetItem *current = m_list->currentItem()) {
        QString text = current->text();
        if (text.endsWith(QLatin1Char('/')))
            text.chop(1);
        emit currentChanged(withSlash(m_directory) + text);
    }
    if (names.isEmpty())
        return;
    if (m_fileMode == ExistingFiles && names.size() > 1)
        m_fileName->setText(QLatin1Char('"') + names.join(QStringLiteral("\" \"")) + QLatin1Char('"'));
    else
        m_fileName->setText(names.first());
}

/*!
    Shows a file dialog for opening one existing file, with the given \a parent, window \a caption, starting directory
    or file \a dir, name filters \a filter and \a options, and waits. Returns the chosen file, or an empty string if
    the user canceled. \a selectedFilter, if given, chooses the starting filter and is set to the chosen one.
*/
QString QFileDialog::getOpenFileName(QWidget *parent, const QString &caption, const QString &dir, const QString &filter,
                                     QString *selectedFilter, Options options) {
    QFileDialog dialog(parent, caption, dir, filter);
    dialog.setOptions(options);
    dialog.setFileMode(ExistingFile);
    if (selectedFilter && !selectedFilter->isEmpty())
        dialog.selectNameFilter(*selectedFilter);
    if (dialog.exec() != Accepted)
        return QString();
    if (selectedFilter)
        *selectedFilter = dialog.selectedNameFilter();
    return dialog.selectedFiles().value(0);
}

/*!
    Shows a file dialog for opening one or more existing files, with the given \a parent, window \a caption, starting
    directory \a dir, name filters \a filter and \a options, and waits. Returns the chosen files, or an empty list if
    the user canceled. \a selectedFilter, if given, chooses the starting filter and is set to the chosen one.
*/
QStringList QFileDialog::getOpenFileNames(QWidget *parent, const QString &caption, const QString &dir,
                                          const QString &filter, QString *selectedFilter, Options options) {
    QFileDialog dialog(parent, caption, dir, filter);
    dialog.setOptions(options);
    dialog.setFileMode(ExistingFiles);
    if (selectedFilter && !selectedFilter->isEmpty())
        dialog.selectNameFilter(*selectedFilter);
    if (dialog.exec() != Accepted)
        return QStringList();
    if (selectedFilter)
        *selectedFilter = dialog.selectedNameFilter();
    return dialog.selectedFiles();
}

/*!
    Shows a file dialog for saving a file, with the given \a parent, window \a caption, starting directory or file
    \a dir, name filters \a filter and \a options, and waits. Returns the chosen file name, or an empty string if the
    user canceled. \a selectedFilter, if given, chooses the starting filter and is set to the chosen one.
*/
QString QFileDialog::getSaveFileName(QWidget *parent, const QString &caption, const QString &dir, const QString &filter,
                                     QString *selectedFilter, Options options) {
    QFileDialog dialog(parent, caption, dir, filter);
    dialog.setOptions(options);
    dialog.setFileMode(AnyFile);
    dialog.setAcceptMode(AcceptSave);
    if (selectedFilter && !selectedFilter->isEmpty())
        dialog.selectNameFilter(*selectedFilter);
    if (dialog.exec() != Accepted)
        return QString();
    if (selectedFilter)
        *selectedFilter = dialog.selectedNameFilter();
    return dialog.selectedFiles().value(0);
}

/*!
    Shows a file dialog for choosing a directory, with the given \a parent, window \a caption, starting directory
    \a dir and \a options, and waits. Returns the chosen directory, or an empty string if the user canceled.
*/
QString QFileDialog::getExistingDirectory(QWidget *parent, const QString &caption, const QString &dir,
                                          Options options) {
    QFileDialog dialog(parent, caption, dir);
    dialog.setOptions(options);
    dialog.setFileMode(Directory);
    if (dialog.exec() != Accepted)
        return QString();
    return dialog.selectedFiles().value(0);
}

#include "moc_qfiledialog.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
