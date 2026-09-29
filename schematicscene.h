#ifndef SCHEMATICSCENE_H
#define SCHEMATICSCENE_H

#include <QGraphicsScene>

class Document;
class Component;
struct WireRoute;

// SchematicScene hosts the schematic symbols, wires and annotations that
// make up a drawing. setDocument() performs a full clear-and-rebuild of the
// graphics items from a Document snapshot; the Document remains the sole
// source of truth (see AGENTS.md "keep the schematic document model
// independent of Qt rendering items and input handling") - this class only
// ever displays it, never mutates it.
//
// Step 4 scope: read-only rendering only. No selection, move, delete, or
// grid-spacing wiring live here - see SchematicView for the existing fixed
// background grid, and later steps for interactive editing and a
// configurable grid control.
class SchematicScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit SchematicScene(QObject *parent = nullptr);

    // Clears all existing items and rebuilds the scene from a snapshot of
    // document. This always does a full teardown-and-rebuild - the simplest
    // correct approach for a read-only milestone. Whether a later
    // interactive-editing step needs an incremental update path instead is
    // an open question left to that step, not assumed here.
    void setDocument(const Document &document);

private:
    void addWireItems(const WireRoute &route);
    void addJunctionItem(QPointF point);
    void addComponentItems(const Component &component);
};

#endif // SCHEMATICSCENE_H
