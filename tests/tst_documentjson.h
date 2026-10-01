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
    // Missing, empty and whitespace-only references are generated from the
    // kind's prefix (with a warning each), trimmed, and never collide with
    // explicit references - including ones later in the file.
    void generatesMissingReferences();
    // Duplicate non-empty references (after trimming, case-sensitive) are a
    // load error naming both components; they are never silently renamed.
    void rejectsDuplicateReferences();
    // Grid spacing finer than two decimals survives save/load unchanged.
    void preservesPreciseGridSpacing();
    // Nodes are written in ascending ID order, so equal documents give
    // byte-identical JSON.
    void serializesNodesInStableOrder();
};

#endif
