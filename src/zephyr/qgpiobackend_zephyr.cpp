// MIT License - see LICENSE
//
// QGpioLine on Zephyr. The pin is configured with gpio_pin_configure() and watched with an
// interrupt on both edges. Nt may not be called from an interrupt, so the handler only submits
// a work item, and the work item, in the thread of the system work queue, writes an eventfd that
// the event loop of the QGpioLine watches with a QSocketNotifier. A pin whose controller has no
// interrupt, such as GPIO7 to GPIO12 for the Cortex-M7 of the i.MX RT1170, is polled.
//
// Lines are named "<controller>:<pin>", where the controller is the name device_get_binding()
// takes (gpio@40c64000) or a node label of it in the devicetree (gpio9), or by the alias or node
// label of an LED or key of the board (led0, sw0, green_led, user_button), which carries its pin and
// flags. The names are gathered from the devicetree at build time.
#include "qgpiobackend_p.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util_macro.h>

#include <cerrno>
#include <cstring>
#include <sys/eventfd.h>
#include <unistd.h>

namespace QtPrivate {

namespace {

// A name of the devicetree that a line can be given.
struct GpioName {
    const char *name;   // a node label or alias
    const char *device; // the name that device_get_binding() takes
    int pin;            // -1 for a controller, whose pin follows the colon
    gpio_flags_t flags; // GPIO_ACTIVE_LOW and the pull of an LED or key
};

constexpr gpio_flags_t kNamedFlags = GPIO_ACTIVE_LOW | GPIO_PULL_UP | GPIO_PULL_DOWN;

// Every node label of every GPIO controller.
#define NT_GPIO_CONTROLLER_LABEL(label, node) {STRINGIFY(label), DEVICE_DT_NAME(node), -1, 0},
#define NT_GPIO_CONTROLLER(node)                                                                                       \
    COND_CODE_1(DT_NODE_HAS_PROP(node, gpio_controller),                                                               \
                (DT_FOREACH_NODELABEL_VARGS(node, NT_GPIO_CONTROLLER_LABEL, node)), ())

// Every node label of every LED and key, with its pin.
#define NT_GPIO_PIN_ENTRY(name, node)                                                                                  \
    {name, DEVICE_DT_NAME(DT_GPIO_CTLR(node, gpios)), DT_GPIO_PIN(node, gpios),                                        \
     static_cast<gpio_flags_t>(DT_GPIO_FLAGS(node, gpios)) & kNamedFlags},
#define NT_GPIO_PIN_LABEL(label, node) NT_GPIO_PIN_ENTRY(STRINGIFY(label), node)
#define NT_GPIO_PIN_NODE(node)                                                                                         \
    COND_CODE_1(DT_NODE_HAS_PROP(node, gpios), (DT_FOREACH_NODELABEL_VARGS(node, NT_GPIO_PIN_LABEL, node)), ())
#define NT_GPIO_PIN_PARENT(node) DT_FOREACH_CHILD_STATUS_OKAY(node, NT_GPIO_PIN_NODE)

// The usual aliases of LEDs and keys.
#define NT_GPIO_ALIAS(alias)                                                                                           \
    COND_CODE_1(DT_NODE_HAS_PROP(DT_ALIAS(alias), gpios), (NT_GPIO_PIN_ENTRY(STRINGIFY(alias), DT_ALIAS(alias))), ())

// The devicetree's iteration macros pass empty variadic arguments, which -Wpedantic warns about while
// preprocessing, where #pragma GCC diagnostic has no effect; CMakeLists.txt turns it off for this file.
const GpioName kGpioNames[] = {
    DT_FOREACH_STATUS_OKAY_NODE(NT_GPIO_CONTROLLER) DT_FOREACH_STATUS_OKAY(gpio_leds, NT_GPIO_PIN_PARENT)
        DT_FOREACH_STATUS_OKAY(gpio_keys, NT_GPIO_PIN_PARENT) NT_GPIO_ALIAS(led0) NT_GPIO_ALIAS(led1)
            NT_GPIO_ALIAS(led2) NT_GPIO_ALIAS(led3) NT_GPIO_ALIAS(sw0) NT_GPIO_ALIAS(sw1) NT_GPIO_ALIAS(sw2)
                NT_GPIO_ALIAS(sw3){nullptr, nullptr, -1, 0},
};

const GpioName *findGpioName(const QByteArray &name, bool controller) {
    for (const GpioName &entry : kGpioNames) {
        if (entry.name && (entry.pin < 0) == controller && name == entry.name)
            return &entry;
    }
    return nullptr;
}

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
        const QByteArray fullName = config.name.toUtf8();
        const qsizetype colon = fullName.lastIndexOf(':');
        QByteArray deviceName;
        uint pin = 0;
        gpio_flags_t named = 0;
        if (colon < 0) {
            const GpioName *entry = findGpioName(fullName, false);
            if (!entry) {
                failure.error = QGpioLine::DeviceNotFoundError;
                failure.message =
                    QStringLiteral("%1 is neither controller:pin nor an LED or key of the devicetree").arg(config.name);
                return false;
            }
            deviceName = entry->device;
            pin = uint(entry->pin);
            named = entry->flags;
        } else {
            bool ok = false;
            pin = fullName.mid(colon + 1).toUInt(&ok);
            if (colon == 0 || !ok || pin >= 32) {
                failure.error = QGpioLine::DeviceNotFoundError;
                failure.message = QStringLiteral("%1 is not controller:pin").arg(config.name);
                return false;
            }
            deviceName = fullName.left(colon);
            if (const GpioName *entry = findGpioName(deviceName, true))
                deviceName = entry->device;
        }
        m_device = device_get_binding(deviceName.constData());
        if (!m_device || !device_is_ready(m_device)) {
            failure.error = QGpioLine::DeviceNotFoundError;
            failure.message = QStringLiteral("No GPIO device is called %1").arg(QString::fromUtf8(deviceName));
            return false;
        }
        m_pin = static_cast<gpio_pin_t>(pin);

        // An LED or key is active low if the devicetree says so; setActiveLow() inverts it once more.
        const bool activeLow = ((named & GPIO_ACTIVE_LOW) != 0) != config.activeLow;
        gpio_flags_t flags = activeLow ? GPIO_ACTIVE_LOW : GPIO_ACTIVE_HIGH;
        if (config.bias == QGpioLine::PullUp)
            flags |= GPIO_PULL_UP;
        else if (config.bias == QGpioLine::PullDown)
            flags |= GPIO_PULL_DOWN;
        else if (config.bias == QGpioLine::AsIs)
            flags |= named & (GPIO_PULL_UP | GPIO_PULL_DOWN);
        const bool input = config.direction == QGpioLine::Input;
        if (input)
            flags |= GPIO_INPUT;
        else
            flags |= config.initialValue ? GPIO_OUTPUT_ACTIVE : GPIO_OUTPUT_INACTIVE;
        const int result = gpio_pin_configure(m_device, m_pin, flags);
        if (result < 0) {
            setGpioFailure(failure, -result,
                           QStringLiteral("Cannot configure pin %1 of %2").arg(pin).arg(QString::fromUtf8(deviceName)));
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
