#ifndef SCHEMATICSCENE_H
#define SCHEMATICSCENE_H

#include "identifiers.h"
#include "pendingwireendpoint.h"
#include "symbolkind.h"

#include <QGraphicsScene>

#include <optional>

class Document;
class Component;
struct WireRoute;
class ComponentItem;
class ComponentLabelItem;

// SchematicScene hosts the schematic symbols, wires and annotations that
// make up a drawing, and (from step 5) interprets mouse/keyboard input as
// edits against a bound Document. Per AGENTS.md "keep the schematic
// document model independent of Qt rendering items and input handling",
// this class is a renderer/input *adapter* only: MainWindow owns the one
// real Document instance; every edit here calls straight into
// *m_document, and only after a Document mutator commits does the scene
// rebuild its items from the (now-updated) Document - the Document is
// never treated as scene-owned state.
class SchematicScene : public QGraphicsScene
{
    Q_OBJECT

public:
    enum class EditMode
    {
        Select,
        PlaceComponent,
        DrawWire,
    };

    explicit SchematicScene(QObject *parent = nullptr);

    // Binds document as the scene's live, mutable data source and performs
    // an immediate full rebuild from it. Every interactive handler
    // (place/move/delete/draw-wire) calls directly into *document; after
    // any committed change the scene rebuilds its items from *document
    // again. Passing nullptr unbinds the scene (clears all items; every
    // interactive handler becomes a no-op).
    //
    // Lifetime requirement: the scene does not own *document and performs
    // no lifetime management of it. The caller must call bindDocument()
    // again (with a new pointer, or nullptr) *before* the previously bound
    // Document instance is destroyed, moved, or reallocated - e.g. before
    // replacing MainWindow's Document member wholesale for a new/opened
    // file (step 6). Holding onto a stale pointer past that point is
    // undefined behaviour; this class has no way to detect it.
    void bindDocument(Document *document);

    EditMode editMode() const { return m_mode; }
    void setEditMode(EditMode mode);

    // The SymbolKind placed by a click while editMode() == PlaceComponent.
    void setPlaceComponentKind(SymbolKind kind) { m_placeKind = kind; }

    // Discards any in-progress wire (see DrawWire mode) with zero Document
    // calls - equivalent to pressing Escape. Exposed so MainWindow can
    // cancel a pending wire when the user switches away from DrawWire mode
    // via the toolbar rather than the keyboard.
    void cancelPendingWire();

    // Returns the id of the single selected ComponentItem, or std::nullopt
    // if zero or more than one component is currently selected. Used by
    // MainWindow to drive the Rename (F2) action's enabled state and target
    // - see AGENTS.md/CLAUDE.md step 6 plan.
    std::optional<ComponentId> singleSelectedComponent() const;

    // Rebuilds scene items from the bound Document and emits
    // documentEdited(). For edits MainWindow makes directly against the
    // Document outside of this class's own mouse/keyboard handlers (e.g.
    // Document::setComponentLabels() from the label-edit dialog or the
    // Rename action) - see AGENTS.md/CLAUDE.md step 6 plan. A no-op if no
    // document is bound.
    void refreshAfterExternalEdit();

signals:
    // Emitted after any interactive edit is successfully committed to the
    // bound Document (place, move, delete, draw/branch a wire, or grid
    // spacing - see MainWindow). Not emitted for edits that Document
    // rejected, nor for the initial bindDocument() rebuild.
    void documentEdited();
    // A short, user-facing explanation of why the in-progress interaction
    // did nothing (e.g. "Start a wire from a terminal, node, or existing
    // wire.") - MainWindow may surface this in a status bar.
    void statusMessage(const QString &message);
    // Emitted when a ComponentItem or its label is double-clicked in
    // Select mode - MainWindow opens the reference/value edit dialog for
    // componentId in response (see MainWindow::onEditComponentLabels()).
    void componentDoubleClicked(ComponentId componentId);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    enum class HitKind { None, Terminal, Node, WireSegment };
    struct HitResult
    {
        HitKind kind = HitKind::None;
        TerminalRef terminal;
        NodeId node = kInvalidNodeId;
        WireId wire = kInvalidWireId;
        QPointF point;
    };

    void rebuild();
    void addWireItems(const WireRoute &route);
    void addJunctionItem(QPointF point);
    void addComponentItems(const Component &component);

    QPointF snapToGrid(QPointF point) const;
    qreal hitToleranceInSceneUnits() const;
    HitResult hitTestConnection(QPointF scenePos) const;
    // Finds the enclosing ComponentItem/ComponentLabelItem (if any) for an
    // item returned by itemAt(), so a click on either a symbol or its
    // labels drives the same select/drag/delete behaviour.
    ComponentItem *componentItemAt(QPointF scenePos) const;

    void beginDrag(ComponentItem *item, QPointF scenePos);
    void updateDrag(QPointF scenePos);
    void endDrag();

    void extendPendingWire(QPointF scenePos);
    void deleteSelection();

    Document *m_document = nullptr;
    EditMode m_mode = EditMode::Select;
    SymbolKind m_placeKind = SymbolKind::Resistor;

    // Select-mode manual drag state (see AGENTS.md/CLAUDE.md step 5 plan:
    // preview by moving only the dragged symbol's items, attached wires
    // reshape only once Document::moveComponent() commits on release).
    bool m_dragging = false;
    ComponentId m_dragComponentId = kInvalidComponentId;
    QPointF m_dragOriginalPosition;
    QPointF m_dragPressScenePos;
    QPointF m_dragCurrentSnappedPosition;
    // The dragged component's artwork group plus its label items, moved
    // together by the same preview delta - see beginDrag()/updateDrag().
    // m_dragItemOrigins holds each item's pre-drag scene position, parallel
    // to m_dragItems by index.
    QVector<QGraphicsItem *> m_dragItems;
    QVector<QPointF> m_dragItemOrigins;

    // DrawWire-mode pending-wire state. m_pendingVertices always starts
    // with the resolved start point; m_pendingStart is only meaningful
    // while m_wireActive is true.
    bool m_wireActive = false;
    PendingWireEndpoint m_pendingStart;
    QVector<QPointF> m_pendingVertices;
    QGraphicsPathItem *m_wirePreviewItem = nullptr;
};

#endif // SCHEMATICSCENE_H
