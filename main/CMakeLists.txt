idf_component_register(
    SRCS "main.cpp" 
         "samples/helloworld/HelloWorldSample.cpp"
    INCLUDE_DIRS "." 
                 "samples"
                 "samples/helloworld"
)

# Force C++20 for all source files
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
