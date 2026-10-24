cmake_minimum_required(VERSION 3.22)

project(EacReadyChecker VERSION 0.1.0 LANGUAGES CXX)

set(QT_MIN_VERSION "6.5.0")
set(KF_MIN_VERSION "6.0.0")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)
include(FeatureSummary)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Widgets Concurrent)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS CoreAddons IconThemes)

ecm_setup_version(${PROJECT_VERSION}
    VARIABLE_PREFIX EACHECK
    VERSION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/eacheck_version.h
)

set(EACHECK_APP_ID "io.github.Mahir007.EacReadyChecker")

add_subdirectory(src)
add_subdirectory(packaging)

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
