// MIT License - see LICENSE
#pragma once

#include <QtCore/qdir.h>
#include <QtCore/qstringlist.h>
#include <QtWidgets/qdialog.h>

class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QToolButton;

class QFileDialog : public QDialog {
    Q_OBJECT
    Q_PROPERTY(ViewMode viewMode READ viewMode WRITE setViewMode)
    Q_PROPERTY(FileMode fileMode READ fileMode WRITE setFileMode)
    Q_PROPERTY(AcceptMode acceptMode READ acceptMode WRITE setAcceptMode)
    Q_PROPERTY(QString defaultSuffix READ defaultSuffix WRITE setDefaultSuffix)
public:
    enum ViewMode { Detail, List };
    Q_ENUM(ViewMode)
    enum FileMode { AnyFile, ExistingFile, Directory, ExistingFiles };
    Q_ENUM(FileMode)
    enum AcceptMode { AcceptOpen, AcceptSave };
    Q_ENUM(AcceptMode)
    enum DialogLabel { LookIn, FileName, FileType, Accept, Reject };
    Q_ENUM(DialogLabel)
    enum Option {
        ShowDirsOnly = 0x00000001,
        DontResolveSymlinks = 0x00000002,
        DontConfirmOverwrite = 0x00000004,
        DontUseNativeDialog = 0x00000008,
        ReadOnly = 0x00000010,
        HideNameFilterDetails = 0x00000020,
        DontUseCustomDirectoryIcons = 0x00000040
    };
    Q_DECLARE_FLAGS(Options, Option)

    QFileDialog(QWidget *parent, Qt::WindowFlags flags);
    explicit QFileDialog(QWidget *parent = nullptr, const QString &caption = QString(),
                         const QString &directory = QString(), const QString &filter = QString());
    ~QFileDialog() override;

    void setDirectory(const QString &directory);
    void setDirectory(const QDir &directory) { setDirectory(directory.absolutePath()); }
    QDir directory() const { return QDir(m_directory); }
    void selectFile(const QString &filename);
    QStringList selectedFiles() const;
    void setNameFilter(const QString &filter);
    void setNameFilters(const QStringList &filters);
    QStringList nameFilters() const { return m_nameFilters; }
    void selectNameFilter(const QString &filter);
    QString selectedNameFilter() const;
    void setViewMode(ViewMode mode) { m_viewMode = mode; }
    ViewMode viewMode() const { return m_viewMode; }
    void setFileMode(FileMode mode);
    FileMode fileMode() const { return m_fileMode; }
    void setAcceptMode(AcceptMode mode);
    AcceptMode acceptMode() const { return m_acceptMode; }
    void setDefaultSuffix(const QString &suffix);
    QString defaultSuffix() const { return m_defaultSuffix; }
    void setLabelText(DialogLabel label, const QString &text);
    QString labelText(DialogLabel label) const;
    void setOption(Option option, bool on = true);
    bool testOption(Option option) const { return m_options.testFlag(option); }
    void setOptions(Options options);
    Options options() const { return m_options; }

    static QString getOpenFileName(QWidget *parent = nullptr, const QString &caption = QString(),
                                   const QString &dir = QString(), const QString &filter = QString(),
                                   QString *selectedFilter = nullptr, Options options = Options());
    static QStringList getOpenFileNames(QWidget *parent = nullptr, const QString &caption = QString(),
                                        const QString &dir = QString(), const QString &filter = QString(),
                                        QString *selectedFilter = nullptr, Options options = Options());
    static QString getSaveFileName(QWidget *parent = nullptr, const QString &caption = QString(),
                                   const QString &dir = QString(), const QString &filter = QString(),
                                   QString *selectedFilter = nullptr, Options options = Options());
    static QString getExistingDirectory(QWidget *parent = nullptr, const QString &caption = QString(),
                                        const QString &dir = QString(), Options options = ShowDirsOnly);

public slots:
    void accept() override;

signals:
    /*!
        This signal is emitted when the user moves to the file or directory at \a path.
    */
    void currentChanged(const QString &path) { emitSignal(&QFileDialog::currentChanged, path); }
    /*!
        This signal is emitted when the dialog shows the \a directory.
    */
    void directoryEntered(const QString &directory) { emitSignal(&QFileDialog::directoryEntered, directory); }
    /*!
        This signal is emitted when the dialog is accepted with one \a file, in all the modes but ExistingFiles.
    */
    void fileSelected(const QString &file) { emitSignal(&QFileDialog::fileSelected, file); }
    /*!
        This signal is emitted when the dialog is accepted, with the \a selected files.
    */
    void filesSelected(const QStringList &selected) { emitSignal(&QFileDialog::filesSelected, selected); }
    /*!
        This signal is emitted when the user chooses the name \a filter.
    */
    void filterSelected(const QString &filter) { emitSignal(&QFileDialog::filterSelected, filter); }

private:
    void init(const QString &caption, const QString &directory, const QString &filter);
    void refresh();
    void updateMode();
    QStringList currentPatterns() const;
    QStringList typedNames() const;
    QString resolve(const QString &name) const;
    void itemActivated(QListWidgetItem *item);
    void selectionChanged();

    QLabel *m_lookInLabel = nullptr;
    QLineEdit *m_lookIn = nullptr;
    QToolButton *m_up = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_fileNameLabel = nullptr;
    QLineEdit *m_fileName = nullptr;
    QLabel *m_typeLabel = nullptr;
    QComboBox *m_types = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
    QString m_directory;
    QStringList m_nameFilters;
    QStringList m_selected;
    QString m_defaultSuffix;
    QString m_acceptText;
    QString m_rejectText;
    ViewMode m_viewMode = Detail;
    FileMode m_fileMode = AnyFile;
    AcceptMode m_acceptMode = AcceptOpen;
    Options m_options;
    bool m_updating = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QFileDialog::Options)
