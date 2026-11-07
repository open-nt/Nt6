// MIT License - see LICENSE
#pragma once

#include <QtCore/qiodevice.h>

class QBuffer : public QIODevice {
    Q_OBJECT
public:
    explicit QBuffer(QObject *parent = nullptr);
    explicit QBuffer(QByteArray *byteArray, QObject *parent = nullptr);
    ~QBuffer() override;

    /*!
        Returns a reference to the buffer's byte array, which can be modified.

        \sa data(), setBuffer()
    */
    QByteArray &buffer() { return *m_buf; }
    /*!
        \overload

        Returns a const reference to the buffer's byte array.
    */
    const QByteArray &buffer() const { return *m_buf; }
    /*!
        Returns the data contained in the buffer.

        \sa setData(), buffer()
    */
    const QByteArray &data() const { return *m_buf; }
    void setBuffer(QByteArray *a);
    void setData(const QByteArray &data);
    /*!
        \overload

        Sets the contents of the buffer to the first \a len bytes of \a data.
        Does nothing if the buffer is open.
    */
    void setData(const char *data, qint64 len) { setData(QByteArray(data, len)); }

    bool open(OpenMode mode) override;
    void close() override;
    /*!
        \reimp

        Returns the current position in the data.
    */
    qint64 pos() const override { return m_pos; }
    /*!
        \reimp

        Returns the size of the data in bytes.
    */
    qint64 size() const override { return m_buf->size(); }
    bool seek(qint64 off) override;
    /*!
        \reimp

        Returns \c true if the position is at or past the end of the data.
    */
    bool atEnd() const override { return m_pos >= m_buf->size(); }
    qint64 bytesAvailable() const override;
    bool canReadLine() const override;

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    QByteArray m_internal;
    QByteArray *m_buf;
    qint64 m_pos = 0;
};
