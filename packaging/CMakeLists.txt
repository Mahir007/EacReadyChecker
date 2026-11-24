set(desktop_file ${CMAKE_CURRENT_SOURCE_DIR}/${EACHECK_APP_ID}.desktop)
set(metainfo_file ${CMAKE_CURRENT_BINARY_DIR}/${EACHECK_APP_ID}.metainfo.xml)

# The metainfo's <releases> come from the version tags (metainfo-releases.py). Builds that can't run
# git, such as the Flatpak's, are given the output of the script made beforehand.
set(EACHECK_METAINFO_RELEASES "" CACHE FILEPATH "File with the metainfo's <releases>, from packaging/metainfo-releases.py")
find_package(Python3 COMPONENTS Interpreter)
if(EACHECK_METAINFO_RELEASES)
    get_filename_component(releases_file ${EACHECK_METAINFO_RELEASES} ABSOLUTE BASE_DIR ${PROJECT_SOURCE_DIR})
    file(READ ${releases_file} EACHECK_RELEASES)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${releases_file})
elseif(Python3_Interpreter_FOUND)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/metainfo-releases.py ${EACHECK_VERSION}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE EACHECK_RELEASES
        COMMAND_ERROR_IS_FATAL ANY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS metainfo-releases.py)
else()
    # As the script does without tags.
    string(TIMESTAMP day "%Y-%m-%d" UTC)
    string(REGEX REPLACE "\\+.*" "" version ${EACHECK_VERSION})
    string(REGEX REPLACE "^([^-]*)-" "\\1~" version ${version})
    set(type stable)
    if(EACHECK_VERSION MATCHES "-")
        set(type development)
    endif()
    set(EACHECK_RELEASES "  <releases>\n    <release version=\"${version}\" date=\"${day}\" type=\"${type}\"/>\n  </releases>\n")
endif()
string(STRIP "${EACHECK_RELEASES}" EACHECK_RELEASES)
set(EACHECK_RELEASES "  ${EACHECK_RELEASES}")
configure_file(${EACHECK_APP_ID}.metainfo.xml.in ${metainfo_file} @ONLY)

install(FILES ${desktop_file} DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${metainfo_file} DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES icons/${EACHECK_APP_ID}.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

if(BUILD_TESTING)
    find_program(APPSTREAMCLI appstreamcli)
    if(APPSTREAMCLI)
        add_test(NAME appstream COMMAND ${APPSTREAMCLI} validate --no-net ${metainfo_file})
    endif()
    find_program(DESKTOP_FILE_VALIDATE desktop-file-validate)
    if(DESKTOP_FILE_VALIDATE)
        add_test(NAME desktop-file COMMAND ${DESKTOP_FILE_VALIDATE} ${desktop_file})
    endif()
endif()
