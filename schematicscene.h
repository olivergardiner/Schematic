#ifndef SCHEMATICSCENE_H
#define SCHEMATICSCENE_H

#include <QGraphicsScene>

// SchematicScene hosts the schematic symbols, wires and annotations that
// make up a drawing. For now it simply establishes a sensible default
// canvas size; symbol/wire item classes will be added here as the
// application grows.
class SchematicScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit SchematicScene(QObject *parent = nullptr);
};

#endif // SCHEMATICSCENE_H
