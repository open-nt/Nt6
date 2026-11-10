// MIT License - see LICENSE
#include <QtCore/qfileselector.h>

#include <QtCore/qfileinfo.h>
#include <QtCore/qlocale.h>
#include <QtCore/qsysinfo.h>

/*!
    \class QFileSelector
    \brief The QFileSelector class provides a convenient way of selecting file variants.
    \since 5.2

    A program can ship several variants of a file, for a platform or a language, in directories
    whose names start with a plus sign, next to the file they replace:

    \code
    images/background.png
    images/+linux/background.png
    images/+de_DE/background.png
    \endcode

    select() returns the variant that fits the selectors that are active, or the path it was given
    if no variant does:

    \code
    QFileSelector selector;
    const QString path = selector.select(u"images/background.png"_s);   // images/+linux/background.png
    \endcode

    The selectors, in the order they are tried, are the extra selectors that the program set with
    setExtraSelectors(), the comma-separated names in the environment variable
    \c QT_FILE_SELECTORS, the name of the default locale such as \c de_DE, and the names of the
    platform: \c unix, the kernel such as \c linux, and the distribution such as \c ubuntu.
    Selector directories nest, and a variant in \c{+linux/+de_DE} is preferred to the one in
    \c{+linux}. The file must also exist outside the selector directories.

    Files in \l{The Nt Resource System}{resources} are selected in the same way: give select() a
    path that starts with \c{:/} or a URL with the scheme \c qrc.

    \differences
    The environment variable is read by every call of allSelectors(), not once for the process.
    There is no \c{assets:} scheme, which only Android has.
    \enddifferences

    \sa QLocale, QSysInfo
*/

namespace {

QStringList platformSelectors() {
    QStringList selectors;
#ifndef _WIN32
    selectors.append(QString("unix"));
#endif
    selectors.append(QSysInfo::kernelType());
    const QString product = QSysInfo::productType();
    if (!product.isEmpty() && product != QString("unknown") && !selectors.contains(product))
        selectors.append(product);
    return selectors;
}

// Looks in the selector directories of `directory`, which ends with a slash, the first selector first, and in the
// directories nested in them; then for the file itself. Returns an empty string if the file is nowhere.
QString selectIn(const QString &directory, const QString &fileName, const QStringList &selectors) {
    for (qsizetype i = 0; i < selectors.size(); ++i) {
        const QString nested = directory + QChar(u'+') + selectors.at(i) + QChar(u'/');
        if (!QFileInfo(nested).isDir())
            continue;
        const QString found = selectIn(nested, fileName, selectors.mid(i + 1));
        if (!found.isEmpty())
            return found;
    }
    const QString plain = directory + fileName;
    return QFileInfo::exists(plain) ? plain : QString();
}

QString selected(const QString &filePath, const QStringList &selectors) {
    // Without the file there is nothing to replace.
    if (!QFileInfo::exists(filePath))
        return filePath;
    const qsizetype slash = filePath.lastIndexOf(QChar(u'/'));
    const QString directory = filePath.left(slash + 1), fileName = filePath.mid(slash + 1);
    const QString found = selectIn(directory, fileName, selectors);
    return found.isEmpty() ? filePath : found;
}

} // namespace

/*!
    Constructs a file selector with the given \a parent and no extra selectors.
*/
QFileSelector::QFileSelector(QObject *parent) : QObject(parent) {}

/*!
    Destroys the file selector.
*/
QFileSelector::~QFileSelector() = default;

/*!
    Returns the variant of \a filePath that fits the active selectors best, or \a filePath if it
    has no fitting variant or does not exist.

    \sa allSelectors()
*/
QString QFileSelector::select(const QString &filePath) const { return selected(filePath, allSelectors()); }

/*!
    \overload
    Returns the URL of the variant of \a filePath. Only URLs of local files and of resources, with
    the scheme \c file or \c qrc, have variants; any other URL is returned unchanged.
*/
QUrl QFileSelector::select(const QUrl &filePath) const {
    const bool resource = filePath.scheme() == QString("qrc");
    if (!resource && !filePath.isLocalFile())
        return filePath;
    QUrl out(filePath);
    if (resource) {
        const QString path = selected(QChar(u':') + filePath.path(), allSelectors());
        out.setPath(path.mid(1));
    } else {
        out = QUrl::fromLocalFile(selected(filePath.toLocalFile(), allSelectors()));
        if (filePath.hasFragment())
            out.setFragment(filePath.fragment());
        if (filePath.hasQuery())
            out.setQuery(filePath.query());
    }
    return out;
}

/*!
    Returns the selectors that the program added with setExtraSelectors().
*/
QStringList QFileSelector::extraSelectors() const { return m_extra; }

/*!
    Sets the selectors of the program to \a list. They are tried before all others, the first
    one first.

    \sa extraSelectors(), allSelectors()
*/
void QFileSelector::setExtraSelectors(const QStringList &list) { m_extra = list; }

/*!
    Returns every selector that select() uses, the one with the highest priority first: the extra
    selectors, those of the environment variable \c QT_FILE_SELECTORS, the name of the default
    locale and the platform names.
*/
QStringList QFileSelector::allSelectors() const {
    QStringList all = m_extra;
    const QStringList fromEnvironment =
        qEnvironmentVariable("QT_FILE_SELECTORS").split(QChar(u','), Qt::SkipEmptyParts);
    for (const QString &selector : fromEnvironment)
        all.append(selector);
    all.append(QLocale().name());
    for (const QString &selector : platformSelectors())
        all.append(selector);
    return all;
}

#include "moc_qfileselector.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
