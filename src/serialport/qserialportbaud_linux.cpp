// MIT License - see LICENSE
#include "qserialportbaud_p.h"

#include <asm/termbits.h>
#include <sys/ioctl.h>

#include <cerrno>

namespace QtPrivate {

bool setSerialCustomBaudRate(int fd, int inputRate, int outputRate) {
#if defined(TCGETS2) && defined(BOTHER)
    struct termios2 tio = {};
    if (::ioctl(fd, TCGETS2, &tio) != 0)
        return false;
    tio.c_cflag &= ~static_cast<tcflag_t>(CBAUD | (CBAUD << IBSHIFT));
    tio.c_cflag |= BOTHER | (BOTHER << IBSHIFT);
    tio.c_ispeed = static_cast<speed_t>(inputRate);
    tio.c_ospeed = static_cast<speed_t>(outputRate);
    return ::ioctl(fd, TCSETS2, &tio) == 0;
#else
    static_cast<void>(fd);
    static_cast<void>(inputRate);
    static_cast<void>(outputRate);
    errno = ENOTSUP;
    return false;
#endif
}

} // namespace QtPrivate
