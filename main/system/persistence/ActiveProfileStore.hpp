#pragma once

#include "system/hardware/HardwareConfig.hpp"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace InertialSaber::System {

/**
 * @brief Persists the index of the active profile on the SD card.
 *
 * load() reads synchronously at boot; saveAsync() hands the write to a low-priority task, so
 * real-time callers never touch the SD card. Only the latest pending index is written.
 */
class ActiveProfileStore {
public:
    /** @param task Parameters of the writer task. */
    explicit ActiveProfileStore(const Hardware::TaskSpec& task);

    /** @brief Writes any pending index, then blocks until the task has exited. */
    ~ActiveProfileStore();

    ActiveProfileStore(const ActiveProfileStore&) = delete;
    ActiveProfileStore& operator=(const ActiveProfileStore&) = delete;

    /**
     * @brief Spawns the writer task.
     * @return ESP_OK; ESP_ERR_INVALID_STATE if already started; ESP_ERR_NO_MEM if the task
     *         cannot be created.
     */
    [[nodiscard]] esp_err_t start();

    /** @brief Reads the stored index; empty if the file is missing or unreadable. Blocking. */
    [[nodiscard]] std::optional<size_t> load() const;

    /** @brief Schedules @p index to be written, replacing any pending index; never blocks. */
    void saveAsync(size_t index);

private:
    static constexpr uint32_t kShutdownRequest = UINT32_MAX;

    static void taskEntry(void* arg);
    void run();
    void write(uint32_t index) const;

    const Hardware::TaskSpec m_taskSpec;

    StaticQueue_t m_queueControl{};
    std::array<uint8_t, sizeof(uint32_t)> m_queueStorage{};
    QueueHandle_t m_queue = nullptr;

    StaticSemaphore_t m_exitSemaphoreControl{};
    SemaphoreHandle_t m_exitSemaphore = nullptr;

    TaskHandle_t m_task = nullptr;
};

} // namespace InertialSaber::System
