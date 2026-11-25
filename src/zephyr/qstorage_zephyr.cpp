// MIT License - see LICENSE
//
// CONFIG_NT_STORAGE: mounts LittleFS on the flash partition labelled storage_partition at CONFIG_NT_HOME_PATH, before
// main() runs, so that QFile, QSettings and QStandardPaths have a file system that survives a reboot. A partition
// that holds no LittleFS yet, or a damaged one, is formatted. A file system that the devicetree's fstab already
// mounted there is left as it is.
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/init.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/printk.h>

#include <cerrno>

#if !PARTITION_EXISTS(storage_partition)
#error "CONFIG_NT_STORAGE needs a flash partition labelled storage_partition in the devicetree"
#endif

namespace {

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(ntStorage);

fs_mount_t ntMount;

} // namespace

extern "C" int nt_storage_mount();

// Nothing in Nt calls this function, so the build names it to the linker (-Wl,--undefined), which then keeps this
// object file of the static library and with it the entry of SYS_INIT.
extern "C" int nt_storage_mount() {
    ntMount.type = FS_LITTLEFS;
    ntMount.mnt_point = CONFIG_NT_HOME_PATH;
    ntMount.fs_data = &ntStorage;
    ntMount.storage_dev = reinterpret_cast<void *>(static_cast<uintptr_t>(PARTITION_ID(storage_partition)));
    const int rc = fs_mount(&ntMount);
    if (rc < 0 && rc != -EBUSY) {
        printk("Nt: cannot mount LittleFS at %s: %d\n", CONFIG_NT_HOME_PATH, rc);
        return 0; // the application still starts, without persistent files
    }
    // QDir::tempPath() is a directory of the home directory on Zephyr.
    fs_mkdir(CONFIG_NT_HOME_PATH "/tmp");
    return 0;
}

SYS_INIT(nt_storage_mount, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
