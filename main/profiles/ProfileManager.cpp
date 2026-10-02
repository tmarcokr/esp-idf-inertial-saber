#include "profiles/ProfileManager.hpp"
#include "profiles/ProfileLoader.hpp"
#include "diagnostics/Metrics.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include <optional>
#include <utility>

namespace InertialSaber::Profiles {

static constexpr const char* TAG = "ProfileManager";

ProfileManager::ProfileManager(const SaberServices& services, System::ActiveProfileStore& store,
                               const System::Hardware::TaskSpec& task)
    : m_services(services)
    , m_store(store)
    , m_taskSpec(task)
    , m_exitSemaphore(xSemaphoreCreateBinaryStatic(&m_exitSemaphoreControl)) {}

ProfileManager::~ProfileManager() {
    stop();
    vSemaphoreDelete(m_exitSemaphore);
}

esp_err_t ProfileManager::init() {
    ESP_LOGI(TAG, "Initializing profiles...");

    const esp_err_t err = ProfileLoader::loadFromSd(m_profiles);

    if (m_profiles.empty()) {
        ESP_LOGE(TAG, "No valid profile on SD (scan: %s)", esp_err_to_name(err));
        ESP_LOGE(TAG, "Each profile needs /sdcard/profiles/<name>/profile.json");
        return err != ESP_OK ? err : ESP_ERR_NOT_FOUND;
    }

    m_activeIndex = 0;
    if (const std::optional<size_t> storedIndex = m_store.load(); storedIndex.has_value()) {
        if (*storedIndex < m_profiles.size()) {
            m_activeIndex = *storedIndex;
            ESP_LOGI(TAG, "Restored active profile index: %u",
                     static_cast<unsigned>(m_activeIndex));
        } else {
            ESP_LOGW(TAG, "Loaded active index %u out of bounds (%u profiles). Resetting to 0.",
                     static_cast<unsigned>(*storedIndex), static_cast<unsigned>(m_profiles.size()));
        }
    }
    ESP_LOGI(TAG, "Initialized %u profile(s), active index: %u",
             static_cast<unsigned>(m_profiles.size()), static_cast<unsigned>(m_activeIndex));
    return ESP_OK;
}

esp_err_t ProfileManager::loadActive() {
    if (m_profiles.empty()) return ESP_ERR_INVALID_STATE;

    const ConfigurableProfile& profile = *m_profiles[m_activeIndex];
    ProfileEffects built = profile.buildEffects(m_services, *this, std::nullopt);
    if (!built.set) {
        ESP_LOGE(TAG, "Effect set of profile '%s' is incomplete",
                 profile.definition().profileName.c_str());
        return ESP_FAIL;
    }
    m_services.audioCache.requestPreload(profile.font());
    m_services.bus.installEffects(std::move(built.set));
    m_activePower = built.power;
    logActiveProfile();
    return ESP_OK;
}

esp_err_t ProfileManager::start() {
    if (m_task.load() != nullptr) return ESP_ERR_INVALID_STATE;

    m_stopRequested.store(false);
    TaskHandle_t handle = nullptr;
    if (xTaskCreatePinnedToCore(&ProfileManager::taskEntry, m_taskSpec.name, m_taskSpec.stackSize,
                                this, m_taskSpec.priority, &handle, m_taskSpec.core) != pdPASS) {
        ESP_LOGE(TAG, "%s task creation failed", m_taskSpec.name);
        return ESP_ERR_NO_MEM;
    }
    m_task.store(handle);
    SABER_METRIC_REGISTER_TASK(Diagnostics::TaskId::ProfileControl, handle);
    return ESP_OK;
}

void ProfileManager::stop() {
    const TaskHandle_t handle = m_task.load();
    if (handle == nullptr) {
        return;
    }
    configASSERT(xTaskGetCurrentTaskHandle() != handle);

    m_stopRequested.store(true);
    xTaskNotifyGive(handle);
    xSemaphoreTake(m_exitSemaphore, portMAX_DELAY);
    m_task.store(nullptr);
}

void ProfileManager::requestNext() {
    if (m_switchPending.load()) return;

    m_requestedAtUs.store(static_cast<uint32_t>(esp_timer_get_time()), std::memory_order_relaxed);
    m_switchPending.store(true);
    m_switchRequested.store(true);
    if (const TaskHandle_t task = m_task.load(); task != nullptr) {
        xTaskNotifyGive(task);
    }
}

bool ProfileManager::switchPending() const {
    return m_switchPending.load(std::memory_order_acquire);
}

void ProfileManager::taskEntry(void* arg) {
    auto* manager = static_cast<ProfileManager*>(arg);
    manager->run();
    xSemaphoreGive(manager->m_exitSemaphore);
    vTaskDelete(nullptr);
}

void ProfileManager::run() {
    // Warning: seq_cst Dekker pair with requestNext(); publishing the handle before reading the
    // request flag guarantees a request posted before start() returned is not lost.
    m_task.store(xTaskGetCurrentTaskHandle());

    while (!m_stopRequested.load()) {
        if (m_switchRequested.exchange(false)) {
            performSwitch();
            continue;
        }
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

void ProfileManager::performSwitch() {
    const size_t previousIndex = m_activeIndex;
    const size_t nextIndex = (m_activeIndex + 1) % m_profiles.size();
    const ConfigurableProfile& profile = *m_profiles[nextIndex];

    ProfileEffects built =
        profile.buildEffects(m_services, *this, m_requestedAtUs.load(std::memory_order_relaxed));
    configASSERT(built.set != nullptr);

    m_services.audioCache.requestPreload(profile.font());

    [[maybe_unused]] const bool staged = m_services.bus.stageEffects(std::move(built.set));
    configASSERT(staged);

    std::unique_ptr<Core::EffectSet> retired = waitForRetiredEffects();
    if (!retired) {
        return;
    }
    configASSERT(m_activePower->state() == PowerStateMachine::State::Switching);
    retired.reset();

    m_activeIndex = nextIndex;
    m_activePower = built.power;
    m_switchPending.store(false, std::memory_order_release);

    logActiveProfile();
    if (m_activeIndex != previousIndex) {
        m_store.saveAsync(m_activeIndex);
    }
}

std::unique_ptr<Core::EffectSet> ProfileManager::waitForRetiredEffects() {
    const int64_t startUs = esp_timer_get_time();
    bool warned = false;
    while (!m_stopRequested.load()) {
        if (std::unique_ptr<Core::EffectSet> retired = m_services.bus.takeRetiredEffects()) {
            return retired;
        }
        if (!warned && esp_timer_get_time() - startUs >= int64_t{kCommitWarnMs} * 1000) {
            ESP_LOGW(TAG, "Staged effect set not committed after %lu ms",
                     static_cast<unsigned long>(kCommitWarnMs));
            warned = true;
        }
        vTaskDelay(pdMS_TO_TICKS(kCommitPollMs));
    }
    return nullptr;
}

void ProfileManager::logActiveProfile() const {
    ESP_LOGI(TAG, "Active profile %u/%u: %s", static_cast<unsigned>(m_activeIndex + 1),
             static_cast<unsigned>(m_profiles.size()),
             m_profiles[m_activeIndex]->definition().profileName.c_str());
}

} // namespace InertialSaber::Profiles
