# DeployMacOS.cmake - Runs macdeployqt to bundle Qt frameworks + QML into the .app.
#
# After `cmake --build build`, Ua.app under build/ is a complete runnable bundle.
# Pass `-DCPACK_GENERATOR=DragNDrop` and `cpack` to produce Ua-<ver>.dmg.

find_program(MACDEPLOYQT_EXECUTABLE
    NAMES macdeployqt
    HINTS "${Qt6_DIR}/../../../bin" "${QT6_INSTALL_PREFIX}/bin"
    REQUIRED
)

add_custom_command(TARGET Ua POST_BUILD
    COMMAND "${MACDEPLOYQT_EXECUTABLE}"
            "$<TARGET_BUNDLE_DIR:Ua>"
            -qmldir=${CMAKE_SOURCE_DIR}/qml
            -always-overwrite
    COMMENT "Bundling Qt frameworks into Ua.app"
    VERBATIM
)

install(DIRECTORY $<TARGET_BUNDLE_DIR:Ua>
    DESTINATION .
    USE_SOURCE_PERMISSIONS
)

# CPack hook for a drag-and-drop dmg
set(CPACK_GENERATOR "DragNDrop")
set(CPACK_PACKAGE_FILE_NAME "Ua-${PROJECT_VERSION}-macos")
set(CPACK_DMG_VOLUME_NAME "Ua Toolbox")
include(CPack)
