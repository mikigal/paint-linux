# Desktop integration: the .desktop entry (its file name is the app ID that Wayland compositors
# and xdg-desktop-portal use to identify the app), AppStream metadata and the application icon.

set(PAINT_DESKTOP_NAME "${PAINT_APP_NAME}")
set(PAINT_DESKTOP_EXEC "${PAINT_EXECUTABLE}")
configure_file(${PAINT_APP_ID}.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/${PAINT_APP_ID}.desktop @ONLY)

configure_file(${PAINT_APP_ID}.metainfo.xml.in ${CMAKE_CURRENT_BINARY_DIR}/${PAINT_APP_ID}.metainfo.xml @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PAINT_APP_ID}.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PAINT_APP_ID}.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES ${CMAKE_SOURCE_DIR}/resources/app/${PAINT_APP_ID}.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

find_program(DESKTOP_FILE_VALIDATE desktop-file-validate)
if(PAINT_BUILD_TESTS AND DESKTOP_FILE_VALIDATE)
    add_test(NAME desktop_file_valid
             COMMAND ${DESKTOP_FILE_VALIDATE} ${CMAKE_CURRENT_BINARY_DIR}/${PAINT_APP_ID}.desktop)
endif()
find_program(APPSTREAMCLI appstreamcli)
if(PAINT_BUILD_TESTS AND APPSTREAMCLI)
    add_test(NAME metainfo_valid
             COMMAND ${APPSTREAMCLI} validate --no-net --explain
                     ${CMAKE_CURRENT_BINARY_DIR}/${PAINT_APP_ID}.metainfo.xml)
endif()
