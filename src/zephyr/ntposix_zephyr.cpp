// MIT License - see LICENSE
// The functions that ntposix_zephyr_p.h declares. They are weak, so that the definitions of a
// later Zephyr take their place.
#include "ntposix_zephyr_p.h"

#include <zephyr/kernel.h>

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
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

NT_WEAK int chmod(const char *path, mode_t /*mode*/) { return access(path, F_OK); }

NT_WEAK int fchmod(int /*fd*/, mode_t /*mode*/) { return 0; }

NT_WEAK mode_t umask(mode_t /*mask*/) { return 0; }

NT_WEAK int mkostemp(char *templ, int flags) {
    const std::size_t length = templ ? std::strlen(templ) : 0;
    if (length < 6 || std::strcmp(templ + length - 6, "XXXXXX") != 0) {
        errno = EINVAL;
        return -1;
    }
    static const char kAlphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    static uint32_t counter = 0;
    for (int attempt = 0; attempt < 100; ++attempt) {
        // One process owns the file system, so the names only have to differ, not be unpredictable.
        uint32_t seed = k_cycle_get_32() ^ (++counter * 2654435761U);
        for (std::size_t i = length - 6; i < length; ++i) {
            templ[i] = kAlphabet[seed % 36];
            seed /= 36;
            seed ^= counter << 7;
        }
        struct stat st;
        if (::stat(templ, &st) == 0)
            continue; // O_EXCL is not honoured by every Zephyr file system, so a name in use is skipped here
        const int fd = ::open(templ, O_RDWR | O_CREAT | O_EXCL | (flags & ~O_ACCMODE), 0600);
        if (fd >= 0 || errno != EEXIST)
            return fd;
    }
    errno = EEXIST;
    return -1;
}

NT_WEAK int gethostname(char *name, size_t size) {
#ifdef CONFIG_NET_HOSTNAME
    const char *host = CONFIG_NET_HOSTNAME;
#else
    const char *host = "zephyr";
#endif
    if (!name || std::strlen(host) >= size) {
        errno = name ? ENAMETOOLONG : EINVAL;
        return -1;
    }
    std::strcpy(name, host);
    return 0;
}

NT_WEAK char *realpath(const char *path, char *resolved) {
    if (!path) {
        errno = EINVAL;
        return nullptr;
    }
    char out[PATH_MAX];
    std::size_t used = 0;
    const char *p = path;
    while (*p) {
        while (*p == '/')
            ++p;
        const char *end = p;
        while (*end && *end != '/')
            ++end;
        const std::size_t n = static_cast<std::size_t>(end - p);
        if (n == 0 || (n == 1 && p[0] == '.')) {
            // nothing to add
        } else if (n == 2 && p[0] == '.' && p[1] == '.') {
            while (used > 0 && out[used - 1] != '/')
                --used;
            if (used > 0)
                --used; // the slash before the removed component
        } else {
            if (used + 1 + n >= sizeof out) {
                errno = ENAMETOOLONG;
                return nullptr;
            }
            out[used++] = '/';
            std::memcpy(out + used, p, n);
            used += n;
        }
        p = end;
    }
    if (used == 0)
        out[used++] = '/';
    out[used] = '\0';
    struct stat st;
    if (::stat(out, &st) != 0 && std::strcmp(out, "/") != 0)
        return nullptr; // stat() set errno
    char *target = resolved ? resolved : static_cast<char *>(std::malloc(used + 1));
    if (!target) {
        errno = ENOMEM;
        return nullptr;
    }
    std::memcpy(target, out, used + 1);
    return target;
}

} // extern "C"
