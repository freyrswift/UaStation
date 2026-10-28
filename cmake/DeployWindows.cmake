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

# Install rules for `cmake --install`.  Ua.exe itself ships via the
# `install(TARGETS)` call in the top-level CMakeLists.
#
# windeployqt drops Qt6*.dll next to Ua.exe plus a cluster of plugin
# directories (platforms/, iconengines/, …) and the core QML modules
# (qml/QtQuick/, qml/QtQml/, …).  Earlier versions used `install(DIRECTORY ...
# FILES_MATCHING)` to copy *.dll, which also dragged every intermediate
# CMakeFiles/, Ua_autogen/, UaApp/ build artefact into the install prefix.
# Enumerate exactly what we want instead.

# Qt6 core DLLs at the install root.
install(CODE "
    file(GLOB qt_dlls \"$<TARGET_FILE_DIR:Ua>/Qt6*.dll\")
    file(INSTALL \${qt_dlls} DESTINATION \"\${CMAKE_INSTALL_PREFIX}\")
")

# Plugin + QML module directories, each only if present on disk.
#  - platforms/iconengines/… come from windeployqt
#  - qml/ holds the Qt core QML modules (QtQuick, QtQuick/Controls, …)
#  - UaApp/ is this app's own QML module laid out by qt_add_qml_module;
#    it carries qmldir + .qmltypes next to the embedded qmlcache
foreach(subdir IN ITEMS
        platforms iconengines imageformats
        networkinformation tls styles generic
        qml UaApp)
    install(DIRECTORY $<TARGET_FILE_DIR:Ua>/${subdir}
        DESTINATION .
        OPTIONAL
    )
endforeach()
