# QAdwaitaDecorations, the title bar plugin the app uses on GNOME. Copied
# unchanged from https://github.com/FedoraQt/QAdwaitaDecorations at commit 440d4b0: LICENSE,
# README.md and src/ without its CMakeLists.txt. It uses Qt's private API, so it's built here
# against the app's Qt.
set(EACHECK_DECORATIONS AUTO CACHE STRING "Build the GNOME title bar plugin: AUTO, ON or OFF")
set_property(CACHE EACHECK_DECORATIONS PROPERTY STRINGS AUTO ON OFF)
if(EACHECK_DECORATIONS STREQUAL "OFF")
    return()
endif()

find_package(Qt6 ${QT_MIN_VERSION} QUIET OPTIONAL_COMPONENTS Svg WaylandClient)
# From Qt 6.10 the private modules are separate packages. Finding them warns that the build is tied to
# this Qt version. The app checks the Qt version before it loads the plugin, so the warning is off.
if(Qt6Core_VERSION VERSION_GREATER_EQUAL 6.10)
    set(QT_NO_PRIVATE_MODULE_WARNING ON)
    find_package(Qt6 ${QT_MIN_VERSION} QUIET OPTIONAL_COMPONENTS GuiPrivate WaylandClientPrivate)
endif()

set(found FALSE)
if(TARGET Qt6::Svg AND TARGET Qt6::GuiPrivate AND TARGET Qt6::WaylandClientPrivate)
    # Some distributions ship the private headers separately from the CMake files. The headers pull in
    # most of QtGui, so the check takes a few seconds. A pass is cached; a failure is checked again on
    # the next configure, so installing the headers later is picked up.
    if(NOT EACHECK_HAVE_QT_PRIVATE_HEADERS)
        unset(EACHECK_HAVE_QT_PRIVATE_HEADERS CACHE)
    endif()
    message(CHECK_START "Looking for Qt private headers")
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_LIBRARIES Qt6::GuiPrivate Qt6::WaylandClientPrivate)
    set(CMAKE_REQUIRED_QUIET ON)
    check_cxx_source_compiles("
        #include <QtGui/private/qguiapplication_p.h>
        #include <QtWaylandClient/private/qwaylandabstractdecoration_p.h>
        int main() { return 0; }" EACHECK_HAVE_QT_PRIVATE_HEADERS)
    if(EACHECK_HAVE_QT_PRIVATE_HEADERS)
        message(CHECK_PASS "found")
    else()
        message(CHECK_FAIL "not found")
    endif()
    set(found ${EACHECK_HAVE_QT_PRIVATE_HEADERS})
endif()

if(NOT found)
    set(missing "missing Qt private headers or Qt SVG dev files")
    if(EACHECK_DECORATIONS STREQUAL "ON")
        message(FATAL_ERROR "EACHECK_DECORATIONS is ON: ${missing}")
    endif()
    message(STATUS "GNOME title bar plugin: off (${missing})")
    return()
endif()
message(STATUS "GNOME title bar plugin: on")

add_library(qadwaitadecorations MODULE
    qadwaitadecorations/src/qadwaitadecorations.cpp
    qadwaitadecorations/src/qadwaitadecorationsplugin.cpp
)
# HAS_QT6_SUPPORT turns on the shadow. Upstream's CMakeLists.txt sets it for every Qt 6 build.
target_compile_definitions(qadwaitadecorations PRIVATE HAS_QT6_SUPPORT)
target_link_libraries(qadwaitadecorations PRIVATE
    Qt6::Core Qt6::DBus Qt6::Gui Qt6::GuiPrivate Qt6::Svg Qt6::WaylandClientPrivate Qt6::Widgets
)
cmake_path(ABSOLUTE_PATH EACHECK_PLUGIN_DIR_FROM_BIN BASE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" NORMALIZE
    OUTPUT_VARIABLE build_plugin_dir)
set_target_properties(qadwaitadecorations PROPERTIES
    CXX_STANDARD 17
    LIBRARY_OUTPUT_DIRECTORY "${build_plugin_dir}/wayland-decoration-client"
)
install(TARGETS qadwaitadecorations LIBRARY DESTINATION "${EACHECK_PLUGIN_INSTALL_DIR}/wayland-decoration-client")
