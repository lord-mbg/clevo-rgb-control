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
endif()
