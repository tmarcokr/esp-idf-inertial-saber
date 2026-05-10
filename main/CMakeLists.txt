idf_component_register(
    SRCS "main.cpp"
         "SaberSystem.cpp"
         "core/SaberActionBus.cpp"
         "effects/InertialSwingEffect.cpp"
         "effects/InertialLightEffect.cpp"
    INCLUDE_DIRS "." "core" "effects"
    REQUIRES mpu6050 gpio_button audio_engine audio_channel i2s_transmitter sd_card smart_led
)

# Force C++20 for all source files
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
