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

    // Completing a wire that branches off an existing route must apply the
    // branch split and append the new wire as one atomic change.
    void addWireBranchingAppliesSplitsAtomically();

    // Branching twice from the same wire in one call is rejected (a current
    // scope limit), and any other failure must leave wires, nodes, and ID
    // allocation completely unchanged - no orphaned split, no consumed id.
    void addWireBranchingFailureLeavesDocumentUnchanged();

    // setComponentLabels() trims both fields before validation/storage, so
    // "R1" and " R1 " are the same reference and an all-whitespace
    // reference is treated as empty.
    void setComponentLabelsTrimsBeforeValidationAndStorage();

    // An empty (or all-whitespace) reference is rejected without changing
    // the component's existing labels.
    void setComponentLabelsRejectsEmptyReference();

    // A reference that collides with another component's (trimmed)
    // reference is rejected, but renaming a component to its own current
    // reference succeeds.
    void setComponentLabelsRejectsDuplicateReferenceButAllowsSelfRename();

    // Editing only the value (same reference) succeeds and leaves the
    // reference untouched.
    void setComponentLabelsValueOnlyEditSucceeds();

    // The shared validator rejects a collinear route that reverses direction
    // (horizontal or vertical, at any bend) but still accepts a collinear
    // route that keeps going the same way.
    void routeValidationRejectsCollinearBacktracking();

    // Endpoint movement uses the same validator: a move that would leave a
    // reversing route is rejected and leaves the component and route intact.
    void movementRejectsCollinearReversal();

    // A branching wire whose completed route reverses is rejected without
    // splitting the wire, adding a node, or consuming IDs.
    void addWireBranchingRejectsReversalWithoutSideEffects();
};

#endif // TST_DOCUMENT_H
