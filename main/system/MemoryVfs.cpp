#include "system/MemoryVfs.hpp"
#include "esp_log.h"
#include <fcntl.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <utility>

static constexpr const char* TAG = "MemoryVfs";

namespace Espressif::Wrappers {

MemoryVfs::MemoryVfs(std::string_view mountPoint, uint8_t maxFiles, uint8_t maxFds)
    : m_mountPoint(mountPoint)
    , m_files(maxFiles)
    , m_fds(maxFds) {}

MemoryVfs::~MemoryVfs() {
    if (m_initialized) {
        esp_vfs_unregister(m_mountPoint.c_str());
    }
}

esp_err_t MemoryVfs::init() {
    if (m_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    m_vfsImpl.flags = ESP_VFS_FLAG_CONTEXT_PTR;
    m_vfsImpl.open_p = [](void* ctx, const char* path, int flags, int mode) -> int {
        return static_cast<MemoryVfs*>(ctx)->vfsOpen(path, flags, mode);
    };
    m_vfsImpl.read_p = [](void* ctx, int fd, void* dst, size_t size) -> ssize_t {
        return static_cast<MemoryVfs*>(ctx)->vfsRead(fd, dst, size);
    };
    m_vfsImpl.close_p = [](void* ctx, int fd) -> int {
        return static_cast<MemoryVfs*>(ctx)->vfsClose(fd);
    };
    m_vfsImpl.lseek_p = [](void* ctx, int fd, off_t offset, int mode) -> off_t {
        return static_cast<MemoryVfs*>(ctx)->vfsLseek(fd, offset, mode);
    };
    m_vfsImpl.fstat_p = [](void* ctx, int fd, struct stat* st) -> int {
        return static_cast<MemoryVfs*>(ctx)->vfsFstat(fd, st);
    };

    esp_err_t err = esp_vfs_register(m_mountPoint.c_str(), &m_vfsImpl, this);
    if (err == ESP_OK) {
        m_initialized = true;
        ESP_LOGI(TAG, "Mounted MemoryVfs at '%s'", m_mountPoint.c_str());
    } else {
        ESP_LOGE(TAG, "Failed to register MemoryVfs (err=%s)", esp_err_to_name(err));
    }
    return err;
}

esp_err_t MemoryVfs::registerFile(std::string_view name, MemoryFileHandle file) {
    if (name.empty() || !file || !file->bytes || file->size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!m_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    const size_t size = file->size;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        esp_err_t err = insertFile(name, file);
        if (err != ESP_OK) {
            return err;
        }
    }
    ESP_LOGD(TAG, "Registered virtual file '%s/%.*s' (%zu bytes)", m_mountPoint.c_str(),
             static_cast<int>(name.size()), name.data(), size);
    return ESP_OK;
}

esp_err_t MemoryVfs::insertFile(std::string_view name, MemoryFileHandle& file) {
    const bool duplicate = std::any_of(m_files.begin(), m_files.end(), [name](const FileEntry& entry) {
        return entry.file && entry.name == name;
    });
    if (duplicate) {
        return ESP_ERR_INVALID_SIZE;
    }

    auto freeSlot = std::find_if(m_files.begin(), m_files.end(),
                                 [](const FileEntry& entry) { return !entry.file; });
    if (freeSlot == m_files.end()) {
        return ESP_ERR_NO_MEM;
    }

    freeSlot->name.assign(name);
    freeSlot->file = std::move(file);
    return ESP_OK;
}

esp_err_t MemoryVfs::unregisterFile(std::string_view name) {
    MemoryFileHandle released;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto entry = std::find_if(m_files.begin(), m_files.end(), [name](const FileEntry& e) {
            return e.file && e.name == name;
        });
        if (entry == m_files.end()) {
            return ESP_ERR_NOT_FOUND;
        }
        released = std::move(entry->file);
        entry->name.clear();
    }
    ESP_LOGD(TAG, "Unregistered virtual file '%s/%.*s'", m_mountPoint.c_str(),
             static_cast<int>(name.size()), name.data());
    return ESP_OK;
}

uint8_t MemoryVfs::openDescriptorCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<uint8_t>(std::count_if(m_fds.begin(), m_fds.end(),
                                              [](const FdEntry& fd) { return fd.file != nullptr; }));
}

MemoryVfs::FdEntry* MemoryVfs::findDescriptor(int fd) {
    if (fd < 0 || static_cast<size_t>(fd) >= m_fds.size() || !m_fds[fd].file) {
        return nullptr;
    }
    return &m_fds[fd];
}

int MemoryVfs::vfsOpen(const char* path, int flags, int /*mode*/) {
    if ((flags & O_ACCMODE) != O_RDONLY) {
        errno = EACCES;
        return -1;
    }

    std::string_view requested(path);
    if (!requested.empty() && requested.front() == '/') {
        requested.remove_prefix(1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    auto file = std::find_if(m_files.begin(), m_files.end(), [requested](const FileEntry& entry) {
        return entry.file && entry.name == requested;
    });
    if (file == m_files.end()) {
        errno = ENOENT;
        return -1;
    }

    auto freeFd = std::find_if(m_fds.begin(), m_fds.end(),
                               [](const FdEntry& fd) { return !fd.file; });
    if (freeFd == m_fds.end()) {
        errno = ENFILE;
        return -1;
    }

    freeFd->file = file->file;
    freeFd->position = 0;
    return static_cast<int>(std::distance(m_fds.begin(), freeFd));
}

ssize_t MemoryVfs::vfsRead(int fd, void* dst, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);

    FdEntry* entry = findDescriptor(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    const MemoryFile& file = *entry->file;
    if (entry->position >= file.size) {
        return 0;
    }

    const size_t bytesToCopy = std::min(size, file.size - entry->position);
    std::memcpy(dst, file.bytes.get() + entry->position, bytesToCopy);
    entry->position += bytesToCopy;
    return static_cast<ssize_t>(bytesToCopy);
}

int MemoryVfs::vfsClose(int fd) {
    MemoryFileHandle released;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        FdEntry* entry = findDescriptor(fd);
        if (!entry) {
            errno = EBADF;
            return -1;
        }
        released = std::move(entry->file);
        entry->position = 0;
    }
    return 0;
}

off_t MemoryVfs::vfsLseek(int fd, off_t offset, int mode) {
    std::lock_guard<std::mutex> lock(m_mutex);

    FdEntry* entry = findDescriptor(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    const auto fileSize = static_cast<off_t>(entry->file->size);
    off_t newPosition = 0;
    switch (mode) {
        case SEEK_SET:
            newPosition = offset;
            break;
        case SEEK_CUR:
            newPosition = static_cast<off_t>(entry->position) + offset;
            break;
        case SEEK_END:
            newPosition = fileSize + offset;
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    if (newPosition < 0 || newPosition > fileSize) {
        errno = EINVAL;
        return -1;
    }

    entry->position = static_cast<size_t>(newPosition);
    return newPosition;
}

int MemoryVfs::vfsFstat(int fd, struct stat* st) {
    std::lock_guard<std::mutex> lock(m_mutex);

    FdEntry* entry = findDescriptor(fd);
    if (!entry) {
        errno = EBADF;
        return -1;
    }

    std::memset(st, 0, sizeof(*st));
    st->st_size = static_cast<off_t>(entry->file->size);
    st->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
    return 0;
}

} // namespace Espressif::Wrappers
