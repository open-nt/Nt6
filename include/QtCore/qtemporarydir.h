// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>

class QTemporaryDir {
public:
    QTemporaryDir();
    explicit QTemporaryDir(const QString &templatePath);
    QTemporaryDir(QTemporaryDir &&other) noexcept;
    QTemporaryDir &operator=(QTemporaryDir &&other) noexcept;
    ~QTemporaryDir();
    QTemporaryDir(const QTemporaryDir &) = delete;
    QTemporaryDir &operator=(const QTemporaryDir &) = delete;

    /*!
        Returns \c true if the temporary directory was created successfully;
        otherwise returns \c false. The directory is also invalid after being
        moved from.

        \sa errorString(), path()
    */
    bool isValid() const { return !m_path.isEmpty(); }
    /*!
        Returns a description of the error that prevented the directory from
        being created, or an empty string if there was none.

        \sa isValid()
    */
    QString errorString() const { return m_error; }
    /*!
        Returns \c true if the directory is removed when the object is
        destroyed; otherwise returns \c false. The default is \c true.

        \sa setAutoRemove(), remove()
    */
    bool autoRemove() const { return m_autoRemove; }
    /*!
        Sets whether the directory is removed when the object is destroyed to
        \a b.

        \sa autoRemove()
    */
    void setAutoRemove(bool b) { m_autoRemove = b; }
    bool remove();
    /*!
        Returns the absolute path of the temporary directory, or an empty
        string if the directory is not valid.

        \sa filePath(), isValid()
    */
    QString path() const { return m_path; }
    QString filePath(const QString &fileName) const;

private:
    void create(const QString &templatePath);

    QString m_path;
    QString m_error;
    bool m_autoRemove = true;
};
