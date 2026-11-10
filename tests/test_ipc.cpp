// MIT License - see LICENSE
#include <QtCore>
#include <QtCore/QLibrary>
#include <QtCore/QSharedMemory>
#include <QtCore/QSystemSemaphore>
#include <QtCore/qtemporarydir.h>

#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

// A key that no other run of the tests uses.
static QString uniqueKey(const char *name) {
    return QString("nt-test-") + QString::fromUtf8(name) + QChar(u'-') + QString::number(static_cast<qlonglong>(::getpid()));
}

// Runs the function in a child process and returns its result, or -1 if the child did not end normally.
template <typename Function> static int inChild(Function function) {
    std::fflush(stdout);
    const pid_t pid = ::fork();
    if (pid == 0)
        ::_exit(function());
    int status = 0;
    if (pid < 0 || ::waitpid(pid, &status, 0) != pid || !WIFEXITED(status))
        return -1;
    return WEXITSTATUS(status);
}

// QLibrary

static void testLibrary() {
    const QString dir = QString::fromUtf8(NT_TEST_PLUGIN_DIR);
    const QString full = dir + "/libnt_test_plugin.so";
    using Add = int (*)(int, int);
    using Calls = int (*)();
    using Name = const char *(*)();

    CHECK(QLibrary::isLibrary("libz.so") && QLibrary::isLibrary("/usr/lib/libz.so.1") && QLibrary::isLibrary("libz.so.1.3.1"));
    CHECK(QLibrary::isLibrary("plugin.so") && QLibrary::isLibrary(full));
    CHECK(!QLibrary::isLibrary("libz") && !QLibrary::isLibrary("libz.a") && !QLibrary::isLibrary("libz.so.x"));
    CHECK(!QLibrary::isLibrary("so") && !QLibrary::isLibrary("libz.so.1.txt") && !QLibrary::isLibrary(QString()));
    CHECK(!QLibrary::isLibrary("dir.so/file") && !QLibrary::isLibrary("1.2"));

    // Nothing to load
    QLibrary none;
    CHECK(none.fileName().isEmpty() && !none.isLoaded() && !none.load() && !none.errorString().isEmpty());
    CHECK(none.resolve("nt_plugin_add") == nullptr && !none.unload());
    QLibrary missing(dir + "/no_such_library");
    CHECK(!missing.load() && !missing.isLoaded() && missing.errorString().contains(QString("no_such_library")));
    CHECK(missing.fileName() == dir + "/no_such_library");
    CHECK(QLibrary::resolve(dir + "/no_such_library", "nt_plugin_add") == nullptr);

    // By the full path, and by the name without what the platform adds
    QLibrary library(full);
    CHECK(!library.isLoaded() && library.fileName() == full && library.loadHints() == QLibrary::LoadHints());
    CHECK(library.load() && library.isLoaded() && library.errorString().isEmpty() && library.load());
    CHECK(library.fileName() == QFileInfo(full).absoluteFilePath());
    const Add add = reinterpret_cast<Add>(library.resolve("nt_plugin_add"));
    CHECK(add && add(40, 2) == 42);
    const Name name = reinterpret_cast<Name>(library.resolve("nt_plugin_name"));
    CHECK(name && std::string(name()) == "nt_test_plugin");
    CHECK(library.resolve("nt_plugin_no_such_symbol") == nullptr);
    CHECK(library.errorString().contains(QString("nt_plugin_no_such_symbol")) && library.isLoaded());
    CHECK(library.resolve("nt_plugin_add") != nullptr && library.errorString().isEmpty());

    QLibrary bare(dir + "/nt_test_plugin");
    CHECK(reinterpret_cast<Add>(bare.resolve("nt_plugin_add")) == add && bare.isLoaded()); // resolve() loads
    CHECK(bare.fileName() == QFileInfo(full).absoluteFilePath());
    QLibrary withPrefix(dir + "/libnt_test_plugin");
    CHECK(withPrefix.load() && withPrefix.fileName() == bare.fileName());

    // The objects share the library: it goes with the last of them
    const Calls calls = reinterpret_cast<Calls>(library.resolve("nt_plugin_calls"));
    CHECK(calls && calls() == 1 && calls() == 2);
    CHECK(reinterpret_cast<Calls>(bare.resolve("nt_plugin_calls")) == calls && calls() == 3);
    CHECK(!library.unload() && !library.isLoaded() && !library.unload());
    CHECK(calls() == 4); // still in memory
    CHECK(!bare.unload());
    CHECK(withPrefix.unload() && !withPrefix.isLoaded());
    CHECK(library.fileName() == full); // the name that was given, again

    // The static functions; the library stays loaded
    CHECK(QLibrary::resolve(full, "nt_plugin_add") != nullptr);
    CHECK(QLibrary::resolve(dir + "/nt_test_plugin", "nt_plugin_no_such_symbol") == nullptr);

    // Versions: libname.so.<version>
    QTemporaryDir temp;
    CHECK(temp.isValid());
    CHECK(::symlink(full.toUtf8().constData(), (temp.path() + "/libversioned.so.2").toUtf8().constData()) == 0);
    CHECK(::symlink(full.toUtf8().constData(), (temp.path() + "/libversioned.so.3.1").toUtf8().constData()) == 0);
    QLibrary second(temp.path() + "/versioned", 2);
    CHECK(second.load() && second.fileName() == temp.path() + "/libversioned.so.2");
    CHECK(second.resolve("nt_plugin_add") != nullptr);
    second.unload();
    QLibrary third(temp.path() + "/versioned", QString("3.1"));
    CHECK(third.load() && third.fileName() == temp.path() + "/libversioned.so.3.1");
    third.unload();
    QLibrary wrong(temp.path() + "/versioned", 7);
    CHECK(!wrong.load());
    wrong.setFileNameAndVersion(temp.path() + "/versioned", 2);
    CHECK(wrong.load() && wrong.unload());
    wrong.setFileNameAndVersion(temp.path() + "/versioned", QString("3.1"));
    CHECK(wrong.load());
    wrong.unload();
    CHECK(QLibrary::resolve(temp.path() + "/versioned", 2, "nt_plugin_add") != nullptr);
    CHECK(QLibrary::resolve(temp.path() + "/versioned", QString("3.1"), "nt_plugin_add") != nullptr);
    CHECK(QLibrary::resolve(temp.path() + "/versioned", 9, "nt_plugin_add") == nullptr);
    CHECK(QLibrary::resolve(temp.path() + "/versioned", -1, "nt_plugin_add") == nullptr); // no version: no such file

    // A new file name makes the object stand for another library
    QLibrary renamed(dir + "/no_such_library");
    CHECK(!renamed.load());
    renamed.setFileName(full);
    CHECK(renamed.fileName() == full && renamed.load() && renamed.isLoaded());
    renamed.setFileName(dir + "/other");
    CHECK(!renamed.isLoaded() && renamed.fileName() == dir + "/other" && !renamed.unload());

    // Hints: with all symbols resolved at once and kept in memory, the counter of the library goes on
    QLibrary hinted(full);
    hinted.setLoadHints(QLibrary::ResolveAllSymbolsHint | QLibrary::PreventUnloadHint);
    CHECK(hinted.loadHints() == (QLibrary::ResolveAllSymbolsHint | QLibrary::PreventUnloadHint));
    const Calls hintedCalls = reinterpret_cast<Calls>(hinted.resolve("nt_plugin_calls"));
    CHECK(hintedCalls != nullptr);
    const int before = hintedCalls ? hintedCalls() : 0;
    hinted.unload();
    QLibrary again(full);
    // The runtime of the sanitizers ends the process when a library is loaded with RTLD_DEEPBIND.
    again.setLoadHints(QLibrary::ExportExternalSymbolsHint | QLibrary::DeepBindHint | QLibrary::LoadArchiveMemberHint);
    CHECK(again.loadHints().testFlag(QLibrary::DeepBindHint));
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__) || defined(NT_TEST_NO_DEEPBIND)
    again.setLoadHints(QLibrary::ExportExternalSymbolsHint | QLibrary::LoadArchiveMemberHint);
#elif defined(__has_feature)
#  if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || __has_feature(memory_sanitizer)
    again.setLoadHints(QLibrary::ExportExternalSymbolsHint | QLibrary::LoadArchiveMemberHint);
#  endif
#endif
    const Calls againCalls = reinterpret_cast<Calls>(again.resolve("nt_plugin_calls"));
    CHECK(againCalls && againCalls() == before + 1);

    // A library of the system, found without a directory
    QLibrary math(QString("m"), 6);
    if (math.load()) {
        using Cosine = double (*)(double);
        const Cosine cosine = reinterpret_cast<Cosine>(math.resolve("cos"));
        CHECK(cosine && cosine(0.0) == 1.0);
        CHECK(math.fileName() == QString("libm.so.6"));
        math.unload();
    } else {
        std::puts("libm.so.6 was not found: the system library is not tested");
    }

    // The meta-object
    QObject parent;
    auto *child = new QLibrary(full, &parent);
    CHECK(child->parent() == &parent && std::string(child->metaObject()->className()) == "QLibrary");
    CHECK(child->property("fileName").toString() == full);
    CHECK(child->setProperty("fileName", dir + "/nt_test_plugin") && child->fileName() == dir + "/nt_test_plugin");
    CHECK(child->metaObject()->indexOfProperty("loadHints") >= 0);
    const QMetaEnum hints = QMetaEnum::fromType<QLibrary::LoadHint>();
    CHECK(hints.isValid() && hints.keyCount() == 5 && std::string(hints.valueToKey(QLibrary::DeepBindHint)) == "DeepBindHint");
    CHECK((new QLibrary(full, 1, &parent))->parent() == &parent);
    CHECK((new QLibrary(full, QString("1"), &parent))->parent() == &parent);
}

// QSystemSemaphore

static void testSystemSemaphore() {
    {
        QSystemSemaphore empty(QString(), 1);
        CHECK(empty.key().isEmpty() && empty.error() == QSystemSemaphore::KeyError && !empty.errorString().isEmpty());
        CHECK(!empty.acquire() && !empty.release());
    }

    const QString key = uniqueKey("semaphore");
    {
        QSystemSemaphore semaphore(key, 2, QSystemSemaphore::Create);
        CHECK(semaphore.key() == key && semaphore.error() == QSystemSemaphore::NoError && semaphore.errorString().isEmpty());
        CHECK(semaphore.acquire() && semaphore.acquire());
        CHECK(semaphore.release() && semaphore.acquire());
        CHECK(semaphore.release(0) && !semaphore.release(-1));
        CHECK(semaphore.release(3));
        CHECK(semaphore.acquire() && semaphore.acquire() && semaphore.acquire());

        // Another object for the key shares the count; its initial value is not used
        QSystemSemaphore other(key, 5);
        CHECK(other.error() == QSystemSemaphore::NoError && other.key() == key);
        CHECK(other.release(2) && semaphore.acquire() && other.acquire());
        CHECK(semaphore.release() && other.acquire());

        // Another process: the child gives what the parent waits for, and takes what the parent gives
        CHECK(inChild([&] {
                  QSystemSemaphore mine(key, 0);
                  return mine.error() == QSystemSemaphore::NoError && mine.release(2) ? 0 : 1;
              }) == 0);
        CHECK(semaphore.acquire() && semaphore.acquire());
        CHECK(semaphore.release());
        CHECK(inChild([&] {
                  QSystemSemaphore mine(key, 0);
                  return mine.acquire() ? 0 : 1;
              }) == 0);
        // The parent waits in acquire() until the child, which starts later, releases
        std::fflush(stdout);
        const pid_t pid = ::fork();
        if (pid == 0) {
            ::usleep(50 * 1000);
            QSystemSemaphore mine(key, 0);
            ::_exit(mine.release() ? 0 : 1);
        }
        QElapsedTimer timer;
        timer.start();
        CHECK(semaphore.acquire() && timer.elapsed() >= 30);
        int status = 0;
        CHECK(::waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);

        // Create makes the semaphore anew, with its own count
        CHECK(semaphore.release(4));
        QSystemSemaphore fresh(key, 1, QSystemSemaphore::Create);
        CHECK(fresh.error() == QSystemSemaphore::NoError && fresh.acquire());
        CHECK(fresh.release(2) && fresh.acquire() && fresh.acquire());

        // setKey() moves an object to another semaphore
        const QString otherKey = uniqueKey("semaphore-two");
        other.setKey(otherKey, 3, QSystemSemaphore::Create);
        CHECK(other.key() == otherKey && other.error() == QSystemSemaphore::NoError);
        CHECK(other.acquire() && other.acquire() && other.acquire());
        other.setKey(QString());
        CHECK(other.error() == QSystemSemaphore::KeyError && !other.acquire());
    }
    // The semaphores went with the objects that created them: the key gives a new one
    QSystemSemaphore later(key, 3);
    CHECK(later.error() == QSystemSemaphore::NoError);
    CHECK(later.acquire() && later.acquire() && later.acquire());
    // A negative initial value counts as none
    QSystemSemaphore negative(uniqueKey("semaphore-negative"), -4, QSystemSemaphore::Create);
    CHECK(negative.error() == QSystemSemaphore::NoError && negative.release() && negative.acquire());
    // Keys that differ only in characters that are not letters are different semaphores
    QSystemSemaphore dotted(uniqueKey("semaphore.a"), 1, QSystemSemaphore::Create);
    QSystemSemaphore dashed(uniqueKey("semaphore-a"), 0, QSystemSemaphore::Create);
    CHECK(dotted.acquire() && dashed.release(2) && dashed.acquire() && dashed.acquire());
}

// QSharedMemory

static void testSharedMemory() {
    {
        QSharedMemory none;
        CHECK(none.key().isEmpty() && none.nativeKey().isEmpty() && !none.isAttached() && none.size() == 0);
        CHECK(none.data() == nullptr && none.constData() == nullptr && none.error() == QSharedMemory::NoError);
        CHECK(!none.create(100) && none.error() == QSharedMemory::KeyError && !none.errorString().isEmpty());
        CHECK(!none.attach() && none.error() == QSharedMemory::KeyError);
        CHECK(!none.lock() && !none.unlock() && !none.detach());
    }

    const QString key = uniqueKey("memory");
    QSharedMemory memory(key);
    CHECK(memory.key() == key && memory.nativeKey().startsWith(QString("/qipc_sharedmemory_")) && !memory.isAttached());
    CHECK(!memory.nativeKey().mid(1).contains(QChar(u'/')) && memory.nativeKey().size() < 200);
    CHECK(QSharedMemory(uniqueKey("memory.")).nativeKey() != QSharedMemory(uniqueKey("memory-")).nativeKey());
    CHECK(!memory.attach() && memory.error() == QSharedMemory::NotFound);
    CHECK(!memory.create(0) && memory.error() == QSharedMemory::InvalidSize);
    CHECK(!memory.create(-5) && memory.error() == QSharedMemory::InvalidSize && !memory.isAttached());
    CHECK(!memory.lock() && memory.error() == QSharedMemory::LockError); // not attached

    CHECK(memory.create(4096) && memory.error() == QSharedMemory::NoError && memory.errorString().isEmpty());
    CHECK(memory.isAttached() && memory.size() == 4096 && memory.data() != nullptr);
    CHECK(memory.constData() == memory.data() && static_cast<const QSharedMemory &>(memory).data() == memory.data());
    bool zeros = true;
    for (qsizetype i = 0; i < memory.size(); ++i)
        zeros = zeros && static_cast<const char *>(memory.constData())[i] == 0;
    CHECK(zeros);
    CHECK(!memory.create(4096) && memory.error() == QSharedMemory::LockError); // attached already
    CHECK(!memory.attach() && memory.error() == QSharedMemory::LockError && memory.isAttached());
    std::memcpy(memory.data(), "hello", 6);

    // Another object sees the same bytes, at another address
    QSharedMemory second(key);
    CHECK(second.nativeKey() == memory.nativeKey());
    CHECK(!second.create(4096) && second.error() == QSharedMemory::AlreadyExists && !second.isAttached());
    CHECK(second.attach() && second.isAttached() && second.size() == 4096 && second.data() != memory.data());
    CHECK(std::strcmp(static_cast<const char *>(second.constData()), "hello") == 0);
    static_cast<char *>(second.data())[0] = 'j';
    CHECK(std::strcmp(static_cast<const char *>(memory.constData()), "jello") == 0);
    QSharedMemory reader(key);
    CHECK(reader.attach(QSharedMemory::ReadOnly) && reader.size() == 4096);
    CHECK(std::strcmp(static_cast<const char *>(reader.constData()), "jello") == 0);

    // The lock
    CHECK(memory.lock() && memory.error() == QSharedMemory::NoError);
    CHECK(!memory.lock() && memory.error() == QSharedMemory::LockError);
    CHECK(memory.unlock() && !memory.unlock());
    CHECK(second.lock() && second.unlock() && reader.lock() && reader.unlock());

    // Another process reads what this one wrote, and answers
    CHECK(inChild([&] {
              QSharedMemory mine(key);
              if (!mine.attach() || mine.size() != 4096 || !mine.lock())
                  return 1;
              const bool same = std::strcmp(static_cast<const char *>(mine.constData()), "jello") == 0;
              std::memcpy(static_cast<char *>(mine.data()) + 100, "answer", 7);
              return mine.unlock() && same ? 0 : 2;
          }) == 0);
    CHECK(std::strcmp(static_cast<const char *>(memory.constData()) + 100, "answer") == 0);
    CHECK(memory.isAttached()); // the child detached; the segment is still here

    // Two processes count in the memory under the lock: nothing is lost
    {
        int *counter = static_cast<int *>(memory.data()) + 64;
        *counter = 0;
        const int rounds = 20000;
        std::fflush(stdout);
        const pid_t pid = ::fork();
        if (pid == 0) {
            QSharedMemory mine(key);
            if (!mine.attach())
                ::_exit(1);
            int *shared = static_cast<int *>(mine.data()) + 64;
            for (int i = 0; i < rounds; ++i) {
                if (!mine.lock())
                    ::_exit(2);
                *shared = *shared + 1;
                mine.unlock();
            }
            ::_exit(0);
        }
        for (int i = 0; i < rounds; ++i) {
            CHECK(second.lock());
            int *shared = static_cast<int *>(second.data()) + 64;
            *shared = *shared + 1;
            second.unlock();
        }
        int status = 0;
        CHECK(::waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
        CHECK(*counter == 2 * rounds);
    }
    // A process that ends with the lock does not keep it
    CHECK(inChild([&] {
              QSharedMemory mine(key);
              return mine.attach() && mine.lock() ? 0 : 1; // _exit() runs no destructor
          }) == 0);
    CHECK(memory.lock() && memory.unlock());

    // The segment stays while an object is attached, and goes with the last
    CHECK(memory.detach() && !memory.isAttached() && memory.data() == nullptr && memory.size() == 0);
    CHECK(!memory.detach() && memory.error() == QSharedMemory::LockError);
    CHECK(memory.attach() && std::strcmp(static_cast<const char *>(memory.constData()), "jello") == 0);
    CHECK(reader.detach() && second.detach() && memory.detach());
    CHECK(!memory.attach() && memory.error() == QSharedMemory::NotFound);
    CHECK(memory.create(100) && memory.size() == 100 && static_cast<const char *>(memory.constData())[0] == 0);

    // The destructor detaches, with the lock held or not; a new key detaches too
    {
        QSharedMemory scoped(key);
        CHECK(scoped.attach() && scoped.lock());
    }
    CHECK(memory.lock() && memory.unlock());
    {
        QSharedMemory scoped(key);
        CHECK(scoped.attach());
        CHECK(memory.detach());
        CHECK(memory.attach()); // the scoped object keeps the segment
        CHECK(memory.detach());
    }
    CHECK(!memory.attach() && memory.error() == QSharedMemory::NotFound);
    CHECK(memory.create(64));
    const QString otherKey = uniqueKey("memory-two");
    memory.setKey(otherKey);
    CHECK(!memory.isAttached() && memory.key() == otherKey && memory.error() == QSharedMemory::NoError);
    CHECK(!QSharedMemory(key).attach()); // the first segment went with setKey()

    // A native key is the name in the system
    const QString native = QString("/nt-test-native-") + QString::number(static_cast<qlonglong>(::getpid()));
    memory.setNativeKey(native);
    CHECK(memory.key().isEmpty() && memory.nativeKey() == native);
    CHECK(memory.create(10) && memory.size() == 10);
    QSharedMemory byName;
    byName.setNativeKey(native);
    CHECK(byName.attach() && byName.size() == 10 && byName.lock() && byName.unlock());
    CHECK(QFileInfo::exists(QString("/dev/shm") + native) || !QFileInfo::exists(QString("/dev/shm")));
    CHECK(byName.detach() && memory.detach());
    CHECK(!QFileInfo::exists(QString("/dev/shm") + native));

    // It is a QObject
    QObject parent;
    auto *child = new QSharedMemory(key, &parent);
    CHECK(child->parent() == &parent && std::string(child->metaObject()->className()) == "QSharedMemory");
    CHECK((new QSharedMemory(&parent))->parent() == &parent);
}

int main() {
    ::alarm(120); // a semaphore that is never released would wait for ever
    testLibrary();
    testSystemSemaphore();
    testSharedMemory();

    if (g_failures) {
        std::printf("%d IPC test check(s) failed\n", g_failures);
        return 1;
    }
    std::puts("all IPC tests passed");
    return 0;
}
