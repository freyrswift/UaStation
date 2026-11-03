add_executable(ua_regression
    regression.cpp
    ../src/core/items.cpp
    ../src/core/injector.cpp
    ../src/core/install_plan.cpp
    ../src/core/skills.cpp
    ../src/core/skill_catalog.cpp
    ../src/core/strings.cpp
    ../src/core/app_strings.h
    ../src/core/prompts.cpp
    ../src/core/app_controller.cpp
    ../src/platform/instance_activation.cpp
    ../src/platform/vscode_discovery.cpp
    package-fixture.qrc
    ../resources.qrc
)
ua_add_skill_assets(ua_regression)
if(WIN32)
    target_link_libraries(ua_regression PRIVATE user32 shell32 ole32 uuid)
endif()
set_target_properties(ua_regression PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
target_include_directories(ua_regression PRIVATE ../src)
target_link_libraries(ua_regression PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Widgets Qt6::Concurrent Qt6::Network Qt6::Svg
)
target_compile_definitions(ua_regression PRIVATE
    UA_VERSION_STR="${PROJECT_VERSION}"
    UA_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
if(MSVC)
    target_compile_options(ua_regression PRIVATE /utf-8 /Zc:__cplusplus)
endif()
add_test(NAME ua_regression COMMAND ua_regression)
add_test(NAME ua_installation COMMAND ua_regression --installation-only)
add_test(NAME ua_progress COMMAND ua_regression --progress-only)
add_test(NAME ua_features COMMAND ua_regression --features-only)
add_test(NAME ua_reliability COMMAND ua_regression --reliability-only)
add_test(NAME ua_prompts COMMAND ua_regression --prompts-only)
add_test(NAME ua_vscode_ui COMMAND ua_regression --vscode-ui-only)
add_test(NAME ua_shared_skills COMMAND ua_regression --shared-skills-only)
add_executable(ua_skill_catalog skill_catalog.cpp ../src/core/skill_catalog.cpp)
target_include_directories(ua_skill_catalog PRIVATE ../src)
target_link_libraries(ua_skill_catalog PRIVATE Qt6::Core)
target_compile_definitions(ua_skill_catalog PRIVATE UA_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_target_properties(ua_skill_catalog PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
add_test(NAME ua_skill_catalog COMMAND ua_skill_catalog)
add_executable(ua_mcp_server mcp_server.cpp)
target_include_directories(ua_mcp_server PRIVATE ../src)
target_link_libraries(ua_mcp_server PRIVATE Qt6::Core)
target_compile_definitions(ua_mcp_server PRIVATE UA_MCP_EXECUTABLE="$<TARGET_FILE:ua_skills_mcp>")
set_target_properties(ua_mcp_server PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
add_dependencies(ua_mcp_server ua_skills_mcp)
add_dependencies(ua_regression ua_skills_mcp)
add_test(NAME ua_mcp_server COMMAND ua_mcp_server)
if(MSVC)
    target_compile_options(ua_skill_catalog PRIVATE /utf-8 /Zc:__cplusplus)
    target_compile_options(ua_mcp_server PRIVATE /utf-8 /Zc:__cplusplus)
endif()
add_executable(ua_runtime runtime.cpp ../src/core/updater.h
    ../src/core/settings.cpp ../src/core/settings.h ../src/core/strings.cpp ../src/core/app_strings.h)
set_property(SOURCE runtime.cpp APPEND PROPERTY OBJECT_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/core/updater.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/updater.h")
set_target_properties(ua_runtime PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
target_include_directories(ua_runtime PRIVATE ../src)
target_link_libraries(ua_runtime PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network Qt6::Concurrent)
target_compile_definitions(ua_runtime PRIVATE UA_VERSION_STR="${PROJECT_VERSION}" UA_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(ua_runtime PRIVATE /utf-8 /Zc:__cplusplus)
endif()
add_test(NAME ua_runtime COMMAND ua_runtime)
add_executable(ua_detection vscode_detection.cpp ../src/core/items.cpp)
set_target_properties(ua_detection PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
target_include_directories(ua_detection PRIVATE ../src)
target_link_libraries(ua_detection PRIVATE Qt6::Core Qt6::QmlIntegration)
if(MSVC)
    target_compile_options(ua_detection PRIVATE /utf-8 /Zc:__cplusplus)
endif()
add_test(NAME ua_detection COMMAND ua_detection)
if(WIN32)
    add_executable(ua_shortcuts vscode_shortcuts.cpp)
    set_target_properties(ua_shortcuts PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
    target_link_libraries(ua_shortcuts PRIVATE Qt6::Core shell32 ole32 uuid)
    if(MSVC)
        target_compile_options(ua_shortcuts PRIVATE /utf-8 /Zc:__cplusplus)
    endif()
    add_test(NAME ua_shortcuts COMMAND ua_shortcuts)
endif()
add_executable(ua_update_installer update_installer.cpp ../src/core/update_installer.cpp)
set_target_properties(ua_update_installer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
target_include_directories(ua_update_installer PRIVATE ../src)
target_link_libraries(ua_update_installer PRIVATE Qt6::Core)
if(MSVC)
    target_compile_options(ua_update_installer PRIVATE /utf-8 /Zc:__cplusplus)
endif()
add_test(NAME ua_update_installer COMMAND ua_update_installer)
set(ua_test_names ua_regression ua_installation ua_progress ua_features ua_reliability ua_prompts ua_vscode_ui ua_shared_skills ua_skill_catalog ua_mcp_server ua_runtime ua_detection ua_update_installer)
if(WIN32)
    list(APPEND ua_test_names ua_shortcuts)
endif()
set_tests_properties(${ua_test_names} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QUICK_CONTROLS_STYLE=Basic")
set_property(TEST ${ua_test_names} APPEND PROPERTY ENVIRONMENT
    "QT_PLUGIN_PATH=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}"
    "QT_QPA_PLATFORM_PLUGIN_PATH=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}/platforms"
    "QML_IMPORT_PATH=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_QML}"
    "QML2_IMPORT_PATH=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_QML}"
)
if(WIN32)
    string(REPLACE ";" "\\;" test_runtime_path "$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}")
    set_property(TEST ${ua_test_names} APPEND PROPERTY ENVIRONMENT "PATH=${test_runtime_path}")
endif()
set_tests_properties(${ua_test_names} PROPERTIES TIMEOUT 900)
if(WIN32)
    set_tests_properties(ua_shortcuts PROPERTIES TIMEOUT 120)
endif()

add_executable(ua_logo_assets ../tools/logo_assets.cpp)
target_link_libraries(ua_logo_assets PRIVATE Qt6::Core Qt6::Gui Qt6::Svg)
