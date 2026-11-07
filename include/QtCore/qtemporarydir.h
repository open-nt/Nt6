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

    bool isValid() const { return !m_path.isEmpty(); }
    QString errorString() const { return m_error; }
    bool autoRemove() const { return m_autoRemove; }
    void setAutoRemove(bool b) { m_autoRemove = b; }
    bool remove();
    QString path() const { return m_path; }
    QString filePath(const QString &fileName) const;

private:
    void create(const QString &templatePath);

    QString m_path;
    QString m_error;
    bool m_autoRemove = true;
};
