// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qobjectdefs.h>

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

class QCoreApplication;
class QDebug;

namespace QtPrivate {
template <typename T, typename = void> struct IsPermission : std::false_type {};
template <typename T> struct IsPermission<T, std::void_t<typename T::QtPermissionHelper>> : std::true_type {};
} // namespace QtPrivate

class QPermission {
    Q_GADGET
public:
    QPermission() = default;

    template <typename T, std::enable_if_t<QtPrivate::IsPermission<T>::value, bool> = true>
    QPermission(const T &permission) // NOLINT(google-explicit-constructor): implicit, as in Qt
        : m_type(QMetaType::fromType<T>()), m_data(std::make_shared<T>(permission)) {}

    Qt::PermissionStatus status() const { return m_status; }
    QMetaType type() const { return m_type; }

    template <typename T, std::enable_if_t<QtPrivate::IsPermission<T>::value, bool> = true>
    std::optional<T> value() const {
        if (!m_data || m_type != QMetaType::fromType<T>())
            return std::nullopt;
        return *static_cast<const T *>(m_data.get());
    }

private:
    friend class QCoreApplication;
    friend QDebug operator<<(QDebug debug, const QPermission &permission);

    Qt::PermissionStatus m_status = Qt::PermissionStatus::Undetermined;
    QMetaType m_type;
    std::shared_ptr<const void> m_data;
};

QDebug operator<<(QDebug debug, const QPermission &permission);

// The members that every permission class has: construction, copying, moving and swap().
#define NT_PERMISSION_COMMON(ClassName)                                                                                \
public:                                                                                                                \
    using QtPermissionHelper = void;                                                                                   \
    ClassName() noexcept = default;                                                                                    \
    ClassName(const ClassName &) noexcept = default;                                                                   \
    ClassName(ClassName &&) noexcept = default;                                                                        \
    ClassName &operator=(const ClassName &) noexcept = default;                                                        \
    ClassName &operator=(ClassName &&) noexcept = default;                                                             \
    ~ClassName() = default;                                                                                            \
    void swap(ClassName &other) noexcept { std::swap(*this, other); }

class QLocationPermission {
    Q_GADGET
    NT_PERMISSION_COMMON(QLocationPermission)
public:
    enum Accuracy : quint8 { Approximate, Precise };
    Q_ENUM(Accuracy)

    enum Availability : quint8 { WhenInUse, Always };
    Q_ENUM(Availability)

    void setAccuracy(Accuracy accuracy) noexcept { m_accuracy = accuracy; }
    Accuracy accuracy() const noexcept { return m_accuracy; }

    void setAvailability(Availability availability) noexcept { m_availability = availability; }
    Availability availability() const noexcept { return m_availability; }

private:
    Accuracy m_accuracy = Approximate;
    Availability m_availability = WhenInUse;
};

class QCalendarPermission {
    Q_GADGET
    NT_PERMISSION_COMMON(QCalendarPermission)
public:
    enum AccessMode : quint8 { ReadOnly, ReadWrite };
    Q_ENUM(AccessMode)

    void setAccessMode(AccessMode mode) noexcept { m_mode = mode; }
    AccessMode accessMode() const noexcept { return m_mode; }

private:
    AccessMode m_mode = ReadOnly;
};

class QContactsPermission {
    Q_GADGET
    NT_PERMISSION_COMMON(QContactsPermission)
public:
    enum AccessMode : quint8 { ReadOnly, ReadWrite };
    Q_ENUM(AccessMode)

    void setAccessMode(AccessMode mode) noexcept { m_mode = mode; }
    AccessMode accessMode() const noexcept { return m_mode; }

private:
    AccessMode m_mode = ReadOnly;
};

class QCameraPermission {
    Q_GADGET
    NT_PERMISSION_COMMON(QCameraPermission)
};

class QMicrophonePermission {
    Q_GADGET
    NT_PERMISSION_COMMON(QMicrophonePermission)
};

class QBluetoothPermission {
    Q_GADGET
    NT_PERMISSION_COMMON(QBluetoothPermission)
public:
    enum CommunicationMode : quint8 {
        Access = 0x01,
        Advertise = 0x02,
        Default = Access | Advertise,
    };
    Q_ENUM(CommunicationMode)
    Q_DECLARE_FLAGS(CommunicationModes, CommunicationMode)
    Q_FLAG(CommunicationModes)

    void setCommunicationModes(CommunicationModes modes) { m_modes = modes ? modes : CommunicationModes(Default); }
    CommunicationModes communicationModes() const noexcept { return m_modes; }

private:
    CommunicationModes m_modes = Default;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QBluetoothPermission::CommunicationModes)

#undef NT_PERMISSION_COMMON
