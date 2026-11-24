# Core logic: host access, checks, reports. No widgets, so the CLI and tests can use it headless.
add_library(eacheck_core STATIC
    core/AppInfo.cpp
    core/Certificate.cpp
    core/CheckResult.cpp
    core/CheckRunner.cpp
    core/HostAccess.cpp
    core/HostSnapshot.cpp
    core/Report.cpp
    core/WellKnownKeys.cpp
    checks/AllChecks.cpp
    checks/BpfLsmCheck.cpp
    checks/EventLogHelpers.cpp
    checks/IommuCheck.cpp
    checks/KernelSigningCheck.cpp
    checks/KexecCheck.cpp
    checks/LockdownCheck.cpp
    checks/MeasuredBootCheck.cpp
    checks/SecureBootCheck.cpp
    checks/TpmCheck.cpp
    parsers/EfiSignatureList.cpp
    parsers/KernelCmdline.cpp
    parsers/TcgEventLog.cpp
)
target_include_directories(eacheck_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
)
target_compile_definitions(eacheck_core PUBLIC
    EACHECK_APP_ID="${EACHECK_APP_ID}"
)
target_link_libraries(eacheck_core
    PUBLIC Qt6::Core Qt6::Concurrent
    PRIVATE Qt6::Network eacheck_settings
)
qt_add_resources(eacheck_core "data"
    PREFIX "/data"
    BASE data
    FILES data/well-known-keys.json
)

# Widgets, split out so tests can drive the window offscreen.
add_library(eacheck_app STATIC
    app/CheckRow.cpp
    app/ColorScheme.cpp
    app/IconWidget.cpp
    app/MainWindow.cpp
    app/MessageBar.cpp
    app/Motion.cpp
    app/Revealer.cpp
    app/SummaryBanner.cpp
    app/TouchScroll.cpp
    app/WindowDecoration.cpp
)
target_link_libraries(eacheck_app
    PUBLIC eacheck_core Qt6::Widgets
    PRIVATE Qt6::DBus eacheck_settings
)
# Fallback icons for themes that lack the status icons (e.g. Adwaita without Breeze installed).
qt_add_resources(eacheck_app "icons"
    PREFIX "/icons"
    BASE app/icons
    FILES
        app/icons/status-pass.svg
        app/icons/status-fail.svg
        app/icons/status-not-required.svg
        app/icons/status-needs-admin.svg
        app/icons/status-unknown.svg
        app/icons/verdict-ready.svg
        app/icons/verdict-not-ready.svg
        app/icons/verdict-incomplete.svg
)
qt_add_resources(eacheck_app "appicon"
    PREFIX "/icons"
    BASE ${PROJECT_SOURCE_DIR}/packaging/icons
    FILES ${PROJECT_SOURCE_DIR}/packaging/icons/${EACHECK_APP_ID}.svg
)

add_executable(eac-ready-checker
    main.cpp
    cli/CliMain.cpp
)
target_link_libraries(eac-ready-checker PRIVATE eacheck_app eacheck_settings)
if(TARGET qadwaitadecorations)
    target_compile_definitions(eacheck_app PRIVATE
        EACHECK_DECORATION_PLUGIN_DIR="${EACHECK_PLUGIN_DIR_FROM_BIN}"
        EACHECK_DECORATION_PLUGIN_FILE="$<TARGET_FILE_NAME:qadwaitadecorations>"
    )
    add_dependencies(eac-ready-checker qadwaitadecorations)
endif()

install(TARGETS eac-ready-checker RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
