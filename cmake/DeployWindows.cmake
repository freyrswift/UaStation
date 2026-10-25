# DeployWindows.cmake - Runs windeployqt at install time to copy Qt DLLs,
# QML plugins, platforms/qwindows.dll and translations next to Ua.exe.
#
# Build: cmake --build build --config Release
# Deploy: cmake --install build --config Release --prefix dist

find_program(WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt windeployqt.exe
    HINTS "${Qt6_DIR}/../../../bin" "${QT6_INSTALL_PREFIX}/bin"
    REQUIRED
)

# Attach a POST_BUILD step so a plain `cmake --build` run yields a
# directly-runnable Ua.exe without needing a separate deploy pass.
add_custom_command(TARGET Ua POST_BUILD
    COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --release
            --no-compiler-runtime
            --no-system-d3d-compiler
            --qmldir "${CMAKE_SOURCE_DIR}/qml"
            "$<TARGET_FILE:Ua>"
    COMMENT "Deploying Qt runtime next to Ua.exe"
    VERBATIM
)

# Install rule for `cmake --install`: copy the entire deployed tree.
install(DIRECTORY $<TARGET_FILE_DIR:Ua>/
    DESTINATION .
    FILES_MATCHING
        PATTERN "*.exe"
        PATTERN "*.dll"
        PATTERN "*.qmltypes"
        PATTERN "qmldir"
        PATTERN "*.qm"
        PATTERN "generic/*"
        PATTERN "iconengines/*"
        PATTERN "imageformats/*"
        PATTERN "platforms/*"
        PATTERN "styles/*"
        PATTERN "translations/*"
        PATTERN "QtQuick/*"
        PATTERN "QtQml/*"
        PATTERN "UaApp/*"
        PATTERN "CMakeFiles" EXCLUDE
        PATTERN ".qt" EXCLUDE
        PATTERN ".rcc" EXCLUDE
        PATTERN "Ua_autogen" EXCLUDE
        PATTERN "meta_types" EXCLUDE
        PATTERN "qmltypes" EXCLUDE
        PATTERN "qmltooling" EXCLUDE
)
