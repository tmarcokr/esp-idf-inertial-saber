#pragma once

#include "core/EffectSet.hpp"
#include "profiles/ConfigurableProfile.hpp"
#include "profiles/PowerStateMachine.hpp"
#include "profiles/SaberServices.hpp"
#include "system/hardware/HardwareConfig.hpp"
#include "system/persistence/ActiveProfileStore.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace InertialSaber::Profiles {

/**
 * @brief Active object that owns the profiles and performs runtime profile switches off the bus.
 *
 * The bus only posts a request (requestNext()); the profile_ctrl task builds the next effect
 * set, requests its preload, stages it on the bus and destroys the set it replaces. The new
 * active index is saved on profile_ctrl no earlier than kSaveDelayMs after the last switch, and
 * only when a check finds the preload finished and the blade retracted or faulted. A check that
 * finds the blade lit postpones the save to kSaveDelayMs after a later check finds it idle again.
 *
 * Thread safety:
 *   - init(), loadActive(), start() and stop(): main task only.
 *   - requestNext(): bus task only.
 *   - switchPending() and savePending(): any task.
 */
class ProfileManager {
public:
    /**
     * @param services Services wired into every built effect set.
     * @param store Persistence of the active profile index.
     * @param task Creation parameters of the profile_ctrl task.
     */
    ProfileManager(const SaberServices& services, System::ActiveProfileStore& store,
                   const System::Hardware::TaskSpec& task);

    /** @brief Joins the profile_ctrl task (see stop()). */
    ~ProfileManager();

    ProfileManager(const ProfileManager&) = delete;
    ProfileManager& operator=(const ProfileManager&) = delete;

    /**
     * @brief Discovers and initializes profiles from the SD card.
     * @return ESP_OK if at least one valid profile was loaded, otherwise the scan error or ESP_ERR_NOT_FOUND.
     */
    [[nodiscard]] esp_err_t init();

    /**
     * @brief Builds the active profile's effect set, requests its preload and installs it on the bus.
     *
     * Bus stopped only.
     * @return ESP_OK; ESP_ERR_INVALID_STATE if no profile is available; ESP_FAIL if an effect
     *         was rejected.
     */
    [[nodiscard]] esp_err_t loadActive();

    /**
     * @brief Spawns the profile_ctrl task; call after the bus has started.
     * @return ESP_OK; ESP_ERR_INVALID_STATE if already started; ESP_ERR_NO_MEM if the task
     *         cannot be created.
     */
    [[nodiscard]] esp_err_t start();

    /**
     * @brief Signals the profile_ctrl task to exit and joins it.
     *
     * Idempotent. Must not be called from the profile_ctrl task.
     */
    void stop();

    /**
     * @brief Posts a switch to the next profile; with a single profile, the profile reloads itself.
     *
     * Bus task only: O(1), no allocation, no log above DEBUG. Ignored while a switch is pending.
     */
    void requestNext();

    /** @brief True from requestNext() until the replaced set has been destroyed. Any task. */
    [[nodiscard]] bool switchPending() const;

    /** @brief True while a save of the active index is armed or being written. Any task. */
    [[nodiscard]] bool savePending() const;

private:
    static constexpr uint32_t kSaveDelayMs = 1500;
    static constexpr uint32_t kSavePollMs = 100;
    static constexpr uint32_t kCommitPollMs = 2;
    static constexpr uint32_t kCommitWarnMs = 1000;

    static void taskEntry(void* arg);
    void run();
    void performSwitch();
    [[nodiscard]] std::unique_ptr<Core::EffectSet> waitForRetiredEffects();
    void armSave(int64_t nowUs);
    void serviceSave(int64_t nowUs);
    [[nodiscard]] TickType_t nextWakeTicks(int64_t nowUs) const;
    void logActiveProfile() const;

    const SaberServices& m_services;
    System::ActiveProfileStore& m_store;
    const System::Hardware::TaskSpec m_taskSpec;
    std::vector<std::unique_ptr<ConfigurableProfile>> m_profiles;
    size_t m_activeIndex = 0;
    const PowerStateMachine* m_activePower = nullptr;

    std::atomic<TaskHandle_t> m_task{nullptr};
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_switchRequested{false};
    std::atomic<bool> m_switchPending{false};
    std::atomic<uint32_t> m_requestedAtUs{0};
    StaticSemaphore_t m_exitSemaphoreControl{};
    SemaphoreHandle_t m_exitSemaphore = nullptr;

    std::optional<size_t> m_savedIndex;
    std::atomic<bool> m_saveArmed{false};
    bool m_saveWaitingForIdle = false;
    int64_t m_saveDueUs = 0;
};

} // namespace InertialSaber::Profiles
