# DeployLinux.cmake - Bundle Qt/QML and package the install tree via CPack.

qt_generate_deploy_qml_app_script(
    TARGET Ua
    OUTPUT_SCRIPT ua_linux_deploy_script
    NO_TRANSLATIONS
)
install(SCRIPT "${ua_linux_deploy_script}")

# A .desktop file so AppImage / system installers register us in launchers.
file(WRITE "${CMAKE_BINARY_DIR}/Ua.desktop"
"[Desktop Entry]
Type=Application
Name=Ua Toolbox
Comment=One-click AI tool prompt installer
Exec=ua
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
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    set(ua_package_arch x64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(ua_package_arch arm64)
else()
    message(FATAL_ERROR "Unsupported Linux package architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()
set(CPACK_PACKAGE_FILE_NAME         "Ua-linux-${ua_package_arch}")
set(CPACK_PACKAGE_VERSION           "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT           "freyrswift@users.noreply.github.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "freyrswift <freyrswift@users.noreply.github.com>")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS  ON)
set(CPACK_DEBIAN_PACKAGE_SECTION    "devel")
include(CPack)
