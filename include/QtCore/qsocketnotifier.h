// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>

/*!
    \class QSocketDescriptor
    \brief The QSocketDescriptor class wraps a native socket or file descriptor.

    It is passed to QSocketNotifier::activated() and converts implicitly to
    \c int.

    \differences
    The descriptor type is always \c int; there is no platform-dependent
    \c DescriptorType and no Windows handle support.
    \enddifferences

    \sa QSocketNotifier
*/
class QSocketDescriptor {
public:
    /*!
        Constructs a descriptor wrapping \a descriptor. The default is an
        invalid descriptor.
    */
    constexpr QSocketDescriptor(int descriptor = -1) noexcept : fd(descriptor) {}
    /*!
        Returns the wrapped descriptor value.
    */
    constexpr operator int() const noexcept { return fd; }
    /*!
        Returns \c true if the descriptor is not -1; otherwise returns
        \c false.
    */
    constexpr bool isValid() const noexcept { return fd != -1; }
    /*!
        Returns the wrapped descriptor value.
    */
    constexpr int value() const noexcept { return fd; }

private:
    int fd;
};

class QSocketNotifier : public QObject {
    Q_OBJECT
public:
    /*!
        \enum QSocketNotifier::Type

        This enum describes the conditions a notifier watches for.

        \value Read The descriptor has data to read.
        \value Write The descriptor can be written without blocking.
        \value Exception An exceptional condition, such as out-of-band data, occurred.
    */
    enum Type { Read, Write, Exception };

    QSocketNotifier(qintptr socket, Type type, QObject *parent = nullptr);
    ~QSocketNotifier() override;

    /*!
        Returns the descriptor the notifier watches.
    */
    qintptr socket() const { return m_fd; }
    /*!
        Returns the condition the notifier watches for.
    */
    Type type() const { return m_type; }
    /*!
        Returns \c true if the notifier is enabled; otherwise returns
        \c false. Notifiers are enabled when constructed.

        \sa setEnabled()
    */
    bool isEnabled() const { return m_enabled; }

public slots:
    void setEnabled(bool enable);

signals:
    /*!
        This signal is emitted when the watched condition occurs.
        \a socket is the descriptor and \a activationEvent is the notifier
        type.

        \sa type()
    */
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
