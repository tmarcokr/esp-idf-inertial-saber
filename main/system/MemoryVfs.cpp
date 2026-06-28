#include "system/MemoryVfs.hpp"
#include "esp_log.h"
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <algorithm>

static constexpr const char* TAG = "MemoryVfs";

namespace Espressif::Wrappers {

MemoryVfs::MemoryVfs(const char* mount_point, uint8_t max_files, uint8_t max_fds)
    : m_mountPoint(mount_point)
    , m_maxFiles(max_files)
    , m_maxFds(max_fds) {
    m_files = new FileEntry[m_maxFiles];
    m_fds = new FdEntry[m_maxFds];
}

MemoryVfs::~MemoryVfs() {
    if (m_initialized) {
        esp_vfs_unregister(m_mountPoint.c_str());
    }
    delete[] m_files;
    delete[] m_fds;
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

esp_err_t MemoryVfs::registerFile(const char* name, const uint8_t* data, size_t size) {
    if (!name || name[0] == '\0' || !data || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!m_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Check for duplicates
    for (uint8_t i = 0; i < m_maxFiles; ++i) {
        if (m_files[i].occupied && m_files[i].name == name) {
            return ESP_ERR_INVALID_SIZE; // Duplicate name
        }
    }

    // Find free slot
    for (uint8_t i = 0; i < m_maxFiles; ++i) {
        if (!m_files[i].occupied) {
            m_files[i].name = name;
            m_files[i].data = data;
            m_files[i].size = size;
            m_files[i].occupied = true;
            ESP_LOGD(TAG, "Registered virtual file '%s/%s' (%zu bytes)", m_mountPoint.c_str(), name, size);
            return ESP_OK;
        }
    }

    return ESP_ERR_NO_MEM;
}

esp_err_t MemoryVfs::unregisterFile(const char* name) {
    if (!name) return ESP_ERR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(m_mutex);

    for (uint8_t i = 0; i < m_maxFiles; ++i) {
        if (m_files[i].occupied && m_files[i].name == name) {
            m_files[i].occupied = false;
            m_files[i].name.clear();
            m_files[i].data = nullptr;
            m_files[i].size = 0;
            ESP_LOGD(TAG, "Unregistered virtual file '%s/%s'", m_mountPoint.c_str(), name);
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

void MemoryVfs::unregisterAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (uint8_t i = 0; i < m_maxFiles; ++i) {
        m_files[i].occupied = false;
        m_files[i].name.clear();
        m_files[i].data = nullptr;
        m_files[i].size = 0;
    }
    for (uint8_t i = 0; i < m_maxFds; ++i) {
        m_fds[i].occupied = false;
    }
    ESP_LOGD(TAG, "Unregistered all virtual files");
}

bool MemoryVfs::exists(const char* name) const {
    if (!name) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (uint8_t i = 0; i < m_maxFiles; ++i) {
        if (m_files[i].occupied && m_files[i].name == name) {
            return true;
        }
    }
    return false;
}

uint8_t MemoryVfs::fileCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint8_t count = 0;
    for (uint8_t i = 0; i < m_maxFiles; ++i) {
        if (m_files[i].occupied) count++;
    }
    return count;
}

int MemoryVfs::vfsOpen(const char* path, int flags, int mode) {
    // Read-only filesystem
    int acc_mode = flags & O_ACCMODE;
    if (acc_mode != O_RDONLY) {
        errno = EACCES;
        return -1;
    }

    // Strip leading slash if present
    const char* reqName = path;
    if (reqName[0] == '/') {
        reqName++;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Look up file
    int fileIdx = -1;
    for (uint8_t i = 0; i < m_maxFiles; ++i) {
        if (m_files[i].occupied && m_files[i].name == reqName) {
            fileIdx = i;
            break;
        }
    }

    if (fileIdx == -1) {
        errno = ENOENT;
        return -1;
    }

    // Find free fd
    for (uint8_t i = 0; i < m_maxFds; ++i) {
        if (!m_fds[i].occupied) {
            m_fds[i].file_index = fileIdx;
            m_fds[i].position = 0;
            m_fds[i].occupied = true;
            return i;
        }
    }

    errno = ENFILE;
    return -1;
}

ssize_t MemoryVfs::vfsRead(int fd, void* dst, size_t size) {
    if (fd < 0 || fd >= m_maxFds || !m_fds[fd].occupied) {
        errno = EBADF;
        return -1;
    }

    FdEntry& entry = m_fds[fd];
    FileEntry& file = m_files[entry.file_index];

    if (entry.position >= file.size) {
        return 0; // EOF
    }

    size_t bytes_to_copy = std::min(size, file.size - entry.position);
    if (bytes_to_copy > 0) {
        std::memcpy(dst, file.data + entry.position, bytes_to_copy);
        entry.position += bytes_to_copy;
    }

    return bytes_to_copy;
}

int MemoryVfs::vfsClose(int fd) {
    if (fd < 0 || fd >= m_maxFds || !m_fds[fd].occupied) {
        errno = EBADF;
        return -1;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_fds[fd].occupied = false;
    return 0;
}

off_t MemoryVfs::vfsLseek(int fd, off_t offset, int mode) {
    if (fd < 0 || fd >= m_maxFds || !m_fds[fd].occupied) {
        errno = EBADF;
        return -1;
    }

    FdEntry& entry = m_fds[fd];
    FileEntry& file = m_files[entry.file_index];

    off_t new_pos = 0;
    switch (mode) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = static_cast<off_t>(entry.position) + offset;
            break;
        case SEEK_END:
            new_pos = static_cast<off_t>(file.size) + offset;
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    if (new_pos < 0 || new_pos > static_cast<off_t>(file.size)) {
        errno = EINVAL;
        return -1;
    }

    entry.position = static_cast<size_t>(new_pos);
    return new_pos;
}

int MemoryVfs::vfsFstat(int fd, struct stat* st) {
    if (fd < 0 || fd >= m_maxFds || !m_fds[fd].occupied) {
        errno = EBADF;
        return -1;
    }

    FdEntry& entry = m_fds[fd];
    FileEntry& file = m_files[entry.file_index];

    std::memset(st, 0, sizeof(*st));
    st->st_size = file.size;
    st->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
    return 0;
}

} // namespace Espressif::Wrappers
