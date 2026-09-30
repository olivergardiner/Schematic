#ifndef SCHEMATICITEMS_H
#define SCHEMATICITEMS_H

#include "identifiers.h"

#include <QGraphicsItemGroup>
#include <QGraphicsPathItem>
#include <QGraphicsSimpleTextItem>
#include <QPainter>
#include <QPen>
#include <QStyleOptionGraphicsItem>

// Small QGraphicsItem subclasses that tag scene items with the model
// identity they represent, so SchematicScene's mouse handlers can map a hit
// (from itemAt()) back to the Document id to act on. Each overrides type()
// (a distinct value per class, at or above QGraphicsItem::UserType) so
// qgraphicsitem_cast can distinguish them safely - see Qt's documented
// pattern for custom QGraphicsItem subclasses in a mixed-item scene.

// Groups one component's rotated/translated symbol artwork primitives (see
// SchematicScene::addComponentItems()). Selectable; dragged manually by
// SchematicScene rather than via ItemIsMovable, so the drag preview can be
// deferred to a single Document::moveComponent() call on release.
class ComponentItem : public QGraphicsItemGroup
{
public:
    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    explicit ComponentItem(ComponentId id) : m_id(id) {}
    ComponentId componentId() const { return m_id; }

    // QGraphicsItemGroup::paint() draws nothing of its own (only its
    // children paint), so the built-in dashed selection rectangle
    // QGraphicsItem would otherwise draw via the ItemState_Selected style
    // option never appears. Drawing it explicitly here is the standard Qt
    // workaround for selection feedback on group items.
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
              QWidget *widget) override
    {
        QGraphicsItemGroup::paint(painter, option, widget);
        if (isSelected()) {
            QPen pen(Qt::DashLine);
            pen.setColor(Qt::blue);
            pen.setCosmetic(true);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(boundingRect());
        }
    }

private:
    ComponentId m_id;
};

// One reference or value label, positioned independently of the rotated
// ComponentItem group (see the step 4 design note on labels staying
// upright) but tagged with the same ComponentId so a click on a label
// selects/drags/deletes its owning component - see
// SchematicScene::componentItemAt().
class ComponentLabelItem : public QGraphicsSimpleTextItem
{
public:
    enum { Type = UserType + 2 };
    int type() const override { return Type; }

    explicit ComponentLabelItem(const QString &text, ComponentId id)
        : QGraphicsSimpleTextItem(text), m_id(id) {}
    ComponentId componentId() const { return m_id; }

private:
    ComponentId m_id;
};

// One rendered WireRoute, drawn as a single painter path (see
// SchematicScene::addWireItems()).
class WireItem : public QGraphicsPathItem
{
public:
    enum { Type = UserType + 3 };
    int type() const override { return Type; }

    explicit WireItem(WireId id) : m_id(id) {}
    WireId wireId() const { return m_id; }

private:
    WireId m_id;
};

#endif // SCHEMATICITEMS_H
