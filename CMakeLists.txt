cmake_minimum_required(VERSION 3.19)
project(Schematic VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Svg)

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
    core/symbolid.h
    core/symbollibrary.h
    core/symbollibrary.cpp
    core/symbollayouts.cpp
    core/symbolmanifest.h
    core/symbolmanifest.cpp
    core/symbolsvg.h
    core/symbolsvg.cpp
    core/component.h
    core/propertyjustification.h
    core/component.cpp
    core/terminalref.h
    core/wireendpoint.h
    core/pendingwireendpoint.h

    core/wireroute.h
    core/routegeometry.h
    core/routegeometry.cpp
    core/document.h
    core/document.cpp
    core/textnote.h
    core/documentnotes.cpp
    core/documentloadresult.h
    core/documentjson.cpp
    core/documentfragment.cpp
)

target_include_directories(schematiccore PUBLIC core)

target_link_libraries(schematiccore
    PUBLIC
        Qt6::Core
)

# The built-in symbol library: one JSON file per symbol plus a manifest that
# fixes palette order, embedded as Qt resources under :/symbols. On a static
# library Qt links the compiled resources into every consumer (the app and
# the test executable). Add a symbol by adding its file here and to
# manifest.json.
qt_add_resources(schematiccore symbol_library
    PREFIX "/symbols"
    BASE core/symbols
    FILES
        core/symbols/manifest.json
        core/symbols/valve-triode-with-heaters.json
        core/symbols/valve-triode-no-heaters.json
        core/symbols/valve-pentode-with-heaters.json
        core/symbols/valve-pentode-no-heaters.json
        core/symbols/valve-pentode-cathode-suppressor-with-heaters.json
        core/symbols/valve-pentode-cathode-suppressor-no-heaters.json
        core/symbols/valve-beam-tetrode-with-heaters.json
        core/symbols/valve-beam-tetrode-no-heaters.json
        core/symbols/valve-diode-with-heaters.json
        core/symbols/valve-diode-no-heaters.json
        core/symbols/resistor.json
        core/symbols/capacitor.json
        core/symbols/electrolytic-capacitor.json
        core/symbols/diode.json
        core/symbols/schottky-diode.json
        core/symbols/zener-diode.json
        core/symbols/led.json
        core/symbols/pnp.json
        core/symbols/npn.json
        core/symbols/n-channel-jfet.json
        core/symbols/p-channel-jfet.json
        core/symbols/n-channel-mosfet.json
        core/symbols/p-channel-mosfet.json
        core/symbols/ground.json
        core/symbols/rail-vcc.json
        core/symbols/ground-chassis.json
        core/symbols/ground-signal.json
        core/symbols/port-input.json
        core/symbols/port-output.json
        core/symbols/port-bidirectional.json
        core/symbols/opamp-grid10.json
        core/symbols/opamp-no-supplies.json
        core/symbols/ota-with-supplies.json
        core/symbols/ota-with-supplies-bias-above.json
        core/symbols/ota-no-supplies.json
        core/symbols/ota-no-supplies-bias-above.json
        core/symbols/darlington-buffer-lm13700.json
        core/symbols/potentiometer.json
        core/symbols/preset-resistor.json
        core/symbols/jack.json
        core/symbols/block-high-pass.json
        core/symbols/block-low-pass.json
        core/symbols/block-band-pass.json
        core/symbols/block-buffer.json
        core/symbols/block-summing-node.json
        core/symbols/block-band-stop.json
        core/symbols/block-variable-high-pass.json
        core/symbols/block-variable-low-pass.json
        core/symbols/block-variable-band-pass.json
        core/symbols/block-variable-band-stop.json
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
    symbolmimedata.h
    symbolrenderer.cpp
    symbolrenderer.h
    schematicsvgexporter.cpp
    schematicsvgexporter.h
    symbolpackagestore.cpp
    symbolpackagestore.h
    symbolimportdialog.cpp
    symbolimportdialog.h
    symbolpreviewwidget.cpp
    symbolpreviewwidget.h
    documenteditcontroller.cpp
    documenteditcontroller.h
    documentselection.h
    propertiespanel.cpp
    propertiespanel.h
    preferencesdialog.cpp
    preferencesdialog.h
)

target_compile_definitions(Schematic PRIVATE SCHEMATIC_VERSION="${PROJECT_VERSION}")

target_link_libraries(Schematic
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Svg
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

set(_schematic_runtime_deploy_options)
if(MSVC)
    # Collect app-local CRT DLLs without requiring VCINSTALLDIR in the shell.
    include(InstallRequiredSystemLibraries)
    set(_schematic_runtime_deploy_options NO_COMPILER_RUNTIME)
endif()
qt_generate_deploy_app_script(
    TARGET Schematic
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    ${_schematic_runtime_deploy_options}
)
install(SCRIPT ${deploy_script})

include(cmake/Packaging.cmake)
