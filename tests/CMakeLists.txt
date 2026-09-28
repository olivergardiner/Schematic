# Tests for schematiccore. Kept as a separate, headless executable (Core +
# Test only - no Gui/Widgets) so the document model can be verified without
# a display/QPA platform plugin. See AGENTS.md: "Prefer focused tests for
# connectivity, moving connected components, and save/load".
qt_add_executable(schematiccore_tests
    main.cpp
    tst_symboldefinition.h
    tst_symboldefinition.cpp
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
