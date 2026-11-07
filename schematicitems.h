#ifndef SCHEMATICITEMS_H
#define SCHEMATICITEMS_H

#include "identifiers.h"

#include <QGraphicsItemGroup>
#include <QGraphicsPathItem>
#include <QGraphicsSimpleTextItem>
#include <QPainter>
#include <QPainterPathStroker>
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
// SchematicScene rather than via ItemIsMovable, so candidate previews stay
// separate from the single atomic document transform committed on release.
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

// A single upright drag target containing the visible property rows.
class PropertyBlockItem : public QGraphicsItemGroup
{
public:
    enum { Type = UserType + 4 };
    int type() const override { return Type; }
    explicit PropertyBlockItem(ComponentId id) : m_id(id) {
        setAcceptHoverEvents(true); setCursor(Qt::SizeAllCursor);
    }
    ComponentId componentId() const { return m_id; }
    QPainterPath shape() const override {
        QPainterPath result;
        for (auto *child : childItems()) result = result.united(child->mapToParent(child->shape()));
        return result;
    }
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *) override {
        if ((option->state & QStyle::State_MouseOver) || data(0).toBool()) {
            QPen pen(Qt::gray, 0, Qt::DashLine); painter->setPen(pen);
            painter->setBrush(Qt::NoBrush); painter->drawRect(boundingRect());
        }
    }
private:
    ComponentId m_id;
};

// One reference or value label, positioned independently of the rotated
// ComponentItem group (labels stay upright; see DECISIONS.md "Symbols and
// labels") but tagged with the same ComponentId so a click on a label
// selects/deletes its owning component; dragging moves its property block. See
// SchematicScene::componentItemAt().
class ComponentLabelItem : public QGraphicsSimpleTextItem
{
public:
    enum { Type = UserType + 2 };
    int type() const override { return Type; }

    explicit ComponentLabelItem(const QString &text, ComponentId id, const QString &propertyId = {})
        : QGraphicsSimpleTextItem(text), m_id(id), m_propertyId(propertyId) {}
    ComponentId componentId() const { return m_id; }
    QString propertyId() const { return m_propertyId; }

private:
    ComponentId m_id;
    QString m_propertyId;
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

    QPainterPath shape() const override
    {
        // QGraphicsPathItem's default shape also includes the implicitly
        // closed fill of an open path. A wire is only its stroked route.
        QPainterPathStroker stroker;
        stroker.setWidth(pen().widthF() > 0 ? pen().widthF() : 1.0);
        stroker.setCapStyle(pen().capStyle());
        stroker.setJoinStyle(pen().joinStyle());
        stroker.setMiterLimit(pen().miterLimit());
        return stroker.createStroke(path());
    }

private:
    WireId m_id;
};

#endif // SCHEMATICITEMS_H
