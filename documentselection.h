#ifndef DOCUMENTSELECTION_H
#define DOCUMENTSELECTION_H

#include "identifiers.h"
#include <QVector>

// Transient application state, shared by the scene and undo controller.
struct DocumentSelection
{
    QVector<ComponentId> components;
    QVector<WireId> wires;
    QVector<NoteId> notes;
    bool isEmpty() const { return components.isEmpty() && wires.isEmpty() && notes.isEmpty(); }
    bool operator==(const DocumentSelection &other) const
    { return components == other.components && wires == other.wires && notes == other.notes; }
};

#endif
