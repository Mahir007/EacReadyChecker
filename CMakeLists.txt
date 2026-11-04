cmake_minimum_required(VERSION 3.22)

project(EacReadyChecker VERSION 0.1.0 LANGUAGES CXX)

set(QT_MIN_VERSION "6.5.0")
set(EACHECK_APP_ID "io.github.Mahir007.EacReadyChecker")

include(GNUInstallDirs)
include(CTest) # BUILD_TESTING, on by default

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
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

add_subdirectory(src)
add_subdirectory(packaging)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
