cmake_minimum_required(VERSION 3.21)

# These must be set BEFORE qt_add_executable so the target property is captured
# at creation time; setting them later is a no-op.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

project(Ua
    VERSION 2.0.0
    DESCRIPTION "Ua Toolbox - cross-platform AI tool prompt installer"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Prefer new CMake policies
if(POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

# Qt install hint (set QT_DIR env or -DCMAKE_PREFIX_PATH on cmdline)
if(DEFINED ENV{Qt6_DIR})
    list(PREPEND CMAKE_PREFIX_PATH $ENV{Qt6_DIR})
elseif(WIN32 AND EXISTS "D:/Qt/6.8.3/msvc2022_64")
    list(PREPEND CMAKE_PREFIX_PATH "D:/Qt/6.8.3/msvc2022_64")
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Svg
    Network
    Widgets      # QSystemTrayIcon + QMenu for the tray context menu
)

qt_standard_project_setup(REQUIRES 6.5)

# Silence QTP0004: we intentionally keep a flat qml module without per-folder
# qmldir files. Our files live in qml/components, qml/dialogs, qml/theme but
# all share the single "UaApp" URI.
qt_policy(SET QTP0004 NEW)

# ---------------------------------------------------------------------------
# Executable
# ---------------------------------------------------------------------------
qt_add_executable(Ua WIN32 MACOSX_BUNDLE
    src/main.cpp
)

# Core (cross-platform) sources
target_sources(Ua PRIVATE
    src/core/strings.h
    src/core/strings.cpp
    src/core/items.h
    src/core/items.cpp
    src/core/paths.h
    src/core/paths.cpp
    src/core/injector.h
    src/core/injector.cpp
    src/core/prompts.h
    src/core/prompts.cpp
    src/core/skills.h
    src/core/skills.cpp
    src/core/settings.h
    src/core/settings.cpp
    src/core/updater.h
    src/core/updater.cpp
    src/core/app_controller.h
    src/core/app_controller.cpp
)

# Platform-specific sources
if(WIN32)
    target_sources(Ua PRIVATE
        src/platform/autostart_win.cpp
        src/platform/window_effects_win.cpp
        src/platform/singleton_win.cpp
    )
    target_link_libraries(Ua PRIVATE dwmapi user32 shell32 advapi32)
    set(APP_ICON_WIN ${CMAKE_SOURCE_DIR}/resources/Ua.ico)
    if(EXISTS ${APP_ICON_WIN})
        configure_file(${CMAKE_SOURCE_DIR}/cmake/app.rc.in app.rc @ONLY)
        target_sources(Ua PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
    endif()
elseif(APPLE)
    target_sources(Ua PRIVATE
        src/platform/autostart_mac.cpp
        src/platform/window_effects_mac.mm
        src/platform/singleton_posix.cpp
    )
    set_target_properties(Ua PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "Ua Toolbox"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "cc.utlra.ua"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/cmake/Info.plist.in
    )
else()
    target_sources(Ua PRIVATE
        src/platform/autostart_linux.cpp
        src/platform/window_effects_linux.cpp
        src/platform/singleton_posix.cpp
    )
endif()

# ---------------------------------------------------------------------------
# QML module
# ---------------------------------------------------------------------------
# Qt's auto-detection of `pragma Singleton` in QML_FILES is unreliable in
# this version; mark the Theme singleton explicitly so qmldir gets
# `singleton Theme 1.0 ...` and its properties resolve at runtime.
set_source_files_properties(qml/theme/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(Ua
    URI UaApp
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/components/TitleButton.qml
        qml/components/TabBar.qml
        qml/components/Card.qml
        qml/components/SettingsMenu.qml
        qml/components/Toast.qml
        qml/components/IconGlyph.qml
        qml/dialogs/InjectDialog.qml
        qml/dialogs/SkillsManager.qml
        qml/dialogs/AboutCard.qml
        qml/dialogs/Disclaimer.qml
        qml/theme/Theme.qml
)

# Embedded resources: prompts, skills, logos, icon. AUTORCC (enabled at the
# top of this file) picks up the .qrc file and generates qrc_resources.cpp
# which gets auto-added to the target's compile list.
target_sources(Ua PRIVATE resources.qrc)

target_include_directories(Ua PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/platform
)

target_link_libraries(Ua PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg
    Qt6::Network
    Qt6::Widgets
)

target_compile_definitions(Ua PRIVATE
    QT_DISABLE_DEPRECATED_UP_TO=0x060500
    UA_VERSION_STR="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(Ua PRIVATE /W3 /permissive- /utf-8 /Zc:__cplusplus)
    # Static CRT to match previous Release config (no VC runtime redist needed)
    set_property(TARGET Ua PROPERTY MSVC_RUNTIME_LIBRARY
        "$<IF:$<CONFIG:Debug>,MultiThreadedDebug,MultiThreaded>")
endif()

# ---------------------------------------------------------------------------
# Install / Deploy helpers
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS Ua
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# windeployqt / macdeployqt are driven from CI; see cmake/Deploy*.cmake
if(WIN32)
    include(cmake/DeployWindows.cmake OPTIONAL)
elseif(APPLE)
    include(cmake/DeployMacOS.cmake OPTIONAL)
else()
    include(cmake/DeployLinux.cmake OPTIONAL)
endif()
