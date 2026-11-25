// MIT License - see LICENSE
//
// QWatchdog on the watchdog drivers of Zephyr (CONFIG_WATCHDOG), with the reset cause of hwinfo (CONFIG_HWINFO), and
// QFirmwareUpdate on MCUboot: the image goes into slot1_partition through flash_img (CONFIG_IMG_MANAGER) and is
// booted with boot_request_upgrade() (CONFIG_MCUBOOT_IMG_MANAGER) and sys_reboot() (CONFIG_REBOOT). Each part works
// when its option is set and fails with Unsupported when it is not.
#include "qdevice_p.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#ifdef CONFIG_WATCHDOG
#include <zephyr/drivers/watchdog.h>
#endif
#ifdef CONFIG_HWINFO
#include <zephyr/drivers/hwinfo.h>
#endif
#ifdef CONFIG_FLASH_MAP
#include <zephyr/storage/flash_map.h>
#endif
#ifdef CONFIG_IMG_MANAGER
#include <zephyr/dfu/flash_img.h>
#endif
#ifdef CONFIG_MCUBOOT_IMG_MANAGER
#include <zephyr/dfu/mcuboot.h>
#endif
#ifdef CONFIG_REBOOT
#include <zephyr/sys/reboot.h>
#endif

#include <cerrno>

namespace QtDevicePrivate {

namespace {

bool notConfigured(const char *option, Failure &failure) {
    setFailure(failure, Error::Unsupported, QStringLiteral("Not available without ") + QString::fromLatin1(option));
    return false;
}

#ifdef CONFIG_WATCHDOG
const device *watchdogDevice(const QString &name) {
    if (name.isEmpty() || name == QStringLiteral("watchdog0")) {
#if DT_NODE_HAS_STATUS_OKAY(DT_ALIAS(watchdog0))
        return device_get_binding(DEVICE_DT_NAME(DT_ALIAS(watchdog0)));
#else
        return nullptr;
#endif
    }
    return device_get_binding(name.toUtf8().constData());
}
#endif

class ZephyrWatchdogBackend final : public WatchdogBackend {
public:
    bool start(WatchdogConfig &config, Failure &failure) override {
#ifdef CONFIG_WATCHDOG
        m_device = watchdogDevice(config.name);
        if (!m_device || !device_is_ready(m_device)) {
            setFailure(failure, Error::DeviceNotFound,
                       QStringLiteral("No watchdog %1")
                           .arg(config.name.isEmpty() ? QStringLiteral("watchdog0") : config.name));
            return false;
        }
        wdt_timeout_cfg timeout = {};
        timeout.window.min = 0;
        timeout.window.max = static_cast<uint32_t>(config.timeout);
        timeout.callback = nullptr;
        timeout.flags = WDT_FLAG_RESET_SOC;
        m_channel = wdt_install_timeout(m_device, &timeout);
        if (m_channel < 0) {
            setFailure(failure, m_channel, QStringLiteral("Installing the watchdog timeout"));
            return false;
        }
        uint8_t options = 0;
        if (config.pauseInDebugger)
            options |= WDT_OPT_PAUSE_HALTED_BY_DBG;
        if (config.pauseWhileSleeping)
            options |= WDT_OPT_PAUSE_IN_SLEEP;
        const int rc = wdt_setup(m_device, options);
        if (rc < 0) {
            setFailure(failure, rc, QStringLiteral("Starting the watchdog"));
            return false;
        }
        return true;
#else
        Q_UNUSED(config);
        return notConfigured("CONFIG_WATCHDOG", failure);
#endif
    }

    bool feed(Failure &failure) override {
#ifdef CONFIG_WATCHDOG
        const int rc = wdt_feed(m_device, m_channel);
        if (rc < 0) {
            setFailure(failure, rc, QStringLiteral("Feeding the watchdog"));
            return false;
        }
        return true;
#else
        return notConfigured("CONFIG_WATCHDOG", failure);
#endif
    }

    bool stop(Failure &failure) override {
#ifdef CONFIG_WATCHDOG
        const int rc = wdt_disable(m_device);
        if (rc < 0) {
            setFailure(failure, rc == -EPERM ? -ENOTSUP : rc, QStringLiteral("Stopping the watchdog"));
            return false;
        }
        return true;
#else
        return notConfigured("CONFIG_WATCHDOG", failure);
#endif
    }

    bool causedLastReset(const QString &) override {
#ifdef CONFIG_HWINFO
        uint32_t cause = 0;
        return hwinfo_get_reset_cause(&cause) == 0 && (cause & RESET_WATCHDOG);
#else
        return false;
#endif
    }

private:
#ifdef CONFIG_WATCHDOG
    const device *m_device = nullptr;
    int m_channel = -1;
#endif
};

#ifdef CONFIG_FLASH_MAP
#if PARTITION_EXISTS(slot1_partition)
#define NT_HAVE_SLOTS 1
// The first bytes of a slot, where MCUboot puts the header of its image.
QByteArray readHeader(uint8_t area) {
    const flash_area *fa = nullptr;
    if (flash_area_open(area, &fa) != 0)
        return QByteArray();
    QByteArray header(kImageHeaderSize, '\0');
    const int rc = flash_area_read(fa, 0, header.data(), static_cast<size_t>(header.size()));
    flash_area_close(fa);
    return rc == 0 ? header : QByteArray();
}
#endif
#endif

class ZephyrFirmwareBackend final : public FirmwareBackend {
public:
    bool begin(const QString &, qint64 size, Failure &failure) override {
#if defined(CONFIG_IMG_MANAGER) && defined(NT_HAVE_SLOTS)
        const uint8_t area = PARTITION_ID(slot1_partition);
        const flash_area *fa = nullptr;
        if (flash_area_open(area, &fa) != 0) {
            setFailure(failure, Error::DeviceNotFound, QStringLiteral("No slot1_partition"));
            return false;
        }
        const size_t capacity = fa->fa_size;
        flash_area_close(fa);
        if (size > 0 && static_cast<quint64>(size) > capacity) {
            setFailure(failure, -EFBIG, QStringLiteral("The image does not fit into slot1_partition"));
            return false;
        }
#ifndef CONFIG_IMG_ERASE_PROGRESSIVELY
        const int erased = boot_erase_img_bank(area);
        if (erased != 0) {
            setFailure(failure, erased, QStringLiteral("Erasing slot1_partition"));
            return false;
        }
#endif
        const int rc = flash_img_init(&m_context);
        if (rc != 0) {
            setFailure(failure, rc, QStringLiteral("Opening slot1_partition"));
            return false;
        }
        return true;
#else
        Q_UNUSED(size);
        return notConfigured("CONFIG_IMG_MANAGER and a slot1_partition", failure);
#endif
    }

    bool write(const char *data, qsizetype size, Failure &failure) override {
#if defined(CONFIG_IMG_MANAGER) && defined(NT_HAVE_SLOTS)
        const int rc = flash_img_buffered_write(&m_context, reinterpret_cast<const uint8_t *>(data),
                                                static_cast<size_t>(size), false);
        if (rc != 0) {
            setFailure(failure, rc, QStringLiteral("Writing slot1_partition"));
            return false;
        }
        return true;
#else
        Q_UNUSED(data);
        Q_UNUSED(size);
        return notConfigured("CONFIG_IMG_MANAGER", failure);
#endif
    }

    bool finish(QByteArray &header, Failure &failure) override {
#if defined(CONFIG_IMG_MANAGER) && defined(NT_HAVE_SLOTS)
        const int rc = flash_img_buffered_write(&m_context, nullptr, 0, true);
        if (rc != 0) {
            setFailure(failure, rc, QStringLiteral("Writing slot1_partition"));
            return false;
        }
        header = readHeader(PARTITION_ID(slot1_partition));
        return true;
#else
        Q_UNUSED(header);
        return notConfigured("CONFIG_IMG_MANAGER", failure);
#endif
    }

    void abort() override {}

    bool requestUpgrade(bool permanent, Failure &failure) override {
#ifdef CONFIG_MCUBOOT_IMG_MANAGER
        const int rc = boot_request_upgrade(permanent ? BOOT_UPGRADE_PERMANENT : BOOT_UPGRADE_TEST);
        if (rc != 0) {
            setFailure(failure, rc, QStringLiteral("Requesting the upgrade"));
            return false;
        }
        return true;
#else
        Q_UNUSED(permanent);
        return notConfigured("CONFIG_MCUBOOT_IMG_MANAGER", failure);
#endif
    }

    bool isConfirmed() override {
#ifdef CONFIG_MCUBOOT_IMG_MANAGER
        return boot_is_img_confirmed();
#else
        return true;
#endif
    }

    bool confirm(Failure &failure) override {
#ifdef CONFIG_MCUBOOT_IMG_MANAGER
        const int rc = boot_write_img_confirmed();
        if (rc != 0) {
            setFailure(failure, rc, QStringLiteral("Confirming the image"));
            return false;
        }
        return true;
#else
        return notConfigured("CONFIG_MCUBOOT_IMG_MANAGER", failure);
#endif
    }

    QByteArray runningHeader() override {
#if defined(CONFIG_MCUBOOT_IMG_MANAGER) && defined(NT_HAVE_SLOTS)
        return readHeader(boot_fetch_active_slot());
#else
        return QByteArray();
#endif
    }

    bool reboot(Failure &failure) override {
#ifdef CONFIG_REBOOT
        Q_UNUSED(failure);
        sys_reboot(SYS_REBOOT_COLD);
        return true;
#else
        return notConfigured("CONFIG_REBOOT", failure);
#endif
    }

private:
#ifdef CONFIG_IMG_MANAGER
    flash_img_context m_context = {};
#endif
};

} // namespace

std::unique_ptr<WatchdogBackend> createPlatformWatchdogBackend() { return std::make_unique<ZephyrWatchdogBackend>(); }
std::unique_ptr<FirmwareBackend> createPlatformFirmwareBackend() { return std::make_unique<ZephyrFirmwareBackend>(); }

} // namespace QtDevicePrivate
