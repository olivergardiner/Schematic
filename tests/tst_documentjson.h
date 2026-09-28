#ifndef TST_DOCUMENTJSON_H
#define TST_DOCUMENTJSON_H

#include <QObject>

class TstDocumentJson : public QObject
{
    Q_OBJECT
private slots:
    // Stable round trips must preserve topology and visible route geometry.
    void roundTripsBranchedDocument();
    // Bad input must fail atomically and report endpoint corrections.
    void rejectsInvalidAndRepairsStaleEndpoints();
    // IDs and endpoint references are validated before a document is returned.
    void rejectsInvalidIdsReferencesAndGeometry();
    // Unused node identities are not retained as phantom topology.
    void discardsOrphanNodes();
};

#endif
