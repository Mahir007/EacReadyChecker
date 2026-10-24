include(ECMAddTests)

ecm_add_tests(
    tst_checkresult.cpp
    tst_hostaccess.cpp
    tst_checkrunner.cpp
    LINK_LIBRARIES eacheck_core Qt6::Test
)

ecm_add_test(tst_cli.cpp LINK_LIBRARIES Qt6::Test TEST_NAME tst_cli)
target_compile_definitions(tst_cli PRIVATE EACHECK_BINARY="$<TARGET_FILE:eac-ready-checker>")
add_dependencies(tst_cli eac-ready-checker)

ecm_add_test(tst_mainwindow.cpp LINK_LIBRARIES eacheck_app Qt6::Test TEST_NAME tst_mainwindow)
set_tests_properties(tst_mainwindow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
