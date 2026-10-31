# Tests for schematiccore. Kept as a separate, headless executable (Core +
# Test only - no Gui/Widgets) so the document model can be verified without
# a display/QPA platform plugin. See AGENTS.md: "Prefer focused tests for
# connectivity, moving connected components, and save/load".
qt_add_executable(schematiccore_tests
    main.cpp
    tst_symbollibrary.h
    tst_symbollibrary.cpp
    tst_component.h
    tst_component.cpp
    tst_document.h
    tst_document.cpp
    tst_documentjson.h
    tst_documentjson.cpp
)

target_link_libraries(schematiccore_tests
    PRIVATE
        Qt6::Core
        Qt6::Test
        schematiccore
)

add_test(NAME schematiccore_tests COMMAND schematiccore_tests)

# The application-layer undo controller uses QUndoStack (Qt Gui), while the
# schematiccore tests above remain Core + Test only.
qt_add_executable(schematicundo_tests
    tst_documenteditcontroller.cpp
    ../documenteditcontroller.cpp
    ../documenteditcontroller.h
)

target_link_libraries(schematicundo_tests
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
        schematiccore
)

add_test(NAME schematicundo_tests COMMAND schematicundo_tests)

# Shared symbol artwork conversion and palette preview painting. QImage keeps
# these renderer checks headless while still exercising Qt Gui painting.
qt_add_executable(schematicrenderer_tests
    tst_symbolrenderer.cpp
    ../symbolrenderer.cpp
    ../symbolrenderer.h
    ../symbolmimedata.h
)

target_link_libraries(schematicrenderer_tests
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
        schematiccore
)

add_test(NAME schematicrenderer_tests COMMAND schematicrenderer_tests)

# Scene-level placement checks exercise the snapped, selected, undoable drop
# placement helper without requiring a visible window.
qt_add_executable(schematicplacement_tests
    tst_symbolplacement.cpp
    ../documenteditcontroller.cpp
    ../documenteditcontroller.h
    ../schematicscene.cpp
    ../schematicscene.h
    ../symbolrenderer.cpp
    ../symbolrenderer.h
    ../schematicitems.h
    ../symbolmimedata.h
)

target_link_libraries(schematicplacement_tests
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Test
        schematiccore
)

add_test(NAME schematicplacement_tests COMMAND schematicplacement_tests)
set_tests_properties(schematicplacement_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
)
