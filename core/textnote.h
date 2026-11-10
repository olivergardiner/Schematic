#ifndef TEXTNOTE_H
#define TEXTNOTE_H
#include "identifiers.h"
#include "propertyjustification.h"
#include <QPointF>
#include <QString>

constexpr qreal kMinimumNoteWidth = 20.0;
constexpr qreal kMaximumNoteWidth = 1000000.0;
struct TextNote
{
    NoteId id = kInvalidNoteId;
    QPointF position; // Top-left in scene units; text always stays upright.
    QString text;
    qreal width = 160.0; // Height is derived by the presentation layer.
    PropertyJustification justification = PropertyJustification::Left;
    QString fillColor = QStringLiteral("none");
    QString outlineColor = QStringLiteral("none");
};
bool validNoteColour(const QString &colour);
#endif
