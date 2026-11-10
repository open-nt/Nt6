// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

class QLocale;

/*!
    \class QTranslator
    \brief The QTranslator class provides internationalization support for text output.

    A QTranslator holds a set of translations loaded from a \c .qm file, as
    produced by Qt's \c lrelease tool. Install it on the application with
    QCoreApplication::installTranslator(); tr() and
    QCoreApplication::translate() then return its translations:

    \code
    QDir::setSearchPaths(u"i18n"_s, {QCoreApplication::applicationDirPath() + u"/translations"_s});
    QTranslator translator;
    if (translator.load(QLocale(), u"myapp"_s, u"_"_s, u"i18n:"_s))
        QCoreApplication::installTranslator(&translator);
    \endcode

    Nt has no resource system, so ship the \c .qm files with the program, as
    \l{Resource Files} describes.

    Plural forms are selected with the numerus rules stored in the file, and
    \c{%n} in the result is replaced by the count.

    \differences
    The files listed as dependencies of a \c .qm file are not loaded, and the
    obsolete message tags of very old files are not understood. There is no
    resource system, so file names starting with \c{:} are ordinary paths;
    use a QDir search path prefix instead.
    \enddifferences

    \sa QCoreApplication::installTranslator()
*/
class QTranslator : public QObject {
    Q_OBJECT
public:
    explicit QTranslator(QObject *parent = nullptr);
    ~QTranslator() override;

    virtual QString translate(const char *context, const char *sourceText, const char *disambiguation = nullptr, int n = -1) const;
    virtual bool isEmpty() const;
    QString language() const;
    QString filePath() const;

    bool load(const QString &filename, const QString &directory = QString(), const QString &search_delimiters = QString(),
              const QString &suffix = QString());
    bool load(const QLocale &locale, const QString &filename, const QString &prefix = QString(), const QString &directory = QString(),
              const QString &suffix = QString());
    bool load(const uchar *data, int len, const QString &directory = QString());

private:
    bool loadData(QByteArray data, const QString &path);
    bool tryFile(const QString &path);
    QByteArray m_data;
    qsizetype m_hashes = -1, m_hashesSize = 0;
    qsizetype m_messages = -1, m_messagesSize = 0;
    QByteArray m_numerusRules;
    QString m_language;
    QString m_filePath;
};
