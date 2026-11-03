# DeployMacOS.cmake - Runs macdeployqt to bundle Qt frameworks + QML into the .app.
#
# After `cmake --build build`, ua.app contains the Qt runtime and QML imports.

find_program(MACDEPLOYQT_EXECUTABLE
    NAMES macdeployqt
    HINTS "${Qt6_DIR}/../../../bin" "${QT6_INSTALL_PREFIX}/bin"
    REQUIRED
)

add_custom_command(TARGET Ua POST_BUILD
    COMMAND "${MACDEPLOYQT_EXECUTABLE}"
            "$<TARGET_BUNDLE_DIR:Ua>"
            "-executable=$<TARGET_BUNDLE_DIR:Ua>/Contents/MacOS/ua-skills-mcp"
            -qmldir=${CMAKE_SOURCE_DIR}/qml
            -always-overwrite
    COMMENT "Bundling Qt frameworks into Ua.app"
    VERBATIM
)

# CPack hook for a drag-and-drop dmg
if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64" OR
   (NOT CMAKE_OSX_ARCHITECTURES AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$"))
    set(ua_package_arch arm64)
elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64" OR
       (NOT CMAKE_OSX_ARCHITECTURES AND CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64"))
    set(ua_package_arch x64)
elseif("arm64" IN_LIST CMAKE_OSX_ARCHITECTURES AND "x86_64" IN_LIST CMAKE_OSX_ARCHITECTURES)
    set(ua_package_arch universal)
else()
    message(FATAL_ERROR "Unsupported macOS package architecture: ${CMAKE_OSX_ARCHITECTURES}")
endif()

set(CPACK_GENERATOR "DragNDrop")
set(CPACK_PACKAGE_FILE_NAME "Ua-macos-${ua_package_arch}")
set(CPACK_DMG_VOLUME_NAME "Ua Toolbox")
set(CPACK_DMG_DISABLE_APPLICATIONS_SYMLINK OFF)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
include(CPack)
