cmake_minimum_required(VERSION 4.0)
project(wavex VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# test trigger
option(WAVEX_TEST "Build tests for the wavex library" OFF)

# Locate External Dependencies First
find_package(asio CONFIG REQUIRED)

# Define the Library Target
add_library(wavex src/wavex.cpp)

# Configure Include Paths (Combined and cleaned up)
target_include_directories(wavex PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link Dependencies to the Target (Moved before installation rules)
target_link_libraries(wavex PUBLIC
        asio::asio
)

# Attach C++ Modules
target_sources(wavex
        PUBLIC
        FILE_SET wavex_modules TYPE CXX_MODULES
        BASE_DIRS src
        FILES src/wavex.ixx
)

# Install Targets and Module File Sets
install(TARGETS wavex
        EXPORT WaveXTargets
        FILE_SET wavex_modules DESTINATION include/wavex/modules
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)

# Install Traditional Public Header Files
install(DIRECTORY include/ DESTINATION include)

# Export Targets Under the Custom Namespace (Unified paths to share/cmake/wavex)
install(EXPORT WaveXTargets
        FILE WaveXTargets.cmake
        NAMESPACE wavex::
        DESTINATION share/cmake/wavex
)

# Generate and Install Master Package Configurations (Unified paths to share/cmake/wavex)
include(CMakePackageConfigHelpers)
configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/CMake/WaveXConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/WaveXConfig.cmake"
        INSTALL_DESTINATION share/cmake/wavex
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/WaveXConfig.cmake" DESTINATION share/cmake/wavex)


if(WAVEX_TEST)
    enable_testing()

    # Define a test executable (Add your test framework files here, e.g., Catch2, GTest, or raw C++)
    add_executable(wavex_tests tests/test1.cpp)

    # Link against your exported alias/target to test module & header visibility
    target_link_libraries(wavex_tests PRIVATE wavex)

    # Register the binary with CTest
    add_test(NAME CoreLibraryTests COMMAND wavex_tests)
endif()