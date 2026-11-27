// MIT License - see LICENSE
//
// The emulator backends of Nt GPIO (see qhwemulatorprotocol_p.h). When NT_GPIO_EMULATOR is set,
// the create*Backend() functions return these instead of the platform's. Not built for Zephyr.
#pragma once

#include <QtCore/qstring.h>

#include <memory>

namespace QtPrivate {

class GpioBackend;
class SpiBackend;
class I2cBackend;

// The value of NT_GPIO_EMULATOR, or an empty string.
QString hwEmulatorName();
bool hwEmulatorEnabled();

std::unique_ptr<GpioBackend> createEmulatorGpioBackend();
std::unique_ptr<SpiBackend> createEmulatorSpiBackend();
std::unique_ptr<I2cBackend> createEmulatorI2cBackend();

} // namespace QtPrivate
