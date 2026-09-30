cmake_minimum_required(VERSION 3.19)
project(Schematic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

qt_standard_project_setup()

# Defaults to ON: this project is under active development and AGENTS.md's
# collaboration workflow treats focused tests as a first-class part of each
# milestone, so the default (and the "default"/"mingw" CMake presets) build
# and run schematiccore_tests. Qt6::Test is only find_package()'d, and
# enable_testing()/add_subdirectory(tests) only run, when BUILD_TESTING is
# ON (see the if(BUILD_TESTING) block below) - someone who wants an
# app-only build with no Qt6::Test dependency can configure with
# -DBUILD_TESTING=OFF.
option(BUILD_TESTING "Build the schematiccore test suite" ON)

if(MSVC)
    add_compile_options(/FS)
endif()

# schematiccore: the Qt-Widgets-independent document model (components,
# terminals, wire routes, connectivity). Depends only on Qt6::Core so it can
# be exercised by a lightweight, headless test executable - see AGENTS.md
# "keep the schematic document model independent of Qt rendering items and
# input handling".
add_library(schematiccore STATIC
    core/identifiers.h
    core/symbolkind.h
    core/symbolkind.cpp
    core/symboldefinition.h
    core/symboldefinition.cpp
    core/component.h
    core/component.cpp
    core/terminalref.h
    core/wireendpoint.h
    core/pendingwireendpoint.h

    core/wireroute.h
    core/routegeometry.h
    core/routegeometry.cpp
    core/document.h
    core/document.cpp
    core/documentloadresult.h
    core/documentjson.cpp
)

target_include_directories(schematiccore PUBLIC core)

target_link_libraries(schematiccore
    PUBLIC
        Qt6::Core
)

qt_add_executable(Schematic
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    schematicscene.cpp
    schematicscene.h
    schematicitems.h
    schematicview.cpp
    schematicview.h
)

target_link_libraries(Schematic
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        schematiccore
)

if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS Schematic
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Schematic
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
