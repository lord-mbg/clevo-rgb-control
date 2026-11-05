cmake_minimum_required(VERSION 3.16)
project(clevo_rgb_control LANGUAGES CXX)

find_package(Threads REQUIRED)

add_executable(clevo-rgb src/main.cpp src/effects.cpp src/service.cpp src/profiles.cpp)
target_link_libraries(clevo-rgb PRIVATE Threads::Threads)
target_compile_features(clevo-rgb PRIVATE cxx_std_17)
target_compile_options(clevo-rgb PRIVATE -Wall -Wextra -Wpedantic)

option(BUILD_GUI "Build the Qt Quick desktop control panel" ON)
if(BUILD_GUI)
    find_package(Qt6 6.5 QUIET COMPONENTS Quick QuickControls2 Widgets)
    if(Qt6_FOUND)
        qt_add_executable(clevo-rgb-ui
            src/ui/main.cpp src/ui/controller.cpp src/ui/controller.h src/profiles.cpp)
        set_target_properties(clevo-rgb-ui PROPERTIES AUTOMOC ON)
        target_link_libraries(clevo-rgb-ui PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Widgets)
        target_compile_features(clevo-rgb-ui PRIVATE cxx_std_17)
        target_compile_options(clevo-rgb-ui PRIVATE -Wall -Wextra -Wpedantic)
        qt_add_resources(clevo-rgb-ui "ui" PREFIX "/ui" BASE "src/ui"
            FILES src/ui/Main.qml src/ui/Theme.qml src/ui/StyledButton.qml
                  src/ui/StyledField.qml src/ui/StyledSpinBox.qml src/ui/StyledCombo.qml
                  src/ui/KeyboardPreview.qml src/ui/qmldir)
        add_dependencies(clevo-rgb-ui clevo-rgb)
    else()
        message(STATUS "Qt 6 Quick/QuickControls2 not found; building CLI only")
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME profile-boundaries
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_profiles.py"
                    "$<TARGET_FILE:clevo-rgb>")
    endif()

    # GUI regressions need the Qt panel sources plus Qt Test. Absent Qt Test keeps
    # the CLI/Python suite available; configure with BUILD_GUI=ON and Qt6 Test installed.
    if(BUILD_GUI AND Qt6_FOUND)
        find_package(Qt6 6.5 QUIET COMPONENTS Test)
        if(Qt6Test_FOUND)
            qt_add_executable(gui_interactions
                tests/gui_interactions.cpp
                src/ui/controller.cpp
                src/ui/controller.h
                src/profiles.cpp)
            set_target_properties(gui_interactions PROPERTIES AUTOMOC ON)
            target_link_libraries(gui_interactions PRIVATE
                Qt6::Test Qt6::Quick Qt6::QuickControls2 Qt6::Widgets)
            target_compile_features(gui_interactions PRIVATE cxx_std_17)
            target_compile_options(gui_interactions PRIVATE -Wall -Wextra -Wpedantic)
            add_dependencies(gui_interactions clevo-rgb)
            add_test(NAME gui-interactions COMMAND "$<TARGET_FILE:gui_interactions>")
            set_tests_properties(gui-interactions PROPERTIES
                TIMEOUT 90
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;LIBGL_ALWAYS_SOFTWARE=1;CLEVO_TEST_CLI_PATH=$<TARGET_FILE:clevo-rgb>;CLEVO_TEST_UI_DIR=${CMAKE_CURRENT_SOURCE_DIR}/src/ui")
        else()
            message(STATUS "Qt6 Test not found; skipping GUI interaction regressions (install Qt6 Test to enable)")
        endif()
    endif()
endif()
