// MIT License - see LICENSE
#pragma once

#include <QtCore/qfile.h>

class QTemporaryFile : public QFile {
    Q_OBJECT
public:
    QTemporaryFile();
    explicit QTemporaryFile(const QString &templateName);
    explicit QTemporaryFile(QObject *parent);
    QTemporaryFile(const QString &templateName, QObject *parent);
    ~QTemporaryFile() override;

    /*!
        Returns \c true if the temporary file is removed when the object is
        destroyed; otherwise returns \c false. The default is \c true.

        \sa setAutoRemove()
    */
    bool autoRemove() const { return m_autoRemove; }
    /*!
        Sets whether the file is removed when the object is destroyed to
        \a b.

        \sa autoRemove()
    */
    void setAutoRemove(bool b) { m_autoRemove = b; }
    /*!
        \overload

        Creates the file from the template and opens it for reading and
        writing. Returns \c true on success; otherwise returns \c false.
    */
    bool open() { return open(QIODevice::ReadWrite); }
    /*!
        Returns the file name template, which is \c{qt_temp.XXXXXX} in
        QDir::tempPath() by default.

        \sa setFileTemplate()
    */
    QString fileTemplate() const { return m_template; }
    /*!
        Sets the file name template to \a name. If the file is already
        created, the template has no effect.

        \sa fileTemplate()
    */
    void setFileTemplate(const QString &name) { m_template = name; }
    /*!
        Returns the name of the temporary file, or an empty string if it has
        not been created yet.

        \sa QFileDevice::fileName()
    */
    QString fileName() const { return m_name; }

protected:
    bool open(OpenMode mode) override;

private:
    QString m_template;
    bool m_autoRemove = true;
};
