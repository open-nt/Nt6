// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>

class QSocketDescriptor {
public:
    constexpr QSocketDescriptor(int descriptor = -1) noexcept : fd(descriptor) {}
    constexpr operator int() const noexcept { return fd; }
    constexpr bool isValid() const noexcept { return fd != -1; }
    constexpr int value() const noexcept { return fd; }

private:
    int fd;
};

class QSocketNotifier : public QObject {
    Q_OBJECT
public:
    enum Type { Read, Write, Exception };

    QSocketNotifier(qintptr socket, Type type, QObject *parent = nullptr);
    ~QSocketNotifier() override;

    qintptr socket() const { return m_fd; }
    Type type() const { return m_type; }
    bool isEnabled() const { return m_enabled; }

public slots:
    void setEnabled(bool enable);

signals:
    void activated(QSocketDescriptor socket, QSocketNotifier::Type activationEvent) {
        emitSignal(&QSocketNotifier::activated, socket, activationEvent);
    }

protected:
    bool event(QEvent *e) override;

private:
    qintptr m_fd;
    Type m_type;
    bool m_enabled = true;
    int m_id = 0;
};
