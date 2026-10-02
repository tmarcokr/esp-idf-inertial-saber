idf_component_register(
    SRCS "main.cpp"
         "system/SaberSystem.cpp"
         "system/adapters/ImuAdapter.cpp"
         "system/adapters/InputAdapter.cpp"
         "system/status/RgbStatusIndicator.cpp"
         "system/MemoryVfs.cpp"
         "system/PsramAudioCache.cpp"
         "system/audio/AudioController.cpp"
         "system/persistence/ActiveProfileStore.cpp"
         "core/SaberActionBus.cpp"
         "profiles/inertial/effects/InertialSwingEffect.cpp"
         "profiles/inertial/effects/InertialLightEffect.cpp"
         "profiles/ConfigurableProfile.cpp"
         "profiles/ProfileParser.cpp"
         "profiles/SoundFont.cpp"
         "profiles/PowerStateMachine.cpp"
         "profiles/ProfileLoader.cpp"
         "profiles/ProfileManager.cpp"
         "profiles/inertial/effects/overlays/BladeIgniteSweep.cpp"
         "profiles/inertial/effects/overlays/BladeRetractSweep.cpp"
         "profiles/inertial/effects/PowerToggleEffect.cpp"
         "profiles/inertial/effects/overlays/BladeBlasterBlock.cpp"
         "profiles/inertial/effects/BlasterEffect.cpp"
         "profiles/inertial/effects/overlays/BladeClashFlash.cpp"
         "profiles/inertial/effects/KineticImpactEffect.cpp"
         "profiles/inertial/effects/overlays/BladeDragEffect.cpp"
         "profiles/inertial/effects/DragEffect.cpp"
         "profiles/inertial/effects/ProfileCycleEffect.cpp"
         "profiles/inertial/effects/PreloadWaitEffect.cpp"
         "diagnostics/Metrics.cpp"
         "system/metrics/MetricsReporter.cpp"
         "system/metrics/MetricsAllocHook.cpp"
    INCLUDE_DIRS "." "core" "profiles/inertial/effects" "profiles/inertial/effects/overlays" "profiles"
    REQUIRES mpu6050 gpio_button audio_engine audio_channel i2s_transmitter sd_card smart_led espressif__cjson rgb_led vfs esp_driver_gpio esp_timer esp_app_format
)

# Force C++20 for all source files
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)

if(CONFIG_SABER_METRICS AND CONFIG_HEAP_USE_HOOKS)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-u esp_heap_trace_alloc_hook")
endif()

if(NOT CONFIG_IDF_TARGET_ESP32S3)
    message(FATAL_ERROR "InertialSaber only supports the esp32s3 target. Run: idf.py set-target esp32s3")
endif()
