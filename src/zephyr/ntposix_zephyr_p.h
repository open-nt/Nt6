// MIT License - see LICENSE
#pragma once

// POSIX on Zephyr stops short of what Nt Core uses on a hosted system: there are no users, no
// links, no working directory and no close-on-exec, because there is one process and it never
// runs another program. This file declares the missing functions and constants so that the
// sources stay free of conditionals; ntposix_zephyr.cpp defines the functions, as weak symbols
// that give way if Zephyr grows the real ones. The build includes it ahead of every source file
// of Nt Core and Nt Network (see NT_ZEPHYR in CMakeLists.txt).

#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// There is no exec() to close a descriptor on, and every open of a directory fails anyway.
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif

#ifndef F_OK
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
#endif

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 64
#endif

#ifndef AT_FDCWD
#define AT_FDCWD (-100)
#endif
#ifndef UTIME_OMIT
#define UTIME_NOW (-2L)
#define UTIME_OMIT (-1L)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Succeeds if the file exists: a Zephyr file system keeps no permissions.
int access(const char *path, int mode);
// The same as stat(), as there are no symbolic links.
int lstat(const char *path, struct stat *buf);
// Fail with EINVAL (nothing is a link) and ENOTSUP.
ssize_t readlink(const char *path, char *buf, size_t size);
int symlink(const char *target, const char *path);
int link(const char *from, const char *to);
// The only user is 0, and it has no entry in a user database.
uid_t getuid(void);
struct passwd *getpwuid(uid_t uid);
// The working directory is always "/", so every path that Nt opens has to be absolute.
char *getcwd(char *buf, size_t size);
int chdir(const char *path);

#ifdef __cplusplus
}
#endif
