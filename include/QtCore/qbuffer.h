// MIT License - see LICENSE
#pragma once

#include <QtCore/qiodevice.h>

class QBuffer : public QIODevice {
    Q_OBJECT
public:
    explicit QBuffer(QObject *parent = nullptr);
    explicit QBuffer(QByteArray *byteArray, QObject *parent = nullptr);
    ~QBuffer() override;

    QByteArray &buffer() { return *m_buf; }
    const QByteArray &buffer() const { return *m_buf; }
    const QByteArray &data() const { return *m_buf; }
    void setBuffer(QByteArray *a);
    void setData(const QByteArray &data);
    void setData(const char *data, qint64 len) { setData(QByteArray(data, len)); }

    bool open(OpenMode mode) override;
    void close() override;
    qint64 pos() const override { return m_pos; }
    qint64 size() const override { return m_buf->size(); }
    bool seek(qint64 off) override;
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
