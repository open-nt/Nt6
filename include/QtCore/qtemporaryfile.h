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

    bool autoRemove() const { return m_autoRemove; }
    void setAutoRemove(bool b) { m_autoRemove = b; }
    bool open() { return open(QIODevice::ReadWrite); }
    QString fileTemplate() const { return m_template; }
    void setFileTemplate(const QString &name) { m_template = name; }
    QString fileName() const { return m_name; }

protected:
    bool open(OpenMode mode) override;

private:
    QString m_template;
    bool m_autoRemove = true;
};
