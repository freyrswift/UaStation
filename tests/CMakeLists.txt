add_executable(ua_regression
    regression.cpp
    ../src/core/items.cpp
    ../src/core/injector.cpp
    ../src/core/install_plan.cpp
    ../src/core/skills.cpp
    ../src/core/strings.cpp
    ../src/core/prompts.cpp
    ../src/core/app_controller.cpp
    ../src/platform/instance_activation.cpp
    package-fixture.qrc
    ../resources.qrc
)
ua_add_skill_assets(ua_regression)
if(WIN32)
    target_link_libraries(ua_regression PRIVATE user32)
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
add_executable(ua_runtime runtime.cpp ../src/core/updater.h
    ../src/core/settings.cpp ../src/core/settings.h ../src/core/strings.cpp ../src/core/strings.h)
set_target_properties(ua_runtime PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
target_include_directories(ua_runtime PRIVATE ../src)
target_link_libraries(ua_runtime PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network)
target_compile_definitions(ua_runtime PRIVATE UA_VERSION_STR="${PROJECT_VERSION}" UA_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(ua_runtime PRIVATE /utf-8 /Zc:__cplusplus)
endif()
add_test(NAME ua_runtime COMMAND ua_runtime)
set(ua_test_names ua_regression ua_installation ua_progress ua_features ua_reliability ua_runtime)
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

add_executable(ua_logo_assets ../tools/logo_assets.cpp)
target_link_libraries(ua_logo_assets PRIVATE Qt6::Core Qt6::Gui Qt6::Svg)
