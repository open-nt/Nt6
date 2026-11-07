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

    /*!
        Returns the name of the file that will be replaced by commit().

        \sa setFileName()
    */
    QString fileName() const { return m_target; }
    /*!
        Sets the \a name of the file to write. Does nothing if the file is
        open.

        \sa fileName()
    */
    void setFileName(const QString &name) { if (!isOpen()) { m_target = name; m_name = name; } }
    bool open(OpenMode mode) override;
    bool commit();
    void cancelWriting();
    /*!
        Sets whether open() may fall back to writing directly to the target
        when the temporary file cannot be created. If \a enabled is \c true,
        the target is truncated and written in place, so the write is no
        longer atomic. The default is \c false.

        \sa directWriteFallback()
    */
    void setDirectWriteFallback(bool enabled) { m_directFallback = enabled; }
    /*!
        Returns \c true if open() may write directly to the target when the
        temporary file cannot be created; otherwise returns \c false.

        \sa setDirectWriteFallback()
    */
    bool directWriteFallback() const { return m_directFallback; }

protected:
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    void close() override;

    QString m_target;
    QString m_temp;
    bool m_writeError = false;
    bool m_direct = false;
    bool m_directFallback = false;
};
