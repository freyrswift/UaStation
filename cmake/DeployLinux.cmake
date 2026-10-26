# DeployLinux.cmake - Package Ua as .tar.gz and .deb via CPack.
#
# AppImage builds happen in CI via linuxdeploy / linuxdeploy-plugin-qt rather
# than from CMake directly; see .github/workflows/build.yml.

install(TARGETS Ua
    RUNTIME DESTINATION bin
)

# A .desktop file so AppImage / system installers register us in launchers.
file(WRITE "${CMAKE_BINARY_DIR}/Ua.desktop"
"[Desktop Entry]
Type=Application
Name=Ua Toolbox
Comment=One-click AI tool prompt installer
Exec=Ua
Icon=Ua
Categories=Development;Utility;
Terminal=false
")

install(FILES "${CMAKE_BINARY_DIR}/Ua.desktop"
    DESTINATION share/applications
)

if(EXISTS "${CMAKE_SOURCE_DIR}/resources/Ua.ico")
    install(FILES "${CMAKE_SOURCE_DIR}/resources/Ua.ico"
        DESTINATION share/icons/hicolor/256x256/apps
        RENAME Ua.ico
    )
endif()

set(CPACK_GENERATOR "TGZ;DEB")
set(CPACK_PACKAGE_FILE_NAME         "Ua-${PROJECT_VERSION}-linux")
set(CPACK_PACKAGE_CONTACT           "freyrswift@users.noreply.github.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "freyrswift <freyrswift@users.noreply.github.com>")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS  ON)
set(CPACK_DEBIAN_PACKAGE_SECTION    "devel")
set(CPACK_DEBIAN_PACKAGE_DEPENDS    "libqt6core6, libqt6gui6, libqt6qml6, libqt6quick6, libqt6network6")
include(CPack)
