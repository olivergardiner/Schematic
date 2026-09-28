#ifndef TST_DOCUMENT_H
#define TST_DOCUMENT_H

#include <QObject>

class TstDocument : public QObject
{
    Q_OBJECT

private slots:
    // Explicit endpoint identity must join terminals while coordinate
    // crossings without shared identities remain electrically separate.
    void connectivityUsesEndpointIdentity();

    // Moving a component must preserve an orthogonal route, its opposite
    // endpoint, and the electrical net represented by its identities.
    void movementPreservesOrthogonalRoutesAndNets();

    // A move that would collapse a direct route must be rejected atomically
    // so a connection cannot disappear or become zero length.
    void coincidentDirectEndpointsRejectMove();

    // Redundant vertices should collapse when an endpoint moves onto a
    // corner, while leaving a valid route to the next distinct point.
    void movementCollapsesRedundantCorner();

    // Removing a component removes its attached route legs and recomputes
    // junction display from the remaining identity references.
    void componentDeletionCascadesAcrossBranches();

    // Branching must split a route at an existing corner or segment point
    // and explicitly share one newly allocated node identity.
    void branchingSplitsRoutesAtInteriorPoints();

    // Invalid route geometry and unresolved endpoints must be rejected
    // without changing document contents.
    void invalidRoutesAreRejectedAtomically();

    // A grid increment is a positive finite world-unit value and invalid
    // edits must not replace the last valid setting.
    void gridSpacingMustBePositiveAndFinite();
};

#endif // TST_DOCUMENT_H
