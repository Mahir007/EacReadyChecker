cmake_minimum_required(VERSION 3.22)

project(EacReadyChecker LANGUAGES CXX)

# Only built copies get a real version: CI passes the tag's, or 0.0.0-dev+<commit> without one.
set(EACHECK_VERSION "0.0.0-dev" CACHE STRING "Version the app reports")

set(QT_MIN_VERSION "6.5.0")
set(EACHECK_APP_ID "io.github.Mahir007.EacReadyChecker")

include(GNUInstallDirs)
include(CTest) # BUILD_TESTING, on by default

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Two build profiles. Debug is the fast one, for development, the tests, and dev, alpha and beta
# builds: no optimisation, full debug info and the fastest linker found. Release is for release
# candidates and releases: optimised, with link-time optimisation across all our code, unused code
# removed, and stripped.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # After any -O in CXXFLAGS, such as the Flatpak SDK's -O2. Fortify needs optimisation, and glibc
    # warns on every file when CXXFLAGS turn it on at -O0.
    add_compile_options("$<$<CONFIG:Debug>:-O0;-Wp,-U_FORTIFY_SOURCE>")
    include(CheckLinkerFlag)
    foreach(linker mold lld)
        check_linker_flag(CXX -fuse-ld=${linker} EACHECK_LINKER_${linker})
        if(EACHECK_LINKER_${linker})
            add_link_options("$<$<CONFIG:Debug>:-fuse-ld=${linker}>")
            break()
        endif()
    endforeach()

    add_compile_options("$<$<CONFIG:Release>:-ffunction-sections;-fdata-sections>")
    add_link_options("$<$<CONFIG:Release>:-Wl,--gc-sections;-s>")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # GCC's counterpart of one codegen unit.
        add_link_options("$<$<CONFIG:Release>:-flto-partition=one>")
    endif()
endif()
include(CheckIPOSupported)
check_ipo_supported(RESULT EACHECK_IPO OUTPUT ipo_error LANGUAGES CXX)
if(EACHECK_IPO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
else()
    message(WARNING "No link-time optimisation in Release builds: ${ipo_error}")
endif()

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core DBus Gui Widgets Concurrent Network)
if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
endif()

# Warnings and strict Qt string handling for our own targets.
add_library(eacheck_settings INTERFACE)
target_compile_definitions(eacheck_settings INTERFACE
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
    QT_NO_CAST_FROM_BYTEARRAY
    QT_NO_URL_CAST_FROM_STRING
    QT_NO_NARROWING_CONVERSIONS_IN_CONNECT
    QT_NO_KEYWORDS
    QT_NO_FOREACH
    QT_USE_QSTRINGBUILDER
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(eacheck_settings INTERFACE
        -Wall -Wextra -pedantic
        -Wnon-virtual-dtor -Woverloaded-virtual -Wsuggest-override
        -Wzero-as-null-pointer-constant -Werror=return-type
    )
endif()

configure_file(src/eacheck_version.h.in ${CMAKE_BINARY_DIR}/eacheck_version.h @ONLY)

# Plugins only this app loads, outside Qt's plugin folder so other Qt apps never pick them up.
# The build tree uses the same layout relative to bin/, so the app finds them in both.
set(EACHECK_PLUGIN_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/eac-ready-checker/plugins")
file(RELATIVE_PATH EACHECK_PLUGIN_DIR_FROM_BIN "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_LIBDIR}/eac-ready-checker/plugins")

add_subdirectory(third_party)
add_subdirectory(src)
add_subdirectory(packaging)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
