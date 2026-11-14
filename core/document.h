#ifndef DOCUMENT_H
#define DOCUMENT_H

#include "component.h"
#include "symbollibrary.h"
#include "pendingwireendpoint.h"
#include "routegeometry.h"
#include "wireroute.h"
#include "textnote.h"

#include <QHash>
#include <QByteArray>
#include <QJsonObject>
#include <QPointF>
#include <QVector>

#include <optional>

struct DocumentLoadResult;

enum class DrawingStyle { Standard, HandDrawn };
enum class DrawingVariation { Individual, Consistent };

enum class FlipAxis { Horizontal, Vertical };

struct SelectionTransform
{
    enum class Kind { Translate, Rotate, FlipHorizontal, FlipVertical };
    Kind kind = Kind::Translate;
    QPointF displacement;
    QPointF pivot;
    int quarterTurns = 0; // Positive is clockwise in canvas coordinates.
};

struct Net
{
    QVector<TerminalRef> terminals;
};

struct PropertyEdit
{
    enum class Kind { SetValue, SetVisible, Add, Remove, Rename };
    Kind kind = Kind::SetValue;
    QString id;
    QString name;
    QString value;
    bool visible = false;
};

class Document
{
public:
    const LibrarySymbol *symbol(const SymbolId &id) const { return m_symbols.find(id); }
    int terminalCount(const Component &item) const;
    QPointF terminalPosition(const Component &item, TerminalId terminal) const;
    // Replace presentation only; a used ID can never acquire different terminals.
    bool useSymbol(const LibrarySymbol &definition, QStringList *errors);
    bool setSymbolLayout(ComponentId id, const QString &controlKey, const QString &choiceKey,
                         QStringList *errors = nullptr);
    bool canSetSymbolLayout(ComponentId id, const QString &controlKey, const QString &choiceKey) const;
    DrawingStyle drawingStyle() const { return m_drawingStyle; }
    bool setDrawingStyle(DrawingStyle style);
    DrawingVariation drawingVariation() const { return m_drawingVariation; }
    bool setDrawingVariation(DrawingVariation variation);
    int drawingStrokes() const { return m_drawingStrokes; }
    bool setDrawingStrokes(int strokes);
    qreal drawingLooseness() const { return m_drawingLooseness; }
    bool setDrawingLooseness(qreal looseness);
    qreal gridSpacing() const { return m_gridSpacing; }
    bool setGridSpacing(qreal spacing);

    const QVector<Component> &components() const { return m_components; }
    const QVector<WireRoute> &wires() const { return m_wires; }
    const QHash<NodeId, QPointF> &nodes() const { return m_nodes; }

    const Component *component(ComponentId id) const;
    const WireRoute *wire(WireId id) const;
    const QVector<TextNote> &notes() const { return m_notes; }
    const TextNote *note(NoteId id) const;
    NoteId addNote(QPointF position, const QString &text = QStringLiteral("Note"), qreal width = 160.0);
    bool setNote(NoteId id, QPointF position, const QString &text, qreal width);
    bool setNoteAppearance(NoteId id, PropertyJustification justification,
                           const QString &fillColor, const QString &outlineColor);
    bool removeNote(NoteId id);

    // Returns kInvalidComponentId (document unchanged, no id consumed) if
    // symbolId is absent from both the document and active library.
    ComponentId addComponent(const SymbolId &symbolId, QPointF position,
                             Rotation rotation = Rotation::Deg0);
    // User placement: atomically add a component and connect terminals that
    // touch an unambiguous wire interior or bare junction. No proximity
    // snapping here; the caller supplies the final scene position. Ambiguous
    // contacts stay unconnected and are reported in warnings. Failures leave
    // the document (including identity allocation) unchanged.
    ComponentId placeComponent(const SymbolId &symbolId, QPointF position,
                               Rotation rotation = Rotation::Deg0,
                               QStringList *warnings = nullptr);
    bool moveComponent(ComponentId id, QPointF position);
    // Preserve incident routes, replacing each used terminal by one bare
    // node at that terminal's position. Different terminals stay separate.
    bool removeComponent(ComponentId id);
    // Atomic Delete/Cut: remove explicitly selected wires, then detach the
    // selected components. A failed detachment leaves everything unchanged.
    bool removeSelection(const QVector<ComponentId> &components, const QVector<WireId> &wires,
                         const QVector<NoteId> &notes = {});
    // Transactional: rejects (returns false, document unchanged) if id is
    // unknown or any attached route would become invalid.
    bool rotateComponent(ComponentId id, Rotation rotation);
    bool setComponentMirrored(ComponentId id, bool mirrored);
    bool setComponentTransform(ComponentId id, Rotation rotation, bool mirrored);
    bool flipComponent(ComponentId id, FlipAxis axis);

    // Atomic, identity-based group edit. All candidates derive from the
    // same original state and only final route geometry is validated.
    bool transformSelection(const QVector<ComponentId> &components,
                            const QVector<WireId> &wires,
                            const SelectionTransform &transform, const QVector<NoteId> &notes = {});


    // Renames/re-values an already-placed component. Both reference and
    // value are trimmed before validation and storage (so "R1" and " R1 "
    // are treated as the same reference, and an all-whitespace reference is
    // treated as empty). Rejected (returning false, leaving the document
    // completely unchanged) if id does not exist, the trimmed reference is
    // empty, or the trimmed reference collides with another component's
    // trimmed reference (case-sensitive; renaming a component to its own
    // current reference is allowed). value has no uniqueness constraint and
    // may be empty after trimming - see DECISIONS.md "Symbols and labels".
    bool setComponentLabels(ComponentId id, const QString &reference, const QString &value);
    bool setPropertyBlockOffset(const QVector<ComponentId> &ids, std::optional<QPointF> offset);
    bool setPropertyBlockJustification(const QVector<ComponentId> &ids, PropertyJustification value);

    bool editProperties(const QVector<ComponentId> &ids, const QVector<PropertyEdit> &edits,
                        QStringList *errors = nullptr);

    std::optional<WireId> addWire(const WireEndpoint &start,
                                  const QVector<QPointF> &interiorVertices,
                                  const WireEndpoint &end);
    bool removeWire(WireId id);
    // Move one segment perpendicular to its axis to an absolute scene
    // coordinate (collinear neighbours form one straight run).
    // Endpoints (including shared nodes) stay fixed; end
    // segments acquire connecting bends. Invalid/no-op edits return false.
    bool moveWireSegment(WireId id, int segment, qreal coordinate);
    std::optional<NodeId> branchWireAt(WireId id, QPointF point);

    // Atomic wire completion: resolves each endpoint (materializing any
    // BranchSite's split only if the whole operation succeeds), validates
    // the full candidate route, and only then mutates the document - see
    // DECISIONS.md "Document model and connectivity". Rejects (returning
    // std::nullopt with the document completely unchanged, including no ID
    // allocation) if either endpoint fails to resolve, both endpoints are
    // BranchSites on the same WireId (currently unsupported - draw two
    // wires instead), a BranchSite's point does not lie on its wire, or the
    // resulting route/split geometry is invalid.
    std::optional<WireId> addWireBranching(const PendingWireEndpoint &start,
                                           const QVector<QPointF> &interiorVertices,
                                           const PendingWireEndpoint &end);
    // Atomic free-end completion. The new bare node (and any branch at
    // start) is materialized only if the whole route is valid.
    std::optional<WireId> addWireToPoint(const PendingWireEndpoint &start,
                                       const QVector<QPointF> &interiorVertices, QPointF end);

    QByteArray copyFragment(const QVector<ComponentId> &components, const QVector<WireId> &wires,
                            QStringList *errors = nullptr, const QVector<NoteId> &notes = {}) const;
    bool pasteFragment(const QByteArray &bytes, QPointF displacement,
                       QVector<ComponentId> *components = nullptr, QVector<WireId> *wires = nullptr,
                       QStringList *errors = nullptr, QVector<NoteId> *notes = nullptr);
    // Centre component origins and wire vertices at target with one shared
    // translation, preserving all relative coordinates and connectivity.
    bool pasteFragmentAt(const QByteArray &bytes, QPointF target,
                         QVector<ComponentId> *components = nullptr, QVector<WireId> *wires = nullptr,
                         QStringList *errors = nullptr, QVector<NoteId> *notes = nullptr);

    QVector<Net> computeNets() const;
    QVector<QPointF> junctionPoints() const;

    // Version 13 adds drawing looseness. Versions 1-12 remain readable.
    // Nets, junctions and note layout heights are derived, never serialized.
    QJsonObject toJson() const;
    QByteArray toJsonBytes() const;
    static DocumentLoadResult fromJson(const QJsonObject &root);
    static DocumentLoadResult fromJsonBytes(const QByteArray &data);

private:
    bool pasteFragmentImpl(const QByteArray &bytes, QPointF position, bool atTarget,
                           QVector<ComponentId> *components, QVector<WireId> *wires, QStringList *errors,
                           QVector<NoteId> *notes);
    bool endpointPosition(const WireEndpoint &endpoint, QPointF *position) const;
    bool endpointExists(const WireEndpoint &endpoint) const;
    void removeOrphanNodes();
    WireId allocateWireId();
    bool commitComponentChange(ComponentId id, const Component &changed);
    bool ensureLayoutClosure(const SymbolId &id, QStringList *errors);

    // Pure (non-mutating) computation of a branch split: finds the segment
    // of wire id containing point and returns the two resulting vertex
    // lists and the original route's endpoints, without allocating any ID
    // or touching m_wires/m_nodes. Shared by branchWireAt() and
    // addWireBranching() so both use identical split geometry rules.
    bool computeBranchSplit(WireId id, QPointF point, QVector<QPointF> *leftPoints,
                            QVector<QPointF> *rightPoints, WireEndpoint *originalStart,
                            WireEndpoint *originalEnd) const;
    // Mutates m_wires/m_nodes to actually apply a previously computed split.
    // Callers must have already reserved node/leftId/rightId (e.g. via
    // idsAvailable()) - this function performs no validation and cannot
    // fail, which is what lets addWireBranching() apply two splits and a
    // new wire as one all-or-nothing sequence.
    void commitBranchSplit(WireId id, QPointF point, const QVector<QPointF> &leftPoints,
                           const QVector<QPointF> &rightPoints, const WireEndpoint &originalStart,
                           const WireEndpoint &originalEnd, NodeId node, WireId leftId,
                           WireId rightId);
    // True if count sequential IDs starting at nextId can be allocated
    // without hitting the invalid-ID sentinel (0) or wrapping past
    // quint32's range. Used to pre-flight every ID an addWireBranching()
    // call might need before any mutation begins.
    static bool idsAvailable(quint32 nextId, int count);

    SymbolLibrary m_symbols;
    DrawingStyle m_drawingStyle = DrawingStyle::Standard;
    DrawingVariation m_drawingVariation = DrawingVariation::Individual;
    int m_drawingStrokes = 1;
    qreal m_drawingLooseness = 1.0;
    qreal m_gridSpacing = 10.0;
    ComponentId m_nextComponentId = 1;
    WireId m_nextWireId = 1;
    NodeId m_nextNodeId = 1;
    NoteId m_nextNoteId = 1;
    QVector<TextNote> m_notes;
    QVector<Component> m_components;
    QVector<WireRoute> m_wires;
    QHash<NodeId, QPointF> m_nodes;
};

#endif // DOCUMENT_H
