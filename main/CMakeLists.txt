idf_component_register(
    SRCS "main.cpp"
         "SaberSystem.cpp"
         "core/SaberActionBus.cpp"
    INCLUDE_DIRS "." "core" "effects"
    REQUIRES mpu6050 gpio_button
)

# Force C++20 for all source files
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
