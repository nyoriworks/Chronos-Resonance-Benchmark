cmake_minimum_required(VERSION 3.15)
project(QuantumBenchmark)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags
if(MSVC)
    add_compile_options(/O2 /arch:AVX2)
else()
    add_compile_options(-O3 -march=native)
endif()

add_executable(quantum_benchmark quantum_benchmark.cpp)

# Platform-specific settings
if(WIN32)
    # Windows: Link against winmm for high-resolution timer
    target_link_libraries(quantum_benchmark winmm)
elseif(APPLE)
    # macOS: Link pthread
    target_link_libraries(quantum_benchmark pthread)
else()
    # Linux: Link pthread and rt
    target_link_libraries(quantum_benchmark pthread rt)
endif()

# Install
install(TARGETS quantum_benchmark DESTINATION bin)

# Testing
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    
    # Fetch Google Test
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    
    add_subdirectory(tests)
endif()
