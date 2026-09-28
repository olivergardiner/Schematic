// Combined entry point for schematiccore_tests. Each tst_*.h/.cpp pair is a
// self-contained QObject-based test class (no QTEST_MAIN in the individual
// files) so that multiple test classes can be run from one executable/CTest
// entry, following the same event-loop-free pattern QTest::qExec supports.
// As the model grows, add one QTest::qExec<T>(argc, argv) line per new
// tst_*.h class here rather than registering a separate CTest executable
// per file.

#include "tst_symboldefinition.h"
#include "tst_component.h"

#include <QTest>

int main(int argc, char *argv[])
{
    int status = 0;

    {
        TstSymbolDefinition test;
        status |= QTest::qExec(&test, argc, argv);
    }
    {
        TstComponent test;
        status |= QTest::qExec(&test, argc, argv);
    }

    return status;
}
