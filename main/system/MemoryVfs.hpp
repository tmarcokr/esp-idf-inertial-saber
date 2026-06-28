#pragma once

#include "esp_err.h"
#include "esp_vfs.h"
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <sys/stat.h>

namespace Espressif::Wrappers {

/**
 * @brief Virtual filesystem driver serving in-memory buffers as read-only files.
 */
class MemoryVfs {
public:
    explicit MemoryVfs(const char* mount_point = "/mem",
                       uint8_t max_files = 16,
                       uint8_t max_fds = 8);
    ~MemoryVfs();

    MemoryVfs(const MemoryVfs&) = delete;
    MemoryVfs& operator=(const MemoryVfs&) = delete;

    [[nodiscard]] esp_err_t init();
    [[nodiscard]] esp_err_t registerFile(const char* name,
                                         const uint8_t* data,
                                         size_t size);
    esp_err_t unregisterFile(const char* name);
    void unregisterAll();
    [[nodiscard]] bool exists(const char* name) const;
    [[nodiscard]] uint8_t fileCount() const;

    // VFS Operations Delegation
    int vfsOpen(const char* path, int flags, int mode);
    ssize_t vfsRead(int fd, void* dst, size_t size);
    int vfsClose(int fd);
    off_t vfsLseek(int fd, off_t offset, int mode);
    int vfsFstat(int fd, struct stat* st);

private:
    struct FileEntry {
        std::string name;
        const uint8_t* data = nullptr;
        size_t size = 0;
        bool occupied = false;
    };

    struct FdEntry {
        uint8_t file_index = 0;
        size_t position = 0;
        bool occupied = false;
    };

    std::string m_mountPoint;
    uint8_t m_maxFiles;
    uint8_t m_maxFds;
    bool m_initialized = false;

    FileEntry* m_files = nullptr;
    FdEntry* m_fds = nullptr;
    mutable std::mutex m_mutex;
    esp_vfs_t m_vfsImpl{};
};

} // namespace Espressif::Wrappers
