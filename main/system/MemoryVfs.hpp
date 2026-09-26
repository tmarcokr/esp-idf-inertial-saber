#pragma once

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_vfs.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <vector>

namespace Espressif::Wrappers {

/** @brief Owning PSRAM (or any heap_caps) buffer served as a read-only file. */
struct MemoryFile {
    struct HeapCapsDeleter {
        void operator()(uint8_t* p) const { heap_caps_free(p); }
    };
    std::unique_ptr<uint8_t[], HeapCapsDeleter> bytes;
    size_t size = 0;
};

/** @brief Shared, read-only handle to a MemoryFile. */
using MemoryFileHandle = std::shared_ptr<const MemoryFile>;

/** @brief Virtual filesystem driver serving in-memory buffers as read-only files. Thread-safe; never call from an ISR. */
class MemoryVfs {
public:
    explicit MemoryVfs(std::string_view mountPoint = "/mem",
                       uint8_t maxFiles = 16,
                       uint8_t maxFds = 8);
    ~MemoryVfs();

    MemoryVfs(const MemoryVfs&) = delete;
    MemoryVfs& operator=(const MemoryVfs&) = delete;

    [[nodiscard]] esp_err_t init();

    /**
     * @brief Publishes @p file under @p name (relative to the mount point).
     * @return ESP_ERR_INVALID_SIZE if the name is already registered, ESP_ERR_NO_MEM if the table is full.
     */
    [[nodiscard]] esp_err_t registerFile(std::string_view name, MemoryFileHandle file);

    /** @brief Removes @p name from the file table; open descriptors keep the buffer alive. */
    esp_err_t unregisterFile(std::string_view name);

    [[nodiscard]] uint8_t openDescriptorCount() const;

private:
    struct FileEntry {
        std::string name;
        MemoryFileHandle file;
    };

    struct FdEntry {
        MemoryFileHandle file;
        size_t position = 0;
    };

    int vfsOpen(const char* path, int flags, int mode);
    ssize_t vfsRead(int fd, void* dst, size_t size);
    int vfsClose(int fd);
    off_t vfsLseek(int fd, off_t offset, int mode);
    int vfsFstat(int fd, struct stat* st);

    [[nodiscard]] esp_err_t insertFile(std::string_view name, MemoryFileHandle& file);
    [[nodiscard]] FdEntry* findDescriptor(int fd);

    std::string m_mountPoint;
    bool m_initialized = false;

    std::vector<FileEntry> m_files;
    std::vector<FdEntry> m_fds;
    // Warning: leaf lock. The VFS hooks run under newlib's FILE lock, so while holding m_mutex never
    // log, touch stdio, release the last MemoryFileHandle or take any lock other than the heap's.
    mutable std::mutex m_mutex;
    esp_vfs_t m_vfsImpl{};
};

} // namespace Espressif::Wrappers
