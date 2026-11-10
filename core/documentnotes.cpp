#include "document.h"
#include <algorithm>
#include <cmath>
#include <QRegularExpression>

bool validNoteColour(const QString &colour)
{
    static const QRegularExpression hex(QStringLiteral("^#(?:[0-9a-fA-F]{6}|[0-9a-fA-F]{8})$"));
    return colour == QLatin1String("none") || hex.match(colour).hasMatch();
}

namespace {
bool validNote(QPointF position, const QString &text, qreal width)
{
    return std::isfinite(position.x()) && std::isfinite(position.y())
        && std::isfinite(width) && width >= kMinimumNoteWidth && width <= kMaximumNoteWidth
        && std::isfinite(position.x() + width) && !text.contains(QChar::Null);
}
}

const TextNote *Document::note(NoteId id) const
{
    for (const auto &item : m_notes) if (item.id == id) return &item;
    return nullptr;
}

NoteId Document::addNote(QPointF position, const QString &text, qreal width)
{
    if (!m_nextNoteId || !validNote(position, text, width)) return kInvalidNoteId;
    const NoteId id = m_nextNoteId++;
    m_notes.append({id, position, text, width});
    return id;
}

bool Document::setNote(NoteId id, QPointF position, const QString &text, qreal width)
{
    if (!validNote(position, text, width)) return false;
    for (auto &item : m_notes) {
        if (item.id != id) continue;
        if (item.position == position && item.text == text && item.width == width) return false;
        item.position = position; item.text = text; item.width = width;
        return true;
    }
    return false;
}

bool Document::setNoteAppearance(NoteId id, PropertyJustification justification,
                                  const QString &fillColor, const QString &outlineColor)
{
    if (propertyJustificationName(justification).isEmpty()
        || !validNoteColour(fillColor) || !validNoteColour(outlineColor)) return false;
    for (auto &item : m_notes) {
        if (item.id != id) continue;
        const auto fill = fillColor.toLower(), outline = outlineColor.toLower();
        if (item.justification == justification && item.fillColor == fill && item.outlineColor == outline) return false;
        item.justification = justification; item.fillColor = fill; item.outlineColor = outline;
        return true;
    }
    return false;
}

bool Document::removeNote(NoteId id)
{
    const auto it = std::find_if(m_notes.begin(), m_notes.end(), [id](const TextNote &item) { return item.id == id; });
    if (it == m_notes.end()) return false;
    m_notes.erase(it);
    return true;
}
