include(ECMAddTests)

ecm_add_tests(
    tst_checkresult.cpp
    tst_checkrunner.cpp
    tst_checks.cpp
    tst_fixtures.cpp
    tst_hostaccess.cpp
    tst_kernelcmdline.cpp
    LINK_LIBRARIES eacheck_core Qt6::Test
)
target_compile_definitions(tst_fixtures PRIVATE
    EACHECK_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    EACHECK_CAPTURE_SCRIPT="${PROJECT_SOURCE_DIR}/tools/capture-fixture.sh"
)

ecm_add_test(tst_cli.cpp LINK_LIBRARIES Qt6::Test TEST_NAME tst_cli)
target_compile_definitions(tst_cli PRIVATE EACHECK_BINARY="$<TARGET_FILE:eac-ready-checker>")
add_dependencies(tst_cli eac-ready-checker)

ecm_add_test(tst_mainwindow.cpp LINK_LIBRARIES eacheck_app Qt6::Test TEST_NAME tst_mainwindow)
target_compile_definitions(tst_mainwindow PRIVATE EACHECK_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
set_tests_properties(tst_mainwindow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
