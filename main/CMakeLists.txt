idf_component_register(
    SRCS "main.cpp"
         "system/SaberSystem.cpp"
         "system/adapters/ImuAdapter.cpp"
         "system/adapters/InputAdapter.cpp"
         "system/hardware/SdHardware.cpp"
         "system/hardware/AudioHardware.cpp"
         "system/hardware/LedHardware.cpp"
         "system/hardware/ImuHardware.cpp"
         "system/hardware/ButtonHardware.cpp"
         "core/bus/SaberActionBus.cpp"
         "core/effects/inertial_engine/audio/SwingSwapper.cpp"
         "core/effects/inertial_engine/audio/InertialSwingEffect.cpp"
         "core/effects/inertial_engine/visual/InertialLightEffect.cpp"
         "core/effects/inertial_engine/audio/AudioPathProvider.cpp"
         "profiles/inertial/InertialDefaultProfile.cpp"
         "profiles/inertial/effects/BladeIgniteSweep.cpp"
         "profiles/inertial/effects/BladeRetractSweep.cpp"
         "profiles/inertial/effects/PowerToggleEffect.cpp"
    INCLUDE_DIRS "." "core" "core/effects" "core/effects/inertial_engine/audio" "core/effects/inertial_engine/visual" "profiles/inertial/effects" "profiles"
    REQUIRES mpu6050 gpio_button audio_engine audio_channel i2s_transmitter sd_card smart_led
)

# Force C++20 for all source files
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
