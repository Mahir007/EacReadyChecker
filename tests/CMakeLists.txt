function(eacheck_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE ${ARGN} Qt6::Test eacheck_settings)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

foreach(test tst_checkresult tst_checkrunner tst_checks tst_efisignaturelist tst_fixtures tst_hostaccess tst_kernelcmdline tst_tcgeventlog tst_wellknownkeys)
    eacheck_add_test(${test} eacheck_core)
endforeach()
foreach(test tst_checks tst_efisignaturelist tst_tcgeventlog tst_wellknownkeys)
    target_compile_definitions(${test} PRIVATE EACHECK_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
endforeach()
target_compile_definitions(tst_tcgeventlog PRIVATE EACHECK_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_compile_definitions(tst_fixtures PRIVATE
    EACHECK_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    EACHECK_CAPTURE_SCRIPT="${PROJECT_SOURCE_DIR}/tools/capture-fixture.sh"
)

eacheck_add_test(tst_cli)
target_compile_definitions(tst_cli PRIVATE EACHECK_BINARY="$<TARGET_FILE:eac-ready-checker>")
add_dependencies(tst_cli eac-ready-checker)

eacheck_add_test(tst_mainwindow eacheck_app)
target_compile_definitions(tst_mainwindow PRIVATE EACHECK_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
set_tests_properties(tst_mainwindow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

eacheck_add_test(tst_windowdecoration eacheck_app)

eacheck_add_test(tst_iconwidget eacheck_app)
set_tests_properties(tst_iconwidget PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen:configfile=${CMAKE_CURRENT_SOURCE_DIR}/data/two-screens.json")

# Tests with a stand-in settings portal. They run on a private session bus when they can, since a
# real portal would hold the stand-in's name.
find_program(DBUS_RUN_SESSION dbus-run-session)
foreach(test tst_colorscheme tst_motion)
    add_executable(${test} ${test}.cpp FakePortal.h)
    target_link_libraries(${test} PRIVATE eacheck_app Qt6::DBus Qt6::Test eacheck_settings)
    if(DBUS_RUN_SESSION)
        add_test(NAME ${test} COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:${test}>)
    else()
        add_test(NAME ${test} COMMAND ${test})
    endif()
    set_tests_properties(${test} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()

# The release notes script, which the release workflow runs. It needs Python 3.9 and git.
find_package(Python3 3.9 COMPONENTS Interpreter)
find_package(Git)
if(Python3_Interpreter_FOUND AND GIT_FOUND)
    add_test(NAME release-notes COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_release_notes.py)
endif()
