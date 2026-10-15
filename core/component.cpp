#include "component.h"
#include "symbollibrary.h"

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

Component::Component(ComponentId id, const SymbolId &symbolId, QPointF position)
    : m_id(id)
    , m_symbolId(symbolId)
    , m_position(position)
{
}

int Component::terminalCount() const
{
    const LibrarySymbol *symbol = builtinSymbolLibrary().find(m_symbolId);
    return symbol ? static_cast<int>(symbol->terminals.size()) : 0;
}

QPointF Component::terminalPosition(TerminalId terminal) const
{
    const LibrarySymbol *symbol = builtinSymbolLibrary().find(m_symbolId);
    Q_ASSERT(symbol && terminal >= 0 && terminal < symbol->terminals.size());
    if (!symbol || terminal < 0 || terminal >= symbol->terminals.size())
        return m_position;

    // Apply mirroring if necessary - the local offset is mirrored (x negated) first, then rotated, then translated.
    const QPointF local = symbol->terminals[terminal].offset;
    const QPointF mirroredOffset = m_mirrored ? QPointF(-local.x(), local.y()) : local;
    return m_position + rotateOffset(mirroredOffset, m_rotation);
}
