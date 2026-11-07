#ifndef SCHEMATICSCENE_H
#define SCHEMATICSCENE_H

#include "identifiers.h"
#include "pendingwireendpoint.h"
#include "symbolid.h"
#include "documentselection.h"
#include "document.h"

#include <QGraphicsScene>

#include <functional>
#include <optional>
#include <utility>
#include <QVector>

class Document;
class Component;
struct WireRoute;
class ComponentItem;
class ComponentLabelItem;

// SchematicScene hosts the schematic symbols, wires and annotations that
// make up a drawing, and interprets mouse/keyboard input as
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
    using Selection = DocumentSelection;
    using EditCommitter = std::function<bool(const QString &, const std::function<bool()> &,
                                             std::function<Selection()>)>;

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
    // file. Holding onto a stale pointer past that point is
    // undefined behaviour; this class has no way to detect it.
    void bindDocument(Document *document);
    void setEditCommitter(EditCommitter committer) { m_editCommitter = std::move(committer); }

    EditMode editMode() const { return m_mode; }
    void setEditMode(EditMode mode);

    // Adds one snapped symbol instance through the same undoable path as
    // click-to-place. Used by palette drops without changing the active mode.
    bool placeComponentAt(const SymbolId &id, QPointF scenePos);

    // The symbol placed by a click while editMode() == PlaceComponent.
    void setPlaceComponentKind(const SymbolId &id) { m_placeKind = id; }

    // Discards any in-progress wire (see DrawWire mode) with zero Document
    // calls - equivalent to pressing Escape. Exposed so MainWindow can
    // cancel a pending wire when the user switches away from DrawWire mode
    // via the toolbar rather than the keyboard.
    void cancelPendingWire();

    // Returns the id of the single selected ComponentItem, or std::nullopt
    // if zero or more than one component is currently selected. Used by
    // MainWindow to drive the Rename (F2) action's enabled state and target.
    std::optional<ComponentId> singleSelectedComponent() const;
    QVector<ComponentId> selectedComponentIds() const;

    Selection selection() const;
    void restoreSelection(const Selection &selection);
    void selectInBox(const QRectF &rect, bool crossing, const Selection &baseline);
    bool canStartSelectionBox(QPointF position) const;

    // Restores a model snapshot in place, then rebuilds and reselects the
    // listed component and wire identities that still exist.
    void restoreAfterDocumentChange(const Selection &selection);
    bool transformSelected(SelectionTransform::Kind kind, int quarterTurns = 0);
    bool moveInProgress() const { return m_dragging; }
    void cancelMove();
    void deleteSelection();
    bool resetPropertyPositions(const QVector<ComponentId> &ids);

signals:
    // A canvas selection gesture starts before its eventual selection change.
    void selectionInteractionStarted();
    void moveStateChanged();
    // Cancel transient view gestures before mode changes or item replacement.
    void selectionGestureReset();
    // A short, user-facing explanation of why the in-progress interaction
    // did nothing (e.g. "Start a wire from a terminal, node, or existing
    // wire.") - MainWindow may surface this in a status bar.
    void statusMessage(const QString &message);
    void editModeChanged(SchematicScene::EditMode mode);
    // Emitted when a ComponentItem or its label is double-clicked in
    // Select mode - MainWindow reveals the Properties dock for
    // componentId in response (see MainWindow::onEditComponentLabels()).
    void componentDoubleClicked(ComponentId componentId);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;

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

    void rebuild(const Selection *selectionOverride = nullptr);
    void renderDocument(const Document &document, const Selection &selection);
    void addWireItems(const WireRoute &route);
    void addJunctionItem(QPointF point);
    void addComponentItems(const Component &component, const Document &document);

    QPointF snapToGrid(QPointF point) const;
    qreal hitToleranceInSceneUnits() const;
    HitResult hitTestConnection(QPointF scenePos) const;
    void setWireHoverCandidate(bool visible, QPointF point = {});
    // Finds the enclosing ComponentItem/ComponentLabelItem (if any) for an
    // item returned by itemAt(), so a click on either a symbol or its
    // labels drives the same select/drag/delete behaviour.
    ComponentItem *componentItemAt(QPointF scenePos) const;

    void beginDrag(ComponentItem *item, QPointF scenePos);
    void updateDrag(QPointF scenePos);
    void endDrag();

    void extendPendingWire(QPointF scenePos);

    bool commitEdit(const QString &description, const std::function<bool()> &operation,
                    std::function<Selection()> afterSelection = {});

    Document *m_document = nullptr;
    EditCommitter m_editCommitter;
    EditMode m_mode = EditMode::Select;
    SymbolId m_placeKind = SymbolIds::Resistor;

    // Preview comes from the original snapshot, never the live document.
    bool m_dragging = false;
    bool m_propertyDragging = false;
    QPointF m_propertyOriginalOffset;
    bool m_dragThresholdPassed = false;
    bool m_dragCandidateValid = true;
    ComponentId m_dragComponentId = kInvalidComponentId;
    QPointF m_dragOriginalPosition;
    QPointF m_dragPressScenePos;
    QPointF m_dragDisplacement;
    Selection m_dragSelection;
    std::optional<Document> m_dragOriginal;

    // DrawWire-mode pending-wire state. m_pendingVertices always starts
    // with the resolved start point; m_pendingStart is only meaningful
    // while m_wireActive is true.
    bool m_wireActive = false;
    PendingWireEndpoint m_pendingStart;
    QVector<QPointF> m_pendingVertices;
    QGraphicsPathItem *m_wirePreviewItem = nullptr;
    bool m_hasWireHoverCandidate = false;
    QPointF m_wireHoverCandidate;
};

#endif // SCHEMATICSCENE_H
