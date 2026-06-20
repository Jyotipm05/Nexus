cmake_minimum_required(VERSION 4.0)
project(wavex VERSION 1.0.0 LANGUAGES CXX)

# ===========================================================================
#  Compiler & Tooling Setup
# ===========================================================================
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Required for Clangd
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Override MSVC's -external:I flag → plain -I so compile_commands.json uses
# standard flags that clangd can parse. vcpkg passes all system includes
# (asio, nlohmann, re2, OpenSSL) via -external:I which clangd silently ignores.
#if (MSVC)
#    set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-I ")
#endif ()

option(WAVEX_TEST "Build tests for the wavex library" OFF)

include(FetchContent)

# ===========================================================================
#  Dependencies (Vcpkg First, Git Fallback Second)
# ===========================================================================

# ---------------------------------------------------------------------------
# asio
# ---------------------------------------------------------------------------
find_package(asio CONFIG QUIET)

if (NOT asio_FOUND)
    message(STATUS "asio not found via find_package — fetching and patching like vcpkg")

    FetchContent_Declare(
            asio
            GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
            GIT_TAG asio-1-32-0
            GIT_SHALLOW TRUE
    )

    FetchContent_GetProperties(asio)
    if (NOT asio_POPULATED)
        FetchContent_Populate(asio)

        # Apply vcpkg's exact source-code patch
        set(ASIO_CONFIG_HPP "${asio_SOURCE_DIR}/asio/include/asio/detail/config.hpp")
        file(READ "${ASIO_CONFIG_HPP}" ASIO_CONFIG_CONTENTS)
        string(REPLACE
                "defined(ASIO_STANDALONE)"
                "!defined(WAVEX_DISABLE_ASIO_STANDALONE)"
                ASIO_CONFIG_CONTENTS "${ASIO_CONFIG_CONTENTS}")
        file(WRITE "${ASIO_CONFIG_HPP}" "${ASIO_CONFIG_CONTENTS}")

        # Recreate vcpkg's exact CMake target
        add_library(asio::asio INTERFACE IMPORTED GLOBAL)
        target_include_directories(asio::asio INTERFACE "${asio_SOURCE_DIR}/asio/include")

        find_package(Threads REQUIRED)
        target_link_libraries(asio::asio INTERFACE Threads::Threads)
    endif ()
endif ()

# ---------------------------------------------------------------------------
# nlohmann-json
# ---------------------------------------------------------------------------
find_package(nlohmann_json CONFIG QUIET)
if (NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found via find_package — fetching from GitHub")
    FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.3 GIT_SHALLOW TRUE)
    set(JSON_BuildTests OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(nlohmann_json)
endif ()

# ---------------------------------------------------------------------------
# Google RE2 & Abseil (RE2 strictly requires Abseil)
# ---------------------------------------------------------------------------
find_package(re2 CONFIG QUIET)

if (NOT re2_FOUND)
    message(STATUS "re2 not found via find_package — fetching from GitHub")

    # 1. Fetch Abseil first if we don't have it
    find_package(absl CONFIG QUIET)
    if (NOT absl_FOUND)
        message(STATUS "absl not found — fetching from GitHub (Required by RE2)")
        FetchContent_Declare(
                absl
                GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
                GIT_TAG 20240722.0 # Stable LTS
                GIT_SHALLOW TRUE
        )
        set(ABSL_BUILD_TESTING OFF CACHE INTERNAL "")
        set(ABSL_USE_GOOGLETEST_HEAD OFF CACHE INTERNAL "")
        FetchContent_MakeAvailable(absl)
    endif ()

    # 2. Fetch RE2
    set(RE2_BUILD_TESTING OFF CACHE INTERNAL "")
    FetchContent_Declare(
            re2
            GIT_REPOSITORY https://github.com/google/re2.git
            GIT_TAG 2024-07-02
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(re2)
endif ()

# ---------------------------------------------------------------------------
# OpenSSL
# ---------------------------------------------------------------------------
find_package(OpenSSL QUIET)
if (OpenSSL_FOUND)
    message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
    set(WAVEX_HAS_SSL ON)
else ()
    message(WARNING "OpenSSL not found — TLS features will be compiled out.")
    set(WAVEX_HAS_SSL OFF)
endif ()

# ===========================================================================
#  Library Target
# ===========================================================================
add_library(wavex
        src/wavex.cpp
        src/protos/protos.cpp
        src/Base/Logger.cpp
        src/Base/Uri.cpp
        src/Base/Url.cpp
        src/Engine/Router.cpp
)

target_include_directories(wavex PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Link Dependencies
target_link_libraries(wavex PUBLIC
        asio::asio
        nlohmann_json::nlohmann_json
        re2::re2
)

target_compile_definitions(wavex PUBLIC ASIO_HAS_CO_AWAIT=1 ASIO_STANDALONE=1)

if (WIN32)
    target_compile_definitions(wavex PUBLIC _WIN32_WINNT=0x0A00)
endif ()

if (WAVEX_HAS_SSL)
    target_link_libraries(wavex PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(wavex PUBLIC WAVEX_HAS_SSL=1)
else ()
    target_compile_definitions(wavex PUBLIC WAVEX_HAS_SSL=0)
endif ()

# ===========================================================================
#  Clangd Header Registration
# ===========================================================================
# NOTE: Add any new headers here so Clangd correctly parses them!
target_sources(wavex
        PUBLIC FILE_SET public_headers TYPE HEADERS
        BASE_DIRS include
        FILES
        include/wavex/Base/MiddleWare.hpp
        include/wavex/Engine/Router.hpp
        include/wavex/Engine/HttpRouter.hpp
        include/wavex/protos/http/Methods.hpp
        include/wavex/protos/http/http1codec.hpp
        include/wavex/protos/http/HttpRequest.hpp
        include/wavex/protos/http/HttpResponse.hpp
        include/wavex/Server/WorkStealingQueue.hpp
        include/wavex/Server/ThreadPool.hpp
        include/wavex/Server/Server.hpp
)

# C++ Modules
target_sources(wavex
        PUBLIC FILE_SET wavex_modules TYPE CXX_MODULES
        BASE_DIRS src
        FILES
        src/wavex.ixx
        src/protos/protos.ixx
        src/protos/http/http.ixx
        src/protos/http/Methods.ixx
        src/protos/http/http1codec.ixx
        src/protos/http/HttpRequest.ixx
        src/protos/http/HttpResponse.ixx
        src/Engine/Router.ixx
        src/Engine/HttpRouter.ixx
        src/Server/Server.ixx
        src/Server/WorkStealingQueue.ixx
        src/Server/ThreadPool.ixx
        src/Base/MiddleWare.ixx
)

# ===========================================================================
#  Install Rules
# ===========================================================================
install(TARGETS wavex EXPORT WaveXTargets
        FILE_SET wavex_modules DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wavex/modules"
        FILE_SET public_headers DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

# If we used the Git fallback, we must ship the Asio headers to downstream consumers
if (NOT asio_FOUND)
    install(DIRECTORY "${asio_SOURCE_DIR}/asio/include/asio" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" FILES_MATCHING PATTERN "*.hpp" PATTERN "*.ipp")
    install(FILES "${asio_SOURCE_DIR}/asio/include/asio.hpp" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
endif ()

install(EXPORT WaveXTargets FILE WaveXTargets.cmake NAMESPACE wavex:: DESTINATION "${CMAKE_INSTALL_DATADIR}/cmake/wavex")

include(CMakePackageConfigHelpers)
configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/WaveXConfig.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/WaveXConfig.cmake" INSTALL_DESTINATION "${CMAKE_INSTALL_DATADIR}/cmake/wavex")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/WaveXConfig.cmake" DESTINATION "${CMAKE_INSTALL_DATADIR}/cmake/wavex")

# ===========================================================================
#  Quality of Life
# ===========================================================================
# Auto-copy compile_commands.json to project root for Clangd
#add_custom_target(clangd_sync ALL COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/compile_commands.json" "${CMAKE_SOURCE_DIR}/compile_commands.json" COMMENT "Syncing compile_commands.json for Clangd")

if (WAVEX_TEST)
    enable_testing()

    add_executable(wavex_tests tests/test1.cpp)
    target_link_libraries(wavex_tests PRIVATE wavex)
    add_test(NAME CoreLibraryTests COMMAND wavex_tests)

    add_executable(wavex_mw_tests tests/test_MW.cpp)
    target_link_libraries(wavex_mw_tests PRIVATE wavex)
    add_test(NAME MiddlewareTests COMMAND wavex_mw_tests)

    add_executable(wavex_method_template_tests tests/test_method_template.cpp)
    target_link_libraries(wavex_method_template_tests PRIVATE wavex)
    add_test(NAME MethodTemplateTests COMMAND wavex_method_template_tests)

    add_executable(wavex_server_tests tests/test_server.cpp)
    target_link_libraries(wavex_server_tests PRIVATE wavex)
    add_test(NAME ServerTests COMMAND wavex_server_tests)

    # Interactive dev/Postman manual testing server
    add_executable(wavex_postman_server tests/postman_demo_server.cpp)
    target_link_libraries(wavex_postman_server PRIVATE wavex)
endif ()