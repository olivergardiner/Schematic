#include "component.h"
#include "symboldefinition.h"

#include <QtGlobal>

QPointF rotateOffset(QPointF localOffset, Rotation rotation)
{
    const qreal x = localOffset.x();
    const qreal y = localOffset.y();

    switch (rotation) {
    case Rotation::Deg0:
        return { x, y };
    case Rotation::Deg90:
        // Clockwise quadrant rotation: (x, y) -> (-y, x)
        return { -y, x };
    case Rotation::Deg180:
        return { -x, -y };
    case Rotation::Deg270:
        return { y, -x };
    }
    return localOffset;
}

Component::Component(ComponentId id, SymbolKind kind, QPointF position)
    : m_id(id)
    , m_kind(kind)
    , m_position(position)
{
}

int Component::terminalCount() const
{
    return symbolTerminalCount(m_kind);
}

QPointF Component::terminalPosition(TerminalId terminal) const
{
    const SymbolDefinition &def = symbolDefinition(m_kind);
    Q_ASSERT(terminal >= 0 && terminal < def.terminals.size());
    if (terminal < 0 || terminal >= def.terminals.size())
        return m_position;

    // Apply mirroring if necessary - the local offset is mirrored (x negated) first, then rotated, then translated.
    const QPointF local = def.terminals[terminal].offset;
    const QPointF mirroredOffset = m_mirrored ? QPointF(-local.x(), local.y()) : local;
    return m_position + rotateOffset(mirroredOffset, m_rotation);
}
