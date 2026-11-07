// MIT License - see LICENSE
// Child program of the QProcess tests; it does one thing selected by its first argument.
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern char **environ;

static void writeAll(int fd, const char *data, size_t size) {
    while (size > 0) {
        const ssize_t n = ::write(fd, data, size);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        data += n;
        size -= static_cast<size_t>(n);
    }
}

static void sleepMs(long ms) {
    timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    while (::nanosleep(&ts, &ts) < 0 && errno == EINTR) {
    }
}

static void writeStr(int fd, const std::string &s) { writeAll(fd, s.data(), s.size()); }

static void writePattern(int fd, long total) {
    char buf[8192];
    long done = 0;
    while (done < total) {
        const long chunk = total - done < static_cast<long>(sizeof buf) ? total - done : static_cast<long>(sizeof buf);
        for (long i = 0; i < chunk; ++i)
            buf[i] = static_cast<char>('a' + (done + i) % 26);
        writeAll(fd, buf, static_cast<size_t>(chunk));
        done += chunk;
    }
}

int main(int argc, char **argv) {
    if (argc < 2)
        return 90;
    const std::string mode = argv[1];
    if (mode == "exit") {
        return argc > 2 ? std::atoi(argv[2]) : 0;
    } else if (mode == "args") {
        for (int i = 2; i < argc; ++i)
            writeStr(1, std::string(argv[i]) + "\n");
    } else if (mode == "cat") {
        char buf[65536];
        for (;;) {
            const ssize_t n = ::read(0, buf, sizeof buf);
            if (n < 0 && errno == EINTR)
                continue;
            if (n <= 0)
                break;
            writeAll(1, buf, static_cast<size_t>(n));
        }
    } else if (mode == "count") {
        char buf[65536];
        long total = 0;
        for (;;) {
            const ssize_t n = ::read(0, buf, sizeof buf);
            if (n < 0 && errno == EINTR)
                continue;
            if (n <= 0)
                break;
            total += n;
        }
        writeStr(1, std::to_string(total));
    } else if (mode == "out") {
        writeStr(1, argc > 2 ? argv[2] : "");
    } else if (mode == "err") {
        writeStr(2, argc > 2 ? argv[2] : "");
    } else if (mode == "both") {
        writeStr(1, "out1;");
        writeStr(2, "err1;");
        writeStr(1, "out2;");
        writeStr(2, "err2;");
    } else if (mode == "env") {
        const char *v = argc > 2 ? ::getenv(argv[2]) : nullptr;
        writeStr(1, v ? v : "<unset>");
    } else if (mode == "envcount") {
        long n = 0;
        for (char **e = environ; e && *e; ++e)
            ++n;
        writeStr(1, std::to_string(n));
    } else if (mode == "cwd") {
        char buf[4096];
        writeStr(1, ::getcwd(buf, sizeof buf) ? buf : "?");
    } else if (mode == "big") {
        writePattern(1, argc > 2 ? std::atol(argv[2]) : 0);
    } else if (mode == "bigerr") {
        writePattern(2, argc > 2 ? std::atol(argv[2]) : 0);
    } else if (mode == "bigboth") {
        const long n = argc > 2 ? std::atol(argv[2]) : 0;
        writePattern(1, n);
        writePattern(2, n);
    } else if (mode == "signal") {
        const int sig = argc > 2 ? std::atoi(argv[2]) : SIGKILL;
        signal(sig, SIG_DFL);
        sigset_t s;
        sigemptyset(&s);
        sigaddset(&s, sig);
        sigprocmask(SIG_UNBLOCK, &s, nullptr);
        ::raise(sig);
        sleepMs(5000);
    } else if (mode == "sleep") {
        sleepMs(argc > 2 ? std::atol(argv[2]) : 1000);
    } else if (mode == "ignoreterm") {
        signal(SIGTERM, SIG_IGN);
        writeStr(1, "ready\n");
        sleepMs(argc > 2 ? std::atol(argv[2]) : 30000);
    } else if (mode == "ready") {
        writeStr(1, "ready\n");
        sleepMs(argc > 2 ? std::atol(argv[2]) : 30000);
    } else if (mode == "closestdin") {
        ::close(0);
        writeStr(1, "closed\n");
        sleepMs(argc > 2 ? std::atol(argv[2]) : 30000);
    } else if (mode == "fds") {
        DIR *d = ::opendir("/proc/self/fd");
        std::string out;
        const int self = d ? ::dirfd(d) : -1;
        while (d) {
            const dirent *e = ::readdir(d);
            if (!e)
                break;
            const int fd = std::atoi(e->d_name);
            if (e->d_name[0] != '.' && fd > 2 && fd != self)
                out += std::to_string(fd) + " ";
        }
        if (d)
            ::closedir(d);
        writeStr(1, "[" + out + "]");
    } else if (mode == "sigpipe") {
        struct sigaction sa;
        sigaction(SIGPIPE, nullptr, &sa);
        writeStr(1, sa.sa_handler == SIG_IGN ? "ignored" : sa.sa_handler == SIG_DFL ? "default" : "handler");
    } else if (mode == "sigmask") {
        sigset_t cur;
        sigprocmask(SIG_BLOCK, nullptr, &cur);
        int blocked = 0;
        for (int s = 1; s < 32; ++s)
            blocked += sigismember(&cur, s) == 1;
        writeStr(1, std::to_string(blocked));
    } else if (mode == "umask") {
        const mode_t m = ::umask(0);
        char buf[16];
        std::snprintf(buf, sizeof buf, "%03o", static_cast<unsigned>(m));
        writeStr(1, buf);
    } else if (mode == "background") {
        // The grandchild keeps our stdout open long after we exit.
        const pid_t p = ::fork();
        if (p == 0) {
            sleepMs(argc > 2 ? std::atol(argv[2]) : 3000);
            ::_exit(0);
        }
        writeStr(1, "parent-done\n");
    } else if (mode == "stdinfile") {
        char buf[256];
        const ssize_t n = ::read(0, buf, sizeof buf);
        if (n > 0)
            writeAll(1, buf, static_cast<size_t>(n));
    } else if (mode == "pid") {
        writeStr(1, std::to_string(::getpid()));
    } else if (mode == "isatty") {
        writeStr(1, ::isatty(0) ? "tty" : "notty");
    } else {
        return 91;
    }
    return 0;
}
