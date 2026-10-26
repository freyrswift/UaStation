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
            --no-opengl-sw          # drops the 20 MB opengl32sw.dll fallback
            --no-translations       # we ship our own CN/EN table in strings.cpp
            --no-quick-import       # QML module is already linked statically
            --no-system-dxc-compiler # skips dxcompiler.dll (~14 MB)
            --qmldir "${CMAKE_SOURCE_DIR}/qml"
            "$<TARGET_FILE:Ua>"
    COMMAND ${CMAKE_COMMAND} -E remove -f
            "$<TARGET_FILE_DIR:Ua>/dxcompiler.dll"
            "$<TARGET_FILE_DIR:Ua>/dxil.dll"
    COMMAND ${CMAKE_COMMAND} -E rm -rf
            "$<TARGET_FILE_DIR:Ua>/qmltooling"
    COMMENT "Deploying slim Qt runtime next to Ua.exe"
    VERBATIM
)

# Install rule for `cmake --install`.  Earlier version used
# `install(DIRECTORY ... FILES_MATCHING)` which, with the Visual Studio
# generator, dumped every intermediate ALL_BUILD/ZERO_CHECK/*_autogen/
# directory into the install prefix.  Instead, enumerate exactly what we
# want: the exe, Qt DLLs at the top level, and the plugin + QML module
# directories that windeployqt dropped next to Ua.exe.
install(PROGRAMS $<TARGET_FILE:Ua> DESTINATION .)

install(DIRECTORY $<TARGET_FILE_DIR:Ua>/
    DESTINATION .
    FILES_MATCHING
        PATTERN "*.dll"
        PATTERN "*.exe" EXCLUDE
)

foreach(subdir IN ITEMS
        platforms iconengines imageformats
        networkinformation tls styles generic
        UaApp qml)
    install(DIRECTORY $<TARGET_FILE_DIR:Ua>/${subdir}
        DESTINATION .
        OPTIONAL
    )
endforeach()
