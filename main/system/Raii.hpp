// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <dirent.h>

#include <cstdio>
#include <memory>

namespace InertialSaber::System {

struct FileCloser {
    void operator()(std::FILE* file) const { std::fclose(file); }
};

struct DirCloser {
    void operator()(DIR* dir) const { closedir(dir); }
};

/** @brief Owning stdio stream; closed on destruction. */
using UniqueFile = std::unique_ptr<std::FILE, FileCloser>;

/** @brief Owning directory stream; closed on destruction. */
using UniqueDir = std::unique_ptr<DIR, DirCloser>;

/** @brief std::fopen() wrapper; returns null on failure with errno set. */
[[nodiscard]] inline UniqueFile openFile(const char* path, const char* mode) {
    return UniqueFile(std::fopen(path, mode));
}

/** @brief Closes @p file now and reports the result, which the destructor would discard (e.g. write-back errors). */
[[nodiscard]] inline bool closeFile(UniqueFile file) {
    return file && std::fclose(file.release()) == 0;
}

/** @brief opendir() wrapper; returns null on failure with errno set. */
[[nodiscard]] inline UniqueDir openDir(const char* path) {
    return UniqueDir(opendir(path));
}

} // namespace InertialSaber::System
