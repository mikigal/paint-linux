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

# Binary packages for Debian/Ubuntu (cpack -G DEB) and Fedora/openSUSE (cpack -G RPM), built in
# each distribution's container by packaging/release/build-package.sh, which also names the
# distribution's optional packages. Library dependencies are read from the built executable.
set(CPACK_PACKAGE_NAME ${PAINT_EXECUTABLE})
set(CPACK_PACKAGE_VENDOR "mikigal")
set(CPACK_PACKAGE_CONTACT "mikigal <mikigal.priv@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_DESCRIPTION
    "${PAINT_APP_NAME} brings the drawing app from Windows 11 to the Linux desktop: the same tools, \
layout and ideas, as a native application that follows the desktop's theme, menus and file dialogs.")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
set(CPACK_PACKAGE_RELOCATABLE OFF)
set(CPACK_STRIP_FILES ON)

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_RELEASE 1)
set(CPACK_DEBIAN_PACKAGE_SECTION graphics)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "hicolor-icon-theme")

set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_RELEASE 1)
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
set(CPACK_RPM_PACKAGE_REQUIRES "hicolor-icon-theme")
# Directories that belong to the filesystem and icon theme packages.
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    /usr/share/applications /usr/share/metainfo /usr/share/icons /usr/share/icons/hicolor
    /usr/share/icons/hicolor/scalable /usr/share/icons/hicolor/scalable/apps)

include(CPack)
