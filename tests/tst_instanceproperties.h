#ifndef TST_INSTANCEPROPERTIES_H
#define TST_INSTANCEPROPERTIES_H
#include <QObject>
class TstInstanceProperties : public QObject
{
    Q_OBJECT
private slots:
    void propertyBlockPersistenceAndTransforms();
    void clipboardConnectivityAndAtomicPaste();
    void clipboardCoincidentEndpointsStaySeparate();
    void defaultsAndInstanceVisibility();
    void bulkEditsAreAtomicAndPreserveOtherFields();
    void v5ValidationAndLegacyMigration();
};
#endif
