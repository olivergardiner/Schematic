#ifndef SCHEMATICITEMS_H
#define SCHEMATICITEMS_H

#include "identifiers.h"
#include "textnote.h"

#include <QGraphicsItemGroup>
#include <QGraphicsPathItem>
#include <QGraphicsSimpleTextItem>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPen>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsTextItem>
#include <QTextDocument>
#include <QTextOption>
#include <QTextCursor>
#include <QTextBlockFormat>
#include <cmath>

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

    // Selection is shown by artwork/label colour, without a group rectangle.
    void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *) override {}

private:
    ComponentId m_id;
};

// Suppress Qt's default path-item selection decoration, including when its
// selected state is inherited from the component group.
class ComponentPrimitiveItem : public QGraphicsPathItem
{
public:
    explicit ComponentPrimitiveItem(const QPainterPath &path) : QGraphicsPathItem(path) {}
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        QStyleOptionGraphicsItem appearance(*option);
        appearance.state &= ~QStyle::State_Selected;
        QGraphicsPathItem::paint(painter, &appearance, widget);
    }
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

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *) override
    {
        painter->save();
        QPen appearance = pen();
        if (option->state & QStyle::State_Selected) appearance.setColor(Qt::red);
        painter->setPen(appearance);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path());
        painter->restore();
    }

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

class NoteItem : public QGraphicsTextItem
{
public:
    enum { Type = UserType + 5 };
    int type() const override { return Type; }
    explicit NoteItem(const TextNote &note) : m_id(note.id) {
        QFont font; font.setPixelSize(10); setFont(font);
        // Keep text clear of the box edge, including when right-aligned.
        document()->setDocumentMargin(4);
        auto option = document()->defaultTextOption();
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        document()->setDefaultTextOption(option);
        setPlainText(note.text); setTextWidth(note.width); setPos(note.position);
        QTextCursor cursor(document()); cursor.select(QTextCursor::Document);
        QTextBlockFormat format;
        format.setAlignment(note.justification == PropertyJustification::Middle ? Qt::AlignHCenter
                            : note.justification == PropertyJustification::Right ? Qt::AlignRight : Qt::AlignLeft);
        cursor.mergeBlockFormat(format);
        m_fill = note.fillColor == QLatin1String("none") ? QBrush(Qt::NoBrush) : QBrush(QColor(note.fillColor));
        m_outline = note.outlineColor == QLatin1String("none") ? QPen(Qt::NoPen) : QPen(QColor(note.outlineColor),1);
        setDefaultTextColor(Qt::black); setTextInteractionFlags(Qt::NoTextInteraction);
        setFlag(QGraphicsItem::ItemIsSelectable, true); setZValue(4);
    }
    NoteId noteId() const { return m_id; }
    QPainterPath shape() const override { QPainterPath path; path.addRect(boundingRect()); return path; }
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override {
        painter->save(); painter->setPen(m_outline); painter->setBrush(m_fill);
        if (m_fill.style() != Qt::NoBrush || m_outline.style() != Qt::NoPen)
            painter->drawRect(boundingRect().adjusted(0.5,0.5,-0.5,-0.5));
        painter->restore();
        QStyleOptionGraphicsItem clean(*option); clean.state &= ~QStyle::State_Selected;
        QGraphicsTextItem::paint(painter, &clean, widget);
        if (!isSelected()) return;
        painter->save();
        painter->setPen(QPen(QColor(35,105,155), 0, Qt::DashLine));
        painter->setBrush(Qt::NoBrush); painter->drawRect(boundingRect());
        const qreal scale = std::hypot(painter->worldTransform().m11(), painter->worldTransform().m12());
        const qreal size = scale > 0 ? qMin(qreal(6)/scale, boundingRect().height()) : 6;
        painter->setPen(QPen(QColor(35,105,155),0)); painter->setBrush(Qt::white);
        for (qreal x : {qreal(0), boundingRect().width() - size})
            painter->drawRect(QRectF(x, boundingRect().center().y()-size/2, size, size));
        painter->restore();
    }
private:
    NoteId m_id;
    QBrush m_fill;
    QPen m_outline;
};

#endif // SCHEMATICITEMS_H
