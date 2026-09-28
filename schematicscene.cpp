#include "schematicscene.h"

namespace {
// A generous default canvas in scene units (treated as mm at 1 unit == 1 mm)
// so a typical A4/A3-ish schematic sheet fits comfortably without the user
// needing to resize anything up front.
constexpr qreal kDefaultWidth  = 2000.0;
constexpr qreal kDefaultHeight = 1500.0;
} // namespace

SchematicScene::SchematicScene(QObject *parent)
    : QGraphicsScene(parent)
{
    setSceneRect(-kDefaultWidth / 2.0, -kDefaultHeight / 2.0, kDefaultWidth, kDefaultHeight);
    setBackgroundBrush(Qt::white);
}
