// MIT License - see LICENSE
#include <QtWidgets/qtextbrowser.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtGui/qevent.h>

#include <algorithm>

/*!
    \class QTextBrowser
    \brief The QTextBrowser class shows documents that the user can go back and forward through.

    A text browser is a read-only QTextEdit that loads its text from a source, a file name or a URL, and keeps a history
    of the sources it showed:

    \code
    auto *help = new QTextBrowser(window);
    help->setSearchPaths({"/usr/share/myapp/help"});
    help->setSource(QUrl("index.txt"));
    connect(backButton, &QPushButton::clicked, help, &QTextBrowser::backward);
    \endcode

    A relative source is looked up next to the current one, then in the search paths. Alt+Left and Alt+Right go
    backward and forward.

    \differences
    Text is plain in Nt, so documents are shown as their text: there are no links to click (\c anchorClicked(),
    \c highlighted()), no HTML, Markdown or images, and \c openLinks() and \c openExternalLinks() are recorded only.
    loadResource() reads local files; the fragment of a source is ignored.
    \enddifferences

    \sa QTextEdit
*/

/*!
    Constructs an empty, read-only text browser with the given \a parent.
*/
QTextBrowser::QTextBrowser(QWidget *parent) : QTextEdit(parent) { setReadOnly(true); }

/*!
    Destroys the text browser.
*/
QTextBrowser::~QTextBrowser() = default;

/*!
    Returns the source that is shown, or an empty URL.

    \sa setSource()
*/
QUrl QTextBrowser::source() const { return m_position >= 0 ? m_history[static_cast<std::size_t>(m_position)] : QUrl(); }

/*!
    \fn QStringList QTextBrowser::searchPaths() const

    Returns the directories that relative sources are looked up in.
*/

/*!
    \fn void QTextBrowser::setSearchPaths(const QStringList &paths)

    Sets the directories that relative sources are looked up in to \a paths.
*/

/*!
    Returns the contents of the resource \a name as a QString, or an invalid QVariant if it cannot be read. \a type
    says what the resource is for; Nt reads the text of local files whatever it is. Reimplement it to show documents
    from elsewhere.
*/
QVariant QTextBrowser::loadResource(int type, const QUrl &name) {
    (void)type;
    QString path = name.isLocalFile() ? name.toLocalFile() : name.toString();
    QStringList candidates;
    if (QDir::isAbsolutePath(path)) {
        candidates << path;
    } else {
        const QUrl current = source();
        if (current.isLocalFile() || current.isRelative()) {
            const QString base = current.isLocalFile() ? current.toLocalFile() : current.toString();
            if (!base.isEmpty())
                candidates << QFileInfo(base).absolutePath() + QLatin1Char('/') + path;
        }
        for (const QString &dir : m_searchPaths)
            candidates << dir + QLatin1Char('/') + path;
        candidates << path;
    }
    for (const QString &candidate : candidates) {
        QFile file(candidate);
        if (file.open(QIODevice::ReadOnly))
            return QString::fromUtf8(file.readAll());
    }
    return QVariant();
}

/*!
    \fn bool QTextBrowser::isBackwardAvailable() const

    Returns whether there is a source to go backward to.
*/

/*!
    \fn bool QTextBrowser::isForwardAvailable() const

    Returns whether there is a source to go forward to.
*/

/*!
    Forgets the history but for the source that is shown.
*/
void QTextBrowser::clearHistory() {
    const bool back = isBackwardAvailable(), fwd = isForwardAvailable();
    const QUrl current = source();
    m_history.clear();
    m_position = -1;
    if (!current.isEmpty()) {
        m_history.push_back(current);
        m_position = 0;
    }
    notifyHistory(back, fwd);
}

/*!
    Returns the title of the history entry \a i: a negative \a i is backward, 0 the current source, a positive one
    forward. The title is the file name, as documents have no titles in Nt.
*/
QString QTextBrowser::historyTitle(int i) const {
    const QUrl url = historyUrl(i);
    return url.isEmpty() ? QString() : QFileInfo(url.isLocalFile() ? url.toLocalFile() : url.toString()).fileName();
}

/*!
    Returns the source of the history entry \a i, counted as by historyTitle(), or an empty URL.
*/
QUrl QTextBrowser::historyUrl(int i) const {
    const int at = m_position + i;
    return at >= 0 && at < static_cast<int>(m_history.size()) ? m_history[static_cast<std::size_t>(at)] : QUrl();
}

/*!
    \fn int QTextBrowser::backwardHistoryCount() const

    Returns the number of sources to go backward to.
*/

/*!
    Returns the number of sources to go forward to.
*/
int QTextBrowser::forwardHistoryCount() const {
    return std::max(0, static_cast<int>(m_history.size()) - m_position - 1);
}

/*!
    \fn bool QTextBrowser::openExternalLinks() const

    Returns whether links to other programs are opened; recorded only.
*/

/*!
    \fn void QTextBrowser::setOpenExternalLinks(bool open)

    Sets whether links to other programs are opened to \a open; recorded only.
*/

/*!
    \fn bool QTextBrowser::openLinks() const

    Returns whether clicked links are followed; recorded only.
*/

/*!
    \fn void QTextBrowser::setOpenLinks(bool open)

    Sets whether clicked links are followed to \a open; recorded only.
*/

/*!
    Shows the source \a name and adds it to the history, after the current source; the sources forward of it are
    forgotten. A source that cannot be read shows nothing.

    \sa source(), loadResource()
*/
void QTextBrowser::setSource(const QUrl &name) {
    const bool back = isBackwardAvailable(), fwd = isForwardAvailable();
    // A relative source next to the current one is kept as the file it resolves to, so that the history finds it.
    QUrl url = name;
    if (url.isRelative() && !source().isEmpty()) {
        const QUrl resolved = source().resolved(name);
        if (resolved.isLocalFile() && QFileInfo::exists(resolved.toLocalFile()))
            url = resolved;
    }
    show(url);
    if (m_position + 1 < static_cast<int>(m_history.size()))
        m_history.erase(m_history.begin() + m_position + 1, m_history.end());
    m_history.push_back(url);
    m_position = static_cast<int>(m_history.size()) - 1;
    emit sourceChanged(url);
    notifyHistory(back, fwd);
}

/*!
    Goes back to the source before the current one.
*/
void QTextBrowser::backward() {
    if (!isBackwardAvailable())
        return;
    const bool back = isBackwardAvailable(), fwd = isForwardAvailable();
    --m_position;
    show(source());
    emit sourceChanged(source());
    notifyHistory(back, fwd);
}

/*!
    Goes forward to the source after the current one.
*/
void QTextBrowser::forward() {
    if (!isForwardAvailable())
        return;
    const bool back = isBackwardAvailable(), fwd = isForwardAvailable();
    ++m_position;
    show(source());
    emit sourceChanged(source());
    notifyHistory(back, fwd);
}

/*!
    Goes to the first source of the history.
*/
void QTextBrowser::home() {
    if (!m_history.empty())
        setSource(m_history.front());
}

/*!
    Shows the current source again, read afresh.
*/
void QTextBrowser::reload() {
    if (m_position >= 0)
        show(source());
}

void QTextBrowser::show(const QUrl &url) {
    QUrl file = url;
    file.setFragment(QString());
    const QVariant text = loadResource(1, file);
    setPlainText(text.isValid() ? text.toString() : QString());
}

void QTextBrowser::notifyHistory(bool back, bool fwd) {
    emit historyChanged();
    if (back != isBackwardAvailable())
        emit backwardAvailable(isBackwardAvailable());
    if (fwd != isForwardAvailable())
        emit forwardAvailable(isForwardAvailable());
}

/*!
    \reimp

    Alt+Left goes backward and Alt+Right forward.
*/
void QTextBrowser::keyPressEvent(QKeyEvent *event) {
    if (event->modifiers() == Qt::AltModifier && event->key() == Qt::Key_Left) {
        backward();
        event->accept();
        return;
    }
    if (event->modifiers() == Qt::AltModifier && event->key() == Qt::Key_Right) {
        forward();
        event->accept();
        return;
    }
    QTextEdit::keyPressEvent(event);
}

#include "moc_qtextbrowser.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
