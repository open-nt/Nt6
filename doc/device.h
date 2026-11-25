// MIT License - see LICENSE
//
// Overview page about Nt Device for the NDoc documentation. This file holds only a documentation comment and is not
// part of the library.

/*!
    \page nt-device.html
    \title Using Nt Device
    \brief Keeping an embedded device alive with a watchdog and updating its firmware over the air.

    Nt Device is a module of Nt only, for devices that run unattended. Link with \c Nt::Device.

    \section1 The Watchdog

    QWatchdog arms the hardware watchdog, which resets the system unless it is fed within its
    timeout. With autoFeed the event loop feeds it, so the device restarts by itself when the loop
    hangs, for example in a slot that waits forever:

    \code
    QWatchdog watchdog;
    watchdog.setTimeout(8000);
    watchdog.setAutoFeed(true);
    if (watchdog.causedLastReset())
        qWarning() << "the watchdog reset the device";
    watchdog.start();
    \endcode

    A watchdog that should also catch a worker thread that hangs is fed by hand instead, from a
    place that only runs when every part is healthy. On Linux the device is \c{/dev/watchdog}; on
    Zephyr, with \c CONFIG_WATCHDOG, the one that the alias \c watchdog0 names (WDOG1 on the
    MIMXRT1170-EVKB), and \c CONFIG_HWINFO adds causedLastReset().

    \section1 Firmware Updates

    QFirmwareUpdate writes a new image while the old one runs, and the MCUboot boot loader swaps
    them at the next reset. The new firmware runs on trial: unless it confirms itself, MCUboot goes
    back to the old image at the reset after. A download with QNetworkAccessManager goes straight
    into the flash:

    \code
    auto *update = new QFirmwareUpdate(this);
    QNetworkReply *reply = manager.get(QNetworkRequest(QUrl(QStringLiteral("http://server/fw/zephyr.signed.bin"))));
    QObject::connect(reply, &QNetworkReply::metaDataChanged, [=] {
        update->begin(reply->header(QNetworkRequest::ContentLengthHeader).toLongLong());
    });
    QObject::connect(reply, &QIODevice::readyRead, [=] { update->writeFrom(reply); });
    QObject::connect(reply, &QNetworkReply::finished, [=] {
        update->writeFrom(reply);
        if (reply->error() == QNetworkReply::NoError && update->finish() && update->requestUpgrade())
            QFirmwareUpdate::reboot();
    });
    \endcode

    The new firmware confirms itself once it knows that it works:

    \code
    if (!QFirmwareUpdate::isRunningImageConfirmed()) {
        QObject::connect(&client, &QMqttClient::connected, [] { QFirmwareUpdate::confirmRunningImage(); });
    }
    \endcode

    The Zephyr application is built with MCUboot by sysbuild, and its signed image,
    \c build/<app>/zephyr/zephyr.signed.bin, is what the device downloads:

    \code
    # prj.conf
    CONFIG_NT_DEVICE=y
    CONFIG_BOOTLOADER_MCUBOOT=y
    CONFIG_IMG_MANAGER=y
    CONFIG_MCUBOOT_IMG_MANAGER=y
    CONFIG_STREAM_FLASH=y
    CONFIG_REBOOT=y
    \endcode

    \code
    west build --sysbuild -b mimxrt1170_evk/mimxrt1176/cm7 app -- -DNT_ROOT=/path/to/Nt
    \endcode

    MCUboot checks the signature of the image before it boots it, with the key it was built with;
    sign images for the field with a key of your own (\c SB_CONFIG_BOOT_SIGNATURE_KEY_FILE), not
    the key for development that MCUboot comes with.

    \sa {Nt Device C++ Classes}, {Nt on Zephyr}
*/
