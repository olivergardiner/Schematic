#ifndef PENDINGWIREENDPOINT_H
#define PENDINGWIREENDPOINT_H

#include "wireendpoint.h"

#include <QPointF>

#include <variant>

// A not-yet-materialized branch point on an existing wire: the split is
// only actually applied to the document if the wire being constructed
// around it validates - see Document::addWireBranching(). A BranchSite is
// never persisted and never appears inside a committed WireRoute; it
// exists only for the duration of one addWireBranching() call.
struct BranchSite
{
    WireId wire = kInvalidWireId;
    QPointF point;
};

// Either a genuine, already-existing identity (see WireEndpoint) or a
// pending branch site whose split has not yet been applied to the
// document. Kept distinct from WireEndpoint itself so a persisted
// WireRoute's start/end can never accidentally reference a not-yet-real
// branch - see DECISIONS.md "Document model and connectivity".
using PendingWireEndpoint = std::variant<WireEndpoint, BranchSite>;

inline bool isBranchSite(const PendingWireEndpoint &endpoint)
{
    return std::holds_alternative<BranchSite>(endpoint);
}

inline bool isResolvedEndpoint(const PendingWireEndpoint &endpoint)
{
    return std::holds_alternative<WireEndpoint>(endpoint);
}

#endif // PENDINGWIREENDPOINT_H
