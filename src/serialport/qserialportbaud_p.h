// MIT License - see LICENSE
#pragma once

namespace QtPrivate {

// Sets a rate that has no B constant. Returns false with errno set if the
// system cannot. It has a file of its own because the kernel's termios2
// definitions cannot be included next to <termios.h>.
bool setSerialCustomBaudRate(int fd, int inputRate, int outputRate);

} // namespace QtPrivate
