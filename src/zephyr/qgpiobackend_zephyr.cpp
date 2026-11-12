// MIT License - see LICENSE
//
// QGpioLine on Zephyr. The pin is configured with gpio_pin_configure() and watched with an
// interrupt on both edges. Nt may not be called from an interrupt, so the handler only submits
// a work item, and the work item, in the thread of the system work queue, writes an eventfd that
// the event loop of the QGpioLine watches with a QSocketNotifier.
#include "qgpiobackend_p.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include <cerrno>
#include <sys/eventfd.h>
#include <unistd.h>

namespace QtPrivate {

namespace {

// Kept apart from the backend so that CONTAINER_OF() sees a plain struct.
struct GpioHooks {
    gpio_callback callback;
    k_work work;
    int fd;
};

void onWork(k_work *work) {
    GpioHooks *hooks = CONTAINER_OF(work, GpioHooks, work);
    eventfd_write(hooks->fd, 1);
}

void onInterrupt(const device *, gpio_callback *callback, gpio_port_pins_t) {
    GpioHooks *hooks = CONTAINER_OF(callback, GpioHooks, callback);
    k_work_submit(&hooks->work);
}

class ZephyrGpioBackend final : public GpioBackend {
public:
    ZephyrGpioBackend() {
        m_hooks.fd = -1;
        k_work_init(&m_hooks.work, onWork);
    }
    ~ZephyrGpioBackend() override {
        if (m_interrupt) {
            gpio_pin_interrupt_configure(m_device, m_pin, GPIO_INT_DISABLE);
            gpio_remove_callback(m_device, &m_hooks.callback);
        }
        k_work_sync sync;
        k_work_cancel_sync(&m_hooks.work, &sync);
        if (m_hooks.fd >= 0)
            ::close(m_hooks.fd);
    }
    ZephyrGpioBackend(const ZephyrGpioBackend &) = delete;
    ZephyrGpioBackend &operator=(const ZephyrGpioBackend &) = delete;

    bool open(const GpioConfig &config, GpioFailure &failure) override {
        const qsizetype colon = config.name.lastIndexOf(QLatin1Char(':'));
        bool ok = false;
        const uint pin = colon > 0 ? config.name.mid(colon + 1).toUInt(&ok) : 0;
        if (!ok || pin >= 32) {
            failure.error = QGpioLine::DeviceNotFoundError;
            failure.message =
                QStringLiteral("%1 is not device:pin; line names are not looked up on Zephyr").arg(config.name);
            return false;
        }
        const QByteArray deviceName = config.name.left(colon).toUtf8();
        m_device = device_get_binding(deviceName.constData());
        if (!m_device || !device_is_ready(m_device)) {
            failure.error = QGpioLine::DeviceNotFoundError;
            failure.message = QStringLiteral("No GPIO device is called %1").arg(QString::fromUtf8(deviceName));
            return false;
        }
        m_pin = static_cast<gpio_pin_t>(pin);

        gpio_flags_t flags = config.activeLow ? GPIO_ACTIVE_LOW : GPIO_ACTIVE_HIGH;
        if (config.bias == QGpioLine::PullUp)
            flags |= GPIO_PULL_UP;
        else if (config.bias == QGpioLine::PullDown)
            flags |= GPIO_PULL_DOWN;
        const bool input = config.direction == QGpioLine::Input;
        if (input)
            flags |= GPIO_INPUT;
        else
            flags |= config.initialValue ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE;
        const int result = gpio_pin_configure(m_device, m_pin, flags);
        if (result < 0) {
            setGpioFailure(failure, -result, QStringLiteral("Cannot configure pin %1 of %2").arg(pin).arg(QString::fromUtf8(deviceName)));
            return false;
        }
        if (input)
            watchEdges();
        return true;
    }

    int eventDescriptor() const override { return m_interrupt ? m_hooks.fd : -1; }

    bool readEvents(std::vector<bool> &levels, GpioFailure &failure) override {
        eventfd_t count = 0;
        eventfd_read(m_hooks.fd, &count); // fails with EAGAIN when there was nothing, which is harmless
        bool value = false;
        if (!readValue(value, failure))
            return false;
        levels.push_back(value);
        return true;
    }

    bool readValue(bool &value, GpioFailure &failure) override {
        const int result = gpio_pin_get(m_device, m_pin);
        if (result < 0) {
            setGpioFailure(failure, -result, QStringLiteral("Cannot read the GPIO pin"));
            return false;
        }
        value = result != 0;
        return true;
    }

    bool writeValue(bool value, GpioFailure &failure) override {
        const int result = gpio_pin_set(m_device, m_pin, value ? 1 : 0);
        if (result < 0) {
            setGpioFailure(failure, -result, QStringLiteral("Cannot set the GPIO pin"));
            return false;
        }
        return true;
    }

private:
    // Turns on the interrupt of the pin; without one the line is polled.
    void watchEdges() {
        m_hooks.fd = eventfd(0, EFD_NONBLOCK);
        if (m_hooks.fd < 0)
            return;
        gpio_init_callback(&m_hooks.callback, onInterrupt, static_cast<gpio_port_pins_t>(1U << m_pin));
        if (gpio_add_callback(m_device, &m_hooks.callback) < 0)
            return;
        if (gpio_pin_interrupt_configure(m_device, m_pin, GPIO_INT_EDGE_BOTH) < 0) {
            gpio_remove_callback(m_device, &m_hooks.callback);
            return;
        }
        m_interrupt = true;
    }

    GpioHooks m_hooks{};
    const device *m_device = nullptr;
    gpio_pin_t m_pin = 0;
    bool m_interrupt = false;
};

} // namespace

std::unique_ptr<GpioBackend> createPlatformGpioBackend() { return std::make_unique<ZephyrGpioBackend>(); }

} // namespace QtPrivate
