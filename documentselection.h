#ifndef DOCUMENTSELECTION_H
#define DOCUMENTSELECTION_H

#include "wireroute.h"
#include <QVector>

// Transient application state, shared by the scene and undo controller.
struct DocumentSelection
{
    QVector<ComponentId> components;
    QVector<WireId> wires;
    QVector<NoteId> notes;
    QVector<WireSegmentRef> segments;
    // Whole-object commands use this predicate. Segment selection is for reshaping.
    bool isEmpty() const { return components.isEmpty() && wires.isEmpty() && notes.isEmpty(); }
    bool operator==(const DocumentSelection &other) const
    { return components == other.components && wires == other.wires && notes == other.notes
          && segments == other.segments; }
};

#endif
