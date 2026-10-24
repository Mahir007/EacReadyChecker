# Core logic: host access, checks, reports. No widgets, so the CLI and tests can use it headless.
add_library(eacheck_core STATIC
    core/CheckResult.cpp
    core/CheckRunner.cpp
    core/HostAccess.cpp
    core/HostSnapshot.cpp
    core/Report.cpp
    checks/AllChecks.cpp
    checks/BpfLsmCheck.cpp
    checks/IommuCheck.cpp
    checks/KernelSigningCheck.cpp
    checks/KexecCheck.cpp
    checks/LockdownCheck.cpp
    checks/PlaceholderCheck.cpp
    checks/SecureBootCheck.cpp
    checks/TpmCheck.cpp
    parsers/KernelCmdline.cpp
)
target_include_directories(eacheck_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
)
target_compile_definitions(eacheck_core PUBLIC
    EACHECK_APP_ID="${EACHECK_APP_ID}"
)
target_link_libraries(eacheck_core PUBLIC Qt6::Core Qt6::Concurrent)

# Widgets, split out so tests can drive the window offscreen.
add_library(eacheck_app STATIC
    app/CheckRow.cpp
    app/MainWindow.cpp
    app/SummaryBanner.cpp
)
target_link_libraries(eacheck_app PUBLIC
    eacheck_core
    Qt6::Widgets
    KF6::CoreAddons
)

add_executable(eac-ready-checker
    main.cpp
    cli/CliMain.cpp
)
target_link_libraries(eac-ready-checker PRIVATE
    eacheck_app
    KF6::IconThemes
)

install(TARGETS eac-ready-checker ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
