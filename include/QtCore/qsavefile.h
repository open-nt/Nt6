// MIT License - see LICENSE
#pragma once

#include <QtCore/qfile.h>

// Writes to a temporary file and renames it over the target on commit(), so
// readers never see a partial file.
class QSaveFile : public QFileDevice {
    Q_OBJECT
public:
    explicit QSaveFile(const QString &name);
    explicit QSaveFile(QObject *parent = nullptr);
    QSaveFile(const QString &name, QObject *parent);
    ~QSaveFile() override;

    QString fileName() const { return m_target; }
    void setFileName(const QString &name) { m_target = name; }
    bool open(OpenMode mode) override;
    bool commit();
    void cancelWriting();
    void setDirectWriteFallback(bool enabled) { m_directFallback = enabled; }
    bool directWriteFallback() const { return m_directFallback; }

protected:
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    void close() override;

    QString m_target;
    QString m_temp;
    bool m_cancelled = false;
    bool m_direct = false;
    bool m_directFallback = false;
};
