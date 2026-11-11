// MIT License - see LICENSE
// The functions that ntposix_zephyr_p.h declares. They are weak, so that the definitions of a
// later Zephyr take their place.
#include "ntposix_zephyr_p.h"

#include <cerrno>
#include <cstring>

#define NT_WEAK __attribute__((weak))

extern "C" {

NT_WEAK int access(const char *path, int /*mode*/) {
    struct stat st;
    return ::stat(path, &st);
}

NT_WEAK int lstat(const char *path, struct stat *buf) { return ::stat(path, buf); }

NT_WEAK ssize_t readlink(const char * /*path*/, char * /*buf*/, size_t /*size*/) {
    errno = EINVAL;
    return -1;
}

NT_WEAK int symlink(const char * /*target*/, const char * /*path*/) {
    errno = ENOTSUP;
    return -1;
}

NT_WEAK int link(const char * /*from*/, const char * /*to*/) {
    errno = ENOTSUP;
    return -1;
}

NT_WEAK uid_t getuid(void) { return 0; }

NT_WEAK struct passwd *getpwuid(uid_t /*uid*/) { return nullptr; }

NT_WEAK char *getcwd(char *buf, size_t size) {
    if (!buf || size < 2) {
        errno = buf ? ERANGE : EINVAL;
        return nullptr;
    }
    std::strcpy(buf, "/");
    return buf;
}

NT_WEAK int chdir(const char * /*path*/) {
    errno = ENOTSUP;
    return -1;
}

} // extern "C"
