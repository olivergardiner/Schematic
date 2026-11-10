#include "tst_document.h"

#include "document.h"
#include "documentloadresult.h"

#include <QTest>
#include <QJsonArray>

#include <limits>
#include <algorithm>

void TstDocument::notesV9RoundTripAndLegacy()
{
    Document doc;
    const auto id = doc.addNote({13.5,-27}, "First paragraph\n\nSecond <plain> & text", 120);
    QVERIFY(id);
    QVERIFY(doc.setNoteAppearance(id,PropertyJustification::Right,"#80ABCDEF","#123456"));
    QVERIFY(doc.setNote(id,{13.5,-27},"First paragraph\n\nSecond <plain> & text",121));
    QCOMPARE(doc.note(id)->fillColor,QString("#80abcdef"));
    QCOMPARE(doc.note(id)->justification,PropertyJustification::Right);
    QVERIFY(doc.components().isEmpty()); QVERIFY(doc.computeNets().isEmpty());
    const auto json = doc.toJson();
    QCOMPARE(json["formatVersion"].toInt(), 10);
    QCOMPARE(json["notes"].toArray().size(), 1);
    auto loaded = Document::fromJson(json);
    QVERIFY2(loaded.document, qPrintable(loaded.errors.join('\n')));
    QCOMPARE(loaded.document->toJsonBytes(), doc.toJsonBytes());
    QCOMPARE(loaded.document->note(id)->text, QString("First paragraph\n\nSecond <plain> & text"));
    QCOMPARE(loaded.document->note(id)->width, 121.0);
    auto v9=json; v9["formatVersion"]=9;
    auto v9notes=v9["notes"].toArray(); auto v9note=v9notes[0].toObject();
    for(const auto &key : {"justification","fillColor","outlineColor"}) v9note.remove(key);
    v9notes[0]=v9note; v9["notes"]=v9notes;
    auto oldNote=Document::fromJson(v9); QVERIFY(oldNote.document);
    QCOMPARE(oldNote.document->note(id)->fillColor,QString("none"));
    QCOMPARE(oldNote.document->note(id)->justification,PropertyJustification::Left);
    const auto saved=doc.toJsonBytes();
    QVERIFY(!doc.setNoteAppearance(id,PropertyJustification::Left,"bad","none"));
    QCOMPARE(doc.toJsonBytes(),saved);
    QVERIFY(loaded.document->addNote({0,0}) != id);
    auto legacy = json; legacy["formatVersion"] = 8; legacy.remove("notes");
    auto old = Document::fromJson(legacy);
    QVERIFY(old.document); QVERIFY(old.document->notes().isEmpty());
    auto future = json; future["formatVersion"] = 11;
    QVERIFY(!Document::fromJson(future).document);
    for (const auto &key : {"id", "x", "y", "text", "width", "justification", "fillColor", "outlineColor"}) {
        auto invalid = json;
        auto notes = invalid["notes"].toArray(); auto note = notes[0].toObject();
        note.remove(key); notes[0] = note; invalid["notes"] = notes;
        QVERIFY2(!Document::fromJson(invalid).document, key);
    }
    auto duplicate = json;
    auto notes = duplicate["notes"].toArray(); notes.append(notes[0]); duplicate["notes"] = notes;
    QVERIFY(!Document::fromJson(duplicate).document);
    auto missing = json; missing.remove("notes");
    QVERIFY(!Document::fromJson(missing).document);
}

void TstDocument::notesClipboardAndTransforms()
{
    Document doc;
    const auto a = doc.addComponent(SymbolIds::Resistor, {});
    const auto b = doc.addComponent(SymbolIds::Resistor, {100,0});
    const auto wire = doc.addWire(makeTerminalEndpoint(a,1), {}, makeTerminalEndpoint(b,0));
    QVERIFY(wire);
    const auto note = doc.addNote({30,40}, "Wrapped words\nand a paragraph", 110);
    QVERIFY(note);
    QVERIFY(doc.setNoteAppearance(note,PropertyJustification::Middle,"#abcdef","#123456"));
    const auto bytes = doc.copyFragment({a}, {*wire}, nullptr, {note});
    Document pasted = doc;
    QVector<ComponentId> components; QVector<WireId> wires; QVector<NoteId> notes;
    QVERIFY(pasted.pasteFragmentAt(bytes, {200,100}, &components, &wires, nullptr, &notes));
    QCOMPARE(notes.size(), 1); QVERIFY(notes[0] != note);
    QCOMPARE(pasted.note(notes[0])->position, QPointF(190,120));
    QCOMPARE(pasted.note(notes[0])->width, 110.0);
    QCOMPARE(pasted.note(notes[0])->text, doc.note(note)->text);
    QCOMPARE(pasted.note(notes[0])->fillColor,doc.note(note)->fillColor);
    QCOMPARE(pasted.note(notes[0])->outlineColor,doc.note(note)->outlineColor);
    QCOMPARE(pasted.note(notes[0])->justification,doc.note(note)->justification);
    QVERIFY(pasted.removeSelection(components,wires,notes));
    QCOMPARE(pasted.toJsonBytes(), doc.toJsonBytes());
    const auto only = doc.copyFragment({}, {}, nullptr, {note});
    QVERIFY(pasted.pasteFragmentAt(only, {300,200}, nullptr, nullptr, nullptr, &notes));
    QCOMPARE(pasted.note(notes[0])->position, QPointF(300,200));
    QVERIFY(doc.transformSelection({a}, {}, {SelectionTransform::Kind::Translate,{20,10}}, {note}));
    QCOMPARE(doc.note(note)->position, QPointF(50,50));
    QCOMPARE(doc.note(note)->width, 110.0);
    QVERIFY(doc.transformSelection({}, {}, {SelectionTransform::Kind::Rotate, {}, {}, 1}, {note}));
    QCOMPARE(doc.note(note)->position, QPointF(-50,50));
    QCOMPARE(doc.note(note)->width, 110.0); // text remains upright; only anchor transforms
}

void TstDocument::notesRejectInvalidEditsAtomically()
{
    Document doc;
    QVERIFY(!doc.addNote({qInf(),0}));
    QVERIFY(!doc.addNote({}, "text", 0));
    QCOMPARE(doc.addNote({}, "Note", 160), NoteId(1));
    const auto before = doc.toJsonBytes();
    QVERIFY(!doc.setNote(1, {}, "text", kMaximumNoteWidth+1));
    QVERIFY(!doc.setNote(1, {}, QString(QChar::Null), 100));
    QVERIFY(!doc.setNote(2, {}, "text", 100));
    QVERIFY(!doc.transformSelection({}, {}, {SelectionTransform::Kind::Translate,{10,20}}, {999}));
    QVERIFY(!doc.removeSelection({}, {}, {1,999}));
    QCOMPARE(doc.toJsonBytes(), before);
    auto json = doc.toJson(); auto notes = json["notes"].toArray(); auto item = notes[0].toObject();
    item["id"] = double(std::numeric_limits<NoteId>::max()); notes[0] = item; json["notes"] = notes;
    auto loaded = Document::fromJson(json); QVERIFY(loaded.document);
    QVERIFY(!loaded.document->addNote({}));
}

namespace {

int netContaining(const QVector<Net> &nets, TerminalRef wanted)
{
    for (int i = 0; i < nets.size(); ++i) {
        if (nets[i].terminals.contains(wanted))
            return i;
    }
    return -1;
}

} // namespace

void TstDocument::placementConnectsWireInteriorsAndPreservesNets()
{
    for (bool vertical : {false,true}) {
        for (bool reverse : {false,true}) {
            Document doc;
            const auto a = doc.addComponent(SymbolIds::Resistor, {}, vertical ? Rotation::Deg90 : Rotation::Deg0);
            const auto b = doc.addComponent(SymbolIds::Resistor, vertical ? QPointF(0,100) : QPointF(100,0),
                                            vertical ? Rotation::Deg90 : Rotation::Deg0);
            const auto start = makeTerminalEndpoint(reverse ? b : a, reverse ? 0 : 1);
            const auto end = makeTerminalEndpoint(reverse ? a : b, reverse ? 1 : 0);
            const auto wire = doc.addWire(start, {}, end);
            QVERIFY(wire);
            QStringList warnings;
            const QPointF position = vertical ? QPointF(20,50) : QPointF(50,-20);
            const QPointF contact = vertical ? QPointF(0,50) : QPointF(50,0);
            const auto rail = doc.placeComponent(SymbolIds::RailVCC, position,
                                                  vertical ? Rotation::Deg90 : Rotation::Deg0, &warnings);
            QVERIFY(rail != kInvalidComponentId);
            QVERIFY(warnings.isEmpty());
            QVERIFY(!doc.wire(*wire));
            QCOMPARE(doc.wires().size(), 2);
            QVERIFY(doc.nodes().isEmpty());
            const auto terminal = makeTerminalEndpoint(rail,0);
            QVERIFY(doc.wires()[0].start == start);
            QVERIFY(doc.wires()[0].end == terminal);
            QVERIFY(doc.wires()[1].start == terminal);
            QVERIFY(doc.wires()[1].end == end);
            QCOMPARE(doc.wires()[0].vertices.last(), contact);
            QCOMPARE(doc.wires()[1].vertices.first(), contact);
            QCOMPARE(doc.junctionPoints(), QVector<QPointF>{contact});
            const auto nets = doc.computeNets();
            QCOMPARE(netContaining(nets, {a,1}), netContaining(nets, {rail,0}));
            QCOMPARE(netContaining(nets, {b,0}), netContaining(nets, {rail,0}));
            const auto loaded = Document::fromJsonBytes(doc.toJsonBytes());
            QVERIFY(loaded.document);
            QVERIFY(loaded.errors.isEmpty());
            QVERIFY(loaded.warnings.isEmpty());
            QCOMPARE(loaded.document->toJsonBytes(), doc.toJsonBytes());
            QCOMPARE(loaded.document->junctionPoints(), doc.junctionPoints());
            QVERIFY(doc.moveComponent(rail, position + QPointF(10,10)));
            QCOMPARE(doc.junctionPoints(), QVector<QPointF>{contact + QPointF(10,10)});
            for (const auto &route : doc.wires())
                QCOMPARE(validateRouteGeometry(route.vertices), RouteGeometryError::None);
            QCOMPARE(netContaining(doc.computeNets(), {a,1}), netContaining(doc.computeNets(), {rail,0}));
            QVERIFY(doc.removeComponent(rail));
            QCOMPARE(doc.wires().size(), 2);
            QCOMPARE(doc.nodes().size(), 1);
            QVERIFY(doc.wires()[0].end == doc.wires()[1].start);
            QVERIFY(doc.junctionPoints().isEmpty());
        }
    }
}

void TstDocument::placementConnectsCornersAndMultipleTerminals()
{
    Document corner;
    const auto a = corner.addComponent(SymbolIds::Resistor, {});
    const auto b = corner.addComponent(SymbolIds::Resistor, {100,100});
    QVERIFY(corner.addWire(makeTerminalEndpoint(a,1), {{50,0},{50,100}}, makeTerminalEndpoint(b,0)));
    const auto rail = corner.placeComponent(SymbolIds::RailVCC, {50,-20});
    QVERIFY(rail != kInvalidComponentId);
    QCOMPARE(corner.wires().size(), 2);
    QCOMPARE(corner.wires()[0].vertices, QVector<QPointF>({{20,0},{50,0}}));
    QCOMPARE(corner.wires()[1].vertices, QVector<QPointF>({{50,0},{50,100},{80,100}}));
    QCOMPARE(corner.junctionPoints(), QVector<QPointF>({{50,0}}));

    Document multiple;
    const auto left = multiple.addComponent(SymbolIds::Resistor, {});
    const auto right = multiple.addComponent(SymbolIds::Resistor, {200,0});
    QVERIFY(multiple.addWire(makeTerminalEndpoint(left,1), {}, makeTerminalEndpoint(right,0)));
    const auto placed = multiple.placeComponent(SymbolIds::Resistor, {100,0});
    QVERIFY(placed != kInvalidComponentId);
    QCOMPARE(multiple.wires().size(), 3);
    QCOMPARE(multiple.wires()[0].vertices, QVector<QPointF>({{20,0},{80,0}}));
    QCOMPARE(multiple.wires()[1].vertices, QVector<QPointF>({{80,0},{120,0}}));
    QCOMPARE(multiple.wires()[2].vertices, QVector<QPointF>({{120,0},{180,0}}));
    const auto nets = multiple.computeNets();
    QCOMPARE(netContaining(nets, {left,1}), netContaining(nets, {placed,0}));
    QCOMPARE(netContaining(nets, {right,0}), netContaining(nets, {placed,1}));
    QCOMPARE(multiple.junctionPoints().size(), 2);
}

void TstDocument::placementReusesJunctionIdentity()
{
    Document doc;
    const auto a = doc.addComponent(SymbolIds::Resistor, {});
    const auto b = doc.addComponent(SymbolIds::Resistor, {100,0});
    const auto g = doc.addComponent(SymbolIds::Ground, {50,60});
    const auto wire = doc.addWire(makeTerminalEndpoint(a,1), {}, makeTerminalEndpoint(b,0));
    QVERIFY(wire);
    const auto node = doc.branchWireAt(*wire, {50,0});
    QVERIFY(node);
    QVERIFY(doc.addWire(makeNodeEndpoint(*node), {}, makeTerminalEndpoint(g,0)));
    const auto original = doc.wires();
    const auto rail = doc.placeComponent(SymbolIds::RailVCC, {50,-20});
    QVERIFY(rail != kInvalidComponentId);
    QVERIFY(doc.nodes().isEmpty());
    QCOMPARE(doc.wires().size(), original.size());
    for (int i = 0; i < original.size(); ++i) {
        QCOMPARE(doc.wires()[i].id, original[i].id);
        QCOMPARE(doc.wires()[i].vertices, original[i].vertices);
        const auto expectedStart = original[i].start == makeNodeEndpoint(*node) ? makeTerminalEndpoint(rail,0) : original[i].start;
        const auto expectedEnd = original[i].end == makeNodeEndpoint(*node) ? makeTerminalEndpoint(rail,0) : original[i].end;
        QVERIFY(doc.wires()[i].start == expectedStart);
        QVERIFY(doc.wires()[i].end == expectedEnd);
    }
    const auto nets = doc.computeNets();
    for (TerminalRef ref : {TerminalRef{a,1}, TerminalRef{b,0}, TerminalRef{g,0}})
        QCOMPARE(netContaining(nets, ref), netContaining(nets, {rail,0}));
    QCOMPARE(doc.junctionPoints(), QVector<QPointF>({{50,0}}));
    const auto loaded = Document::fromJsonBytes(doc.toJsonBytes());
    QVERIFY(loaded.document);
    QCOMPARE(loaded.document->toJsonBytes(), doc.toJsonBytes());
}

void TstDocument::placementLeavesAmbiguousAndOccupiedSitesUnconnected()
{
    Document doc;
    const auto a = doc.addComponent(SymbolIds::Resistor, {});
    const auto b = doc.addComponent(SymbolIds::Resistor, {100,0});
    const auto c = doc.addComponent(SymbolIds::Resistor, {50,-50}, Rotation::Deg90);
    const auto d = doc.addComponent(SymbolIds::Resistor, {50,50}, Rotation::Deg90);
    QVERIFY(doc.addWire(makeTerminalEndpoint(a,1), {}, makeTerminalEndpoint(b,0)));
    QVERIFY(doc.addWire(makeTerminalEndpoint(c,1), {}, makeTerminalEndpoint(d,0)));
    const auto original = doc.wires();
    QStringList warnings;
    const auto rail = doc.placeComponent(SymbolIds::RailVCC, {50,-20}, Rotation::Deg0, &warnings);
    QVERIFY(rail != kInvalidComponentId);
    QCOMPARE(warnings.size(), 1);
    QCOMPARE(doc.wires().size(), 2);
    for (int i = 0; i < original.size(); ++i) {
        QCOMPARE(doc.wires()[i].id, original[i].id);
        QCOMPARE(doc.wires()[i].vertices, original[i].vertices);
    }
    const auto nets = doc.computeNets();
    QVERIFY(netContaining(nets, {a,1}) != netContaining(nets, {c,1}));
    QVERIFY(netContaining(nets, {rail,0}) != netContaining(nets, {a,1}));
    QVERIFY(doc.junctionPoints().isEmpty());
    warnings.clear();
    const auto occupied = doc.placeComponent(SymbolIds::RailVCC, {20,-20}, Rotation::Deg0, &warnings);
    QVERIFY(occupied != kInvalidComponentId);
    QCOMPARE(warnings.size(), 1);
    QCOMPARE(doc.wires().size(), 2);
    QVERIFY(netContaining(doc.computeNets(), {occupied,0}) != netContaining(doc.computeNets(), {a,1}));
    warnings.clear();
    const auto near = doc.placeComponent(SymbolIds::RailVCC, {60,-19}, Rotation::Deg0, &warnings);
    QVERIFY(near != kInvalidComponentId); // Merely nearby is not a contact.
    QVERIFY(warnings.isEmpty());
    QCOMPARE(doc.wires().size(), 2);
    QVERIFY(doc.junctionPoints().isEmpty());
}

void TstDocument::placementFailureIsAtomic()
{
    Document doc;
    const auto a = doc.addComponent(SymbolIds::Resistor, {});
    const auto b = doc.addComponent(SymbolIds::Resistor, {200,0});
    QVERIFY(doc.addWire(makeTerminalEndpoint(a,1), {}, makeTerminalEndpoint(b,0)));
    auto json = doc.toJson();
    auto wires = json["wires"].toArray();
    auto wire = wires[0].toObject();
    // Enough identities to connect the first terminal, but not the second.
    wire["id"] = double(std::numeric_limits<WireId>::max() - 3);
    wires[0] = wire;
    json["wires"] = wires;
    auto loaded = Document::fromJson(json);
    QVERIFY(loaded.document);
    doc = *loaded.document;
    const auto before = doc.toJsonBytes();
    QCOMPARE(doc.placeComponent(SymbolIds::Resistor, {100,0}), kInvalidComponentId);
    QCOMPARE(doc.toJsonBytes(), before);
    QCOMPARE(doc.placeComponent(SymbolIds::RailVCC, {0,std::numeric_limits<qreal>::infinity()}), kInvalidComponentId);
    QCOMPARE(doc.placeComponent(SymbolId(QStringLiteral("Unknown")), {}), kInvalidComponentId);
    QCOMPARE(doc.toJsonBytes(), before);
    // A failed placement has not consumed the component identity.
    QCOMPARE(doc.placeComponent(SymbolIds::RailVCC, {400,0}), ComponentId(3));
}

void TstDocument::segmentMovementPreservesEndpointsAndTopology()
{
    // Direct horizontal/vertical routes acquire two endpoint connectors.
    // Test both endpoint orders independently of the implementation.
    for (bool vertical : {false, true}) {
        for (bool reverse : {false, true}) {
            Document doc;
            const auto a = doc.addComponent(SymbolIds::Resistor, {},
                                            vertical ? Rotation::Deg90 : Rotation::Deg0);
            const auto b = doc.addComponent(SymbolIds::Resistor, vertical ? QPointF(0,100) : QPointF(100,0),
                                            vertical ? Rotation::Deg90 : Rotation::Deg0);
            const auto start = makeTerminalEndpoint(reverse ? b : a, reverse ? 0 : 1);
            const auto end = makeTerminalEndpoint(reverse ? a : b, reverse ? 1 : 0);
            const auto id = doc.addWire(start, {}, end);
            QVERIFY(id);
            const QPointF first = vertical ? QPointF(0,20) : QPointF(20,0);
            const QPointF last = vertical ? QPointF(0,80) : QPointF(80,0);
            QVector<QPointF> expected = vertical
                ? QVector<QPointF>{{0,20},{30,20},{30,80},{0,80}}
                : QVector<QPointF>{{20,0},{20,30},{80,30},{80,0}};
            if (reverse) std::reverse(expected.begin(), expected.end());
            QVERIFY(doc.moveWireSegment(*id, 0, 30));
            QCOMPARE(doc.wire(*id)->vertices, expected);
            QVERIFY(doc.wire(*id)->start == start);
            QVERIFY(doc.wire(*id)->end == end);
            QCOMPARE(netContaining(doc.computeNets(), {a,1}), netContaining(doc.computeNets(), {b,0}));
            // Move back onto the original axis: vanishing bends collapse.
            QVERIFY(doc.moveWireSegment(*id, 1, 0));
            QCOMPARE(doc.wire(*id)->vertices, reverse ? QVector<QPointF>({last,first})
                                                     : QVector<QPointF>({first,last}));
        }
    }

    Document doc;
    const auto a = doc.addComponent(SymbolIds::Resistor, {});
    const auto b = doc.addComponent(SymbolIds::Resistor, {100,100});
    const auto c = doc.addComponent(SymbolIds::Resistor, {140,50});
    const auto original = doc.addWire(makeTerminalEndpoint(a,1), {{50,0},{50,100}}, makeTerminalEndpoint(b,0));
    QVERIFY(original);
    const auto node = doc.branchWireAt(*original, {50,50});
    QVERIFY(node);
    QVERIFY(doc.addWire(makeNodeEndpoint(*node), {}, makeTerminalEndpoint(c,0)));
    const auto leg = doc.wires().first().id;
    const auto nodes = doc.nodes();
    const auto dots = doc.junctionPoints();
    const auto other = doc.wires()[1].vertices;
    QVERIFY(doc.moveWireSegment(leg, 1, 60));
    QCOMPARE(doc.wire(leg)->vertices, QVector<QPointF>({{20,0},{60,0},{60,50},{50,50}}));
    QCOMPARE(doc.nodes(), nodes);
    QCOMPARE(doc.junctionPoints(), dots);
    QCOMPARE(doc.wires()[1].vertices, other);
    const auto nets = doc.computeNets();
    QCOMPARE(netContaining(nets, {a,1}), netContaining(nets, {b,0}));
    QCOMPARE(netContaining(nets, {a,1}), netContaining(nets, {c,0}));
    const auto loaded = Document::fromJsonBytes(doc.toJsonBytes());
    QVERIFY(loaded.document);
    QCOMPARE(loaded.document->wire(leg)->vertices, doc.wire(leg)->vertices);
    QCOMPARE(loaded.document->nodes(), nodes);
}

void TstDocument::segmentMovementRejectsInvalidCandidates()
{
    Document doc;
    const auto a = doc.addComponent(SymbolIds::Resistor, {});
    const auto b = doc.addComponent(SymbolIds::Resistor, {100,0});
    const auto id = doc.addWire(makeTerminalEndpoint(a,1), {{40,0},{40,40},{80,40}}, makeTerminalEndpoint(b,0));
    QVERIFY(id);
    const auto before = doc.toJsonBytes();
    QVERIFY(!doc.moveWireSegment(*id, 1, 80)); // collapsed neighbour leaves a reversal
    QVERIFY(!doc.moveWireSegment(*id, -1, 30));
    QVERIFY(!doc.moveWireSegment(*id, 4, 30));
    QVERIFY(!doc.moveWireSegment(kInvalidWireId, 0, 30));
    QVERIFY(!doc.moveWireSegment(*id, 0, std::numeric_limits<qreal>::infinity()));
    QVERIFY(!doc.moveWireSegment(*id, 0, std::numeric_limits<qreal>::quiet_NaN()));
    QVERIFY(!doc.moveWireSegment(*id, 0, 0));
    QCOMPARE(doc.toJsonBytes(), before);
    QVERIFY(doc.moveWireSegment(*id, 1, 60));
    QCOMPARE(doc.wire(*id)->vertices, QVector<QPointF>({{20,0},{60,0},{60,40},{80,40},{80,0}}));

    Document redundant;
    const auto left = redundant.addComponent(SymbolIds::Resistor, {});
    const auto right = redundant.addComponent(SymbolIds::Resistor, {100,0});
    const auto straight = redundant.addWire(makeTerminalEndpoint(left,1), {{40,0},{60,0}}, makeTerminalEndpoint(right,0));
    QVERIFY(straight);
    QVERIFY(redundant.moveWireSegment(*straight, 1, -20));
    QCOMPARE(redundant.wire(*straight)->vertices, QVector<QPointF>({{20,0},{20,-20},{80,-20},{80,0}}));
}

void TstDocument::connectivityUsesEndpointIdentity()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolIds::Resistor, QPointF(100, 0));
    const ComponentId c = document.addComponent(SymbolIds::Resistor, QPointF(50, -50), Rotation::Deg90);
    const ComponentId d = document.addComponent(SymbolIds::Resistor, QPointF(50, 50), Rotation::Deg90);
    QVERIFY(document.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0)));
    QVERIFY(document.addWire(makeTerminalEndpoint(c, 1), {}, makeTerminalEndpoint(d, 0)));

    const QVector<Net> nets = document.computeNets();
    QCOMPARE(nets.size(), 6); // 8 terminals, with two independent pairs joined.
    const int horizontal = netContaining(nets, TerminalRef{a, 1});
    const int vertical = netContaining(nets, TerminalRef{c, 1});
    QVERIFY(horizontal >= 0);
    QVERIFY(vertical >= 0);
    QCOMPARE(horizontal, netContaining(nets, TerminalRef{b, 0}));
    QCOMPARE(vertical, netContaining(nets, TerminalRef{d, 0}));
    QVERIFY(horizontal != vertical); // The routes cross at (50, 0) only geometrically.
}

void TstDocument::movementPreservesOrthogonalRoutesAndNets()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolIds::Resistor, QPointF(100, 0));
    const auto id = document.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0));
    QVERIFY(id);
    const QVector<Net> before = document.computeNets();

    QVERIFY(document.moveComponent(a, QPointF(10, 0)));
    QCOMPARE(document.wire(*id)->vertices, QVector<QPointF>({QPointF(30, 0), QPointF(80, 0)}));
    QVERIFY(document.moveComponent(a, QPointF(10, 10)));
    const WireRoute *route = document.wire(*id);
    QVERIFY(route);
    QCOMPARE(route->vertices, QVector<QPointF>({QPointF(30, 10), QPointF(30, 0), QPointF(80, 0)}));
    QCOMPARE(route->vertices.last(), QPointF(80, 0));
    QCOMPARE(validateRouteGeometry(route->vertices), RouteGeometryError::None);
    const QVector<Net> after = document.computeNets();
    QCOMPARE(netContaining(after, TerminalRef{a, 1}), netContaining(after, TerminalRef{b, 0}));
    QCOMPARE(before.size(), after.size());
}

void TstDocument::coincidentDirectEndpointsRejectMove()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolIds::Resistor, QPointF(80, 0));
    const auto id = document.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0));
    QVERIFY(id);
    const QVector<QPointF> before = document.wire(*id)->vertices;

    QVERIFY(!document.moveComponent(b, QPointF(40, 0)));
    QCOMPARE(document.component(b)->position(), QPointF(80, 0));
    QCOMPARE(document.wire(*id)->vertices, before);
}

void TstDocument::movementCollapsesRedundantCorner()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolIds::Resistor, QPointF(40, 40));
    const auto id = document.addWire(makeTerminalEndpoint(a, 1), {QPointF(20, 20)},
                                     makeTerminalEndpoint(b, 0));
    QVERIFY(id);

    QVERIFY(document.moveComponent(a, QPointF(0, 20)));
    const WireRoute *route = document.wire(*id);
    QVERIFY(route);
    QCOMPARE(route->vertices, QVector<QPointF>({QPointF(20, 20), QPointF(20, 40)}));
    QCOMPARE(validateRouteGeometry(route->vertices), RouteGeometryError::None);
}

void TstDocument::componentDeletionPreservesBranches()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolIds::Resistor, QPointF(100, 0));
    const ComponentId c = document.addComponent(SymbolIds::Resistor, QPointF(60, 40));
    const auto wire = document.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0));
    QVERIFY(wire);
    const auto node = document.branchWireAt(*wire, QPointF(40, 0));
    QVERIFY(node);
    QVERIFY(document.addWire(makeNodeEndpoint(*node), {}, makeTerminalEndpoint(c, 0)));
    QCOMPARE(document.junctionPoints(), QVector<QPointF>({QPointF(40, 0)}));

    QVERIFY(document.removeComponent(c));
    QCOMPARE(document.junctionPoints(), QVector<QPointF>({QPointF(40, 0)}));
    QVERIFY(document.nodes().contains(*node));
    QVERIFY(document.removeComponent(a));
    QCOMPARE(document.wires().size(), 3);
    QVERIFY(document.nodes().contains(*node));
    const auto remaining = document.wires();
    for (const auto &route : remaining) QVERIFY(document.removeWire(route.id));
    QVERIFY(!document.nodes().contains(*node));
    QVERIFY(!document.addWire(makeNodeEndpoint(*node), {}, makeTerminalEndpoint(b, 1)));
    QVERIFY(document.nodes().isEmpty());
}

void TstDocument::deletionPreservesSharedAndSeparateTerminalIdentities()
{
    Document doc;
    const auto a = doc.addComponent(SymbolIds::Resistor, {});
    const auto b = doc.addComponent(SymbolIds::Resistor, {100,0});
    const auto c = doc.addComponent(SymbolIds::Resistor, {100,50});
    const auto d = doc.addComponent(SymbolIds::Resistor, {-100,0});
    const auto e = doc.addComponent(SymbolIds::Resistor, {100,100});
    const auto ab = doc.addWire(makeTerminalEndpoint(a,1), {}, makeTerminalEndpoint(b,0));
    const auto ac = doc.addWire(makeTerminalEndpoint(a,1), {{20,50}}, makeTerminalEndpoint(c,0));
    const auto da = doc.addWire(makeTerminalEndpoint(d,1), {}, makeTerminalEndpoint(a,0));
    const auto loose = doc.addWireToPoint(makeTerminalEndpoint(e,0), {{20,100}}, {20,0});
    QVERIFY(ab); QVERIFY(ac); QVERIFY(da); QVERIFY(loose);
    const auto unrelatedNode = doc.wire(*loose)->end;
    const auto original = doc.wires();
    QVERIFY(doc.removeComponent(a));
    QCOMPARE(doc.wires().size(), original.size());
    for (const auto &route : original) QCOMPARE(doc.wire(route.id)->vertices, route.vertices);
    QVERIFY(isNode(doc.wire(*ab)->start));
    QVERIFY(doc.wire(*ab)->start == doc.wire(*ac)->start);
    QVERIFY(doc.wire(*ab)->start != doc.wire(*da)->end);
    QVERIFY(doc.wire(*ab)->start != unrelatedNode); // Even at the same coordinates.
    const auto nets = doc.computeNets();
    QCOMPARE(netContaining(nets, {b,0}), netContaining(nets, {c,0}));
    QVERIFY(netContaining(nets, {b,0}) != netContaining(nets, {d,1}));
    QVERIFY(netContaining(nets, {b,0}) != netContaining(nets, {e,0}));
    const auto loaded = Document::fromJsonBytes(doc.toJsonBytes());
    QVERIFY(loaded.document);
    QCOMPARE(loaded.document->toJsonBytes(), doc.toJsonBytes());
}

void TstDocument::deletionFailureIsAtomic()
{
    Document doc;
    const auto a = doc.addComponent(SymbolIds::Resistor, {});
    const auto b = doc.addComponent(SymbolIds::Resistor, {100,0});
    const auto wire = doc.addWireToPoint(makeTerminalEndpoint(a,1), {}, {60,0});
    QVERIFY(wire);
    auto json = doc.toJson();
    auto nodes = json["nodes"].toArray();
    auto node = nodes[0].toObject();
    node["id"] = double(std::numeric_limits<NodeId>::max());
    nodes[0] = node; json["nodes"] = nodes;
    auto wires = json["wires"].toArray();
    auto route = wires[0].toObject();
    auto end = route["end"].toObject();
    end["node"] = double(std::numeric_limits<NodeId>::max());
    route["end"] = end; wires[0] = route; json["wires"] = wires;
    auto loaded = Document::fromJson(json);
    QVERIFY(loaded.document);
    doc = *loaded.document;
    const auto before = doc.toJsonBytes();
    QVERIFY(!doc.removeComponent(a));
    QVERIFY(!doc.removeSelection({b,a}, {})); // Earlier staged deletion must roll back.
    QCOMPARE(doc.toJsonBytes(), before);
    // Explicitly selecting the incident wire needs no detached identity.
    QVERIFY(doc.removeSelection({a}, {*wire}));
    QVERIFY(!doc.component(a));
    QVERIFY(doc.component(b));
    QVERIFY(doc.wires().isEmpty());
    QVERIFY(doc.nodes().isEmpty());
}

void TstDocument::freeEndCompletionIsAtomicAndRoundTrips()
{
    Document doc;
    const auto a = doc.addComponent(SymbolIds::Resistor, {});
    const auto b = doc.addComponent(SymbolIds::Resistor, {100,0});
    const auto wire = doc.addWire(makeTerminalEndpoint(a,1), {}, makeTerminalEndpoint(b,0));
    QVERIFY(wire);
    const auto before = doc.toJsonBytes();
    QVERIFY(!doc.addWireToPoint(BranchSite{*wire,{50,0}}, {{50,20},{50,10}}, {50,40}));
    QVERIFY(!doc.addWireToPoint(makeTerminalEndpoint(a,1), {}, {20,0}));
    QVERIFY(!doc.addWireToPoint(makeTerminalEndpoint(a,1), {}, {qInf(),0}));
    QCOMPARE(doc.toJsonBytes(), before);
    const auto loose = doc.addWireToPoint(BranchSite{*wire,{50,0}}, {}, {50,40});
    QVERIFY(loose);
    QCOMPARE(doc.wires().size(), 3);
    QCOMPARE(doc.nodes().size(), 2);
    QVERIFY(isNode(doc.wire(*loose)->start));
    QVERIFY(isNode(doc.wire(*loose)->end));
    QVERIFY(doc.wire(*loose)->start != doc.wire(*loose)->end);
    QCOMPARE(doc.wire(*loose)->vertices, QVector<QPointF>({{50,0},{50,40}}));
    QCOMPARE(doc.junctionPoints(), QVector<QPointF>({{50,0}}));
    const auto loaded = Document::fromJsonBytes(doc.toJsonBytes());
    QVERIFY(loaded.document);
    QCOMPARE(loaded.document->toJsonBytes(), doc.toJsonBytes());
    const auto node = std::get<NodeId>(doc.wire(*loose)->end);
    QVERIFY(doc.removeWire(*loose));
    QVERIFY(!doc.nodes().contains(node));
}

void TstDocument::branchingSplitsRoutesAtInteriorPoints()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolIds::Resistor, QPointF(100, 0));
    const ComponentId c = document.addComponent(SymbolIds::Resistor, QPointF(60, 40));
    const auto wire = document.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0));
    QVERIFY(wire);
    const auto node = document.branchWireAt(*wire, QPointF(40, 0));
    QVERIFY(node);
    QCOMPARE(document.wires().size(), 2);
    QVERIFY(!document.wire(*wire)); // Split retires the original route identity.
    QVERIFY(document.addWire(makeNodeEndpoint(*node), {}, makeTerminalEndpoint(c, 0)));
    QCOMPARE(document.junctionPoints(), QVector<QPointF>({QPointF(40, 0)}));
    const QVector<Net> nets = document.computeNets();
    QCOMPARE(netContaining(nets, TerminalRef{a, 1}), netContaining(nets, TerminalRef{b, 0}));
    QCOMPARE(netContaining(nets, TerminalRef{a, 1}), netContaining(nets, TerminalRef{c, 0}));

    const auto another = document.addWire(makeTerminalEndpoint(a, 0),
                                          {QPointF(-20, 20), QPointF(120, 20)},
                                          makeTerminalEndpoint(b, 1));
    QVERIFY(another);
    const int routeCountBeforeRejectedBranches = document.wires().size();
    QVERIFY(!document.branchWireAt(*another, QPointF(10, 10)));
    QVERIFY(!document.branchWireAt(*another, QPointF(-20, 0)));
    QCOMPARE(document.wires().size(), routeCountBeforeRejectedBranches);

    const auto cornerBranch = document.branchWireAt(*another, QPointF(-20, 20));
    QVERIFY(cornerBranch);
    QVERIFY(!document.wire(*another));
    QCOMPARE(document.wires().size(), 5);
    for (const WireRoute &route : document.wires())
        QCOMPARE(validateRouteGeometry(route.vertices), RouteGeometryError::None);
}

void TstDocument::invalidRoutesAreRejectedAtomically()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolIds::Resistor, QPointF(100, 0));
    const int componentCount = document.components().size();
    const auto reject = [&document, a, b](const QVector<QPointF> &corners) {
        return !document.addWire(makeTerminalEndpoint(a, 1), corners, makeTerminalEndpoint(b, 0));
    };
    QVERIFY(reject({QPointF(40, 10)})); // Diagonal from the first endpoint.
    QVERIFY(reject({QPointF(20, 0)})); // Zero-length first segment.
    QVERIFY(reject({QPointF(std::numeric_limits<qreal>::infinity(), 0)}));
    QVERIFY(!document.addWire(makeTerminalEndpoint(a, 9), {}, makeTerminalEndpoint(b, 0)));
    QVERIFY(!document.addWire(makeTerminalEndpoint(999, 0), {}, makeTerminalEndpoint(b, 0)));
    QVERIFY(!document.addWire(makeTerminalEndpoint(a, 1), {}, makeNodeEndpoint(999)));
    QCOMPARE(document.components().size(), componentCount);
    QVERIFY(document.wires().isEmpty());
    QCOMPARE(document.nodes().size(), 0);
}

void TstDocument::gridSpacingMustBePositiveAndFinite()
{
    Document document;
    QCOMPARE(document.gridSpacing(), 10.0);
    QVERIFY(document.setGridSpacing(12.5));
    QCOMPARE(document.gridSpacing(), 12.5);
    QVERIFY(!document.setGridSpacing(0.0));
    QVERIFY(!document.setGridSpacing(-1.0));
    QVERIFY(!document.setGridSpacing(std::numeric_limits<qreal>::quiet_NaN()));
    QCOMPARE(document.gridSpacing(), 12.5);
}

void TstDocument::addWireBranchingAppliesSplitsAtomically()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolIds::Resistor, QPointF(100, 0));
    const ComponentId c = document.addComponent(SymbolIds::Resistor, QPointF(50, 50));
    const auto wire = document.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0));
    QVERIFY(wire);

    const auto bridge = document.addWireBranching(BranchSite{*wire, QPointF(40, 0)},
                                                   {QPointF(40, 50)}, makeTerminalEndpoint(c, 0));
    QVERIFY(bridge);
    QVERIFY(!document.wire(*wire)); // The branch split retires the original route identity.
    QCOMPARE(document.wires().size(), 3); // left leg + right leg + the new bridge wire.
    QCOMPARE(document.nodes().size(), 1);
    QCOMPARE(document.nodes().value(1), QPointF(40, 0));
    QCOMPARE(document.junctionPoints(), QVector<QPointF>({QPointF(40, 0)}));

    const QVector<Net> nets = document.computeNets();
    QCOMPARE(netContaining(nets, TerminalRef{a, 1}), netContaining(nets, TerminalRef{b, 0}));
    QCOMPARE(netContaining(nets, TerminalRef{a, 1}), netContaining(nets, TerminalRef{c, 0}));
}

void TstDocument::addWireBranchingFailureLeavesDocumentUnchanged()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolIds::Resistor, QPointF(100, 0));
    const ComponentId c = document.addComponent(SymbolIds::Resistor, QPointF(50, 50));
    const auto wire = document.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0));
    QVERIFY(wire);
    const int wiresBefore = document.wires().size();
    const int nodesBefore = document.nodes().size();

    // Rejected: both endpoints branch off the same wire (current scope limit).
    QVERIFY(!document.addWireBranching(BranchSite{*wire, QPointF(30, 0)}, {},
                                       BranchSite{*wire, QPointF(70, 0)}));
    QCOMPARE(document.wires().size(), wiresBefore);
    QCOMPARE(document.nodes().size(), nodesBefore);
    QVERIFY(document.wire(*wire)); // The original route must not have been split.

    // Rejected: the branch point does not lie on the given wire.
    QVERIFY(!document.addWireBranching(BranchSite{*wire, QPointF(500, 500)}, {},
                                       makeTerminalEndpoint(c, 0)));
    QCOMPARE(document.wires().size(), wiresBefore);
    QCOMPARE(document.nodes().size(), nodesBefore);

    // Rejected: the completed route's own geometry is invalid (diagonal).
    QVERIFY(!document.addWireBranching(BranchSite{*wire, QPointF(40, 0)}, {},
                                       makeTerminalEndpoint(c, 0)));
    QCOMPARE(document.wires().size(), wiresBefore);
    QCOMPARE(document.nodes().size(), nodesBefore);
    QVERIFY(document.wire(*wire)); // Still not split by any of the failed attempts.

    // None of the failures above may have advanced node/wire ID allocation:
    // a subsequent successful branch must still get the first available IDs.
    const auto bridge = document.addWireBranching(BranchSite{*wire, QPointF(40, 0)},
                                                   {QPointF(40, 50)}, makeTerminalEndpoint(c, 0));
    QVERIFY(bridge);
    QCOMPARE(document.nodes().size(), 1);
    QVERIFY(document.nodes().contains(1)); // The first available NodeId, unconsumed by failures above.
}

void TstDocument::setComponentLabelsTrimsBeforeValidationAndStorage()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));

    QVERIFY(document.setComponentLabels(a, QStringLiteral(" R1 "), QStringLiteral(" 10k ")));
    QCOMPARE(document.component(a)->reference(), QStringLiteral("R1"));
    QCOMPARE(document.component(a)->value(), QStringLiteral("10k"));
}

void TstDocument::setComponentLabelsRejectsEmptyReference()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
    document.setComponentLabels(a, QStringLiteral("R1"), QStringLiteral("10k"));

    QVERIFY(!document.setComponentLabels(a, QString(), QStringLiteral("22k")));
    QVERIFY(!document.setComponentLabels(a, QStringLiteral("   "), QStringLiteral("22k")));
    // Rejected calls must not change the existing labels.
    QCOMPARE(document.component(a)->reference(), QStringLiteral("R1"));
    QCOMPARE(document.component(a)->value(), QStringLiteral("10k"));
}

void TstDocument::setComponentLabelsRejectsDuplicateReferenceButAllowsSelfRename()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolIds::Resistor, QPointF(100, 0));
    QVERIFY(document.setComponentLabels(a, QStringLiteral("R1"), QString()));
    QVERIFY(document.setComponentLabels(b, QStringLiteral("R2"), QString()));

    // Duplicate (including a whitespace-variant duplicate) is rejected.
    QVERIFY(!document.setComponentLabels(b, QStringLiteral("R1"), QString()));
    QVERIFY(!document.setComponentLabels(b, QStringLiteral(" R1 "), QString()));
    QCOMPARE(document.component(b)->reference(), QStringLiteral("R2"));

    // Renaming a component to its own current reference is allowed.
    QVERIFY(document.setComponentLabels(a, QStringLiteral("R1"), QStringLiteral("10k")));
    QCOMPARE(document.component(a)->reference(), QStringLiteral("R1"));
    QCOMPARE(document.component(a)->value(), QStringLiteral("10k"));
}

void TstDocument::setComponentLabelsValueOnlyEditSucceeds()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
    QVERIFY(document.setComponentLabels(a, QStringLiteral("R1"), QStringLiteral("10k")));

    QVERIFY(document.setComponentLabels(a, QStringLiteral("R1"), QStringLiteral("22k")));
    QCOMPARE(document.component(a)->reference(), QStringLiteral("R1"));
    QCOMPARE(document.component(a)->value(), QStringLiteral("22k"));
}

void TstDocument::routeValidationRejectsCollinearBacktracking()
{
    // Horizontal and vertical reversals, including a full retrace.
    QCOMPARE(validateRouteGeometry({QPointF(0, 0), QPointF(50, 0), QPointF(30, 0)}),
             RouteGeometryError::CollinearBacktrack);
    QCOMPARE(validateRouteGeometry({QPointF(0, 0), QPointF(0, 50), QPointF(0, 30)}),
             RouteGeometryError::CollinearBacktrack);
    QCOMPARE(validateRouteGeometry({QPointF(0, 0), QPointF(50, 0), QPointF(0, 0)}),
             RouteGeometryError::CollinearBacktrack);
    // A reversal after a bend, away from the first segment.
    QCOMPARE(validateRouteGeometry({QPointF(0, 10), QPointF(0, 0), QPointF(50, 0), QPointF(30, 0)}),
             RouteGeometryError::CollinearBacktrack);

    // Collinear segments that keep their direction and genuine bends stay valid.
    QCOMPARE(validateRouteGeometry({QPointF(0, 0), QPointF(30, 0), QPointF(50, 0)}),
             RouteGeometryError::None);
    QCOMPARE(validateRouteGeometry({QPointF(50, 0), QPointF(30, 0), QPointF(0, 0)}),
             RouteGeometryError::None);
    QCOMPARE(validateRouteGeometry({QPointF(0, 0), QPointF(50, 0), QPointF(50, 30), QPointF(20, 30)}),
             RouteGeometryError::None);

    // The same validator guards new routes: a reversing route is rejected.
    Document document;
    const ComponentId a = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolIds::Resistor, QPointF(100, 0));
    QVERIFY(!document.addWire(makeTerminalEndpoint(a, 1), {QPointF(90, 0)},
                              makeTerminalEndpoint(b, 0)));
    QVERIFY(document.wires().isEmpty());
}

void TstDocument::movementRejectsCollinearReversal()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolIds::Resistor, QPointF(100, 0));
    // Terminals sit 20 units either side of the component origin: this route
    // runs (20,0) -> (60,0) -> (80,0) and keeps its direction.
    const auto id = document.addWire(makeTerminalEndpoint(a, 1), {QPointF(60, 0)},
                                     makeTerminalEndpoint(b, 0));
    QVERIFY(id);
    const QVector<QPointF> before = document.wire(*id)->vertices;

    // Moving b left puts its terminal at (40,0), behind the corner at (60,0).
    QVERIFY(!document.moveComponent(b, QPointF(60, 0)));
    QCOMPARE(document.component(b)->position(), QPointF(100, 0));
    QCOMPARE(document.wire(*id)->vertices, before);

    // A move that keeps the same direction is still accepted.
    QVERIFY(document.moveComponent(b, QPointF(110, 0)));
    QCOMPARE(document.wire(*id)->vertices,
             QVector<QPointF>({QPointF(20, 0), QPointF(60, 0), QPointF(90, 0)}));
}

void TstDocument::addWireBranchingRejectsReversalWithoutSideEffects()
{
    Document document;
    const ComponentId a = document.addComponent(SymbolIds::Resistor, QPointF(0, 0));
    const ComponentId b = document.addComponent(SymbolIds::Resistor, QPointF(100, 0));
    // d's terminal 1 is at (60,0).
    const ComponentId d = document.addComponent(SymbolIds::Resistor, QPointF(40, 0));
    const auto wire = document.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0));
    QVERIFY(wire);
    const int wiresBefore = document.wires().size();

    // (40,0) -> (70,0) -> (60,0) doubles back on itself.
    QVERIFY(!document.addWireBranching(BranchSite{*wire, QPointF(40, 0)}, {QPointF(70, 0)},
                                       makeTerminalEndpoint(d, 1)));
    QCOMPARE(document.wires().size(), wiresBefore);
    QCOMPARE(document.nodes().size(), 0);
    QVERIFY(document.wire(*wire)); // Not split by the failed attempt.

    // The failed attempt consumed no IDs: a valid route gets the first node.
    QVERIFY(document.addWireBranching(BranchSite{*wire, QPointF(40, 0)},
                                      {QPointF(40, -20), QPointF(60, -20)},
                                      makeTerminalEndpoint(d, 1)));
    QVERIFY(document.nodes().contains(1));
}

void TstDocument::rotateAndMirrorUpdateRoutesTransactionally()
{
    Document d;
    const ComponentId a = d.addComponent(SymbolIds::Resistor, {0, 0});
    const ComponentId b = d.addComponent(SymbolIds::Resistor, {40, 0}, Rotation::Deg180);
    // b.0 is at (60,0); a.1 at (20,0).
    const auto w = d.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0));
    QVERIFY(w);
    const QVector<QPointF> before = d.wire(*w)->vertices;

    // Rotating b to 0 would put b.0 on (20,0): zero-length route -> rejected.
    QVERIFY(!d.rotateComponent(b, Rotation::Deg0));
    QCOMPARE(d.component(b)->rotation(), Rotation::Deg180);
    QCOMPARE(d.wire(*w)->vertices, before);

    // Mirroring b makes b.0 land on (20,0) as well -> rejected.
    QVERIFY(!d.setComponentMirrored(b, true));
    QVERIFY(!d.component(b)->mirrored());
    QCOMPARE(d.wire(*w)->vertices, before);

    // A valid mirror of a moves a.1 to (-20,0) and the route follows.
    QVERIFY(d.setComponentMirrored(a, true));
    QCOMPARE(d.wire(*w)->vertices.first(), QPointF(-20, 0));
    QCOMPARE(d.wire(*w)->vertices.last(), QPointF(60, 0));

    QVERIFY(!d.rotateComponent(999, Rotation::Deg90));
    QVERIFY(!d.setComponentMirrored(999, true));
}

void TstDocument::canvasFlipsPreserveAxesAndCommitAtomically()
{
    for (const Rotation rotation : {Rotation::Deg0, Rotation::Deg90, Rotation::Deg180, Rotation::Deg270}) {
        for (const bool mirrored : {false, true}) {
            for (const FlipAxis axis : {FlipAxis::Horizontal, FlipAxis::Vertical}) {
                Document document;
                const QPointF origin(100, 200);
                const auto id = document.addComponent(SymbolIds::OpAmpGrid10, origin, rotation);
                QVERIFY(document.setComponentMirrored(id, mirrored));
                QVector<QPointF> terminals;
                for (int i = 0; i < document.terminalCount(*document.component(id)); ++i)
                    terminals.append(document.terminalPosition(*document.component(id), i));
                const auto before = document.toJsonBytes();
                QVERIFY(document.flipComponent(id, axis));
                for (int i = 0; i < terminals.size(); ++i) {
                    const QPointF expected = axis == FlipAxis::Horizontal
                        ? QPointF(2 * origin.x() - terminals[i].x(), terminals[i].y())
                        : QPointF(terminals[i].x(), 2 * origin.y() - terminals[i].y());
                    QCOMPARE(document.terminalPosition(*document.component(id), i), expected);
                }
                QVERIFY(document.flipComponent(id, axis));
                QCOMPARE(document.toJsonBytes(), before);
            }
        }
    }
    Document document;
    const auto a = document.addComponent(SymbolIds::Resistor, {});
    const auto b = document.addComponent(SymbolIds::Resistor, {40, 0}, Rotation::Deg180);
    const auto wire = document.addWire(makeTerminalEndpoint(a, 1), {}, makeTerminalEndpoint(b, 0));
    QVERIFY(wire);
    const auto before = document.toJsonBytes();
    QVERIFY(!document.flipComponent(b, FlipAxis::Horizontal)); // Would collapse the wire.
    QCOMPARE(document.toJsonBytes(), before);
    QVERIFY(!document.rotateComponent(b, Rotation::Deg0)); // Invalid intermediate pose.
    QVERIFY(document.flipComponent(b, FlipAxis::Vertical)); // Final pose is valid.
    QCOMPARE(document.component(b)->rotation(), Rotation::Deg0);
    QVERIFY(document.component(b)->mirrored());
    QCOMPARE(document.wire(*wire)->vertices, QVector<QPointF>({{20, 0}, {60, 0}}));
    QCOMPARE(document.wire(*wire)->end, makeTerminalEndpoint(b, 0));
    QVERIFY(!document.setComponentTransform(b, static_cast<Rotation>(45), false));
    QVERIFY(!document.flipComponent(999, FlipAxis::Vertical));
}

void TstDocument::defaultReferencesUseSmallestUnusedPrefixNumber()
{
    Document d;
    const ComponentId r1 = d.addComponent(SymbolIds::Resistor, {0, 0});
    d.addComponent(SymbolIds::Resistor, {50, 0});
    const ComponentId c1 = d.addComponent(SymbolIds::Capacitor, {100, 0});
    const ComponentId g = d.addComponent(SymbolIds::Ground, {150, 0});
    const ComponentId rv = d.addComponent(SymbolIds::Potentiometer, {200, 0});
    const ComponentId u = d.addComponent(SymbolIds::OpAmpGrid10, {250, 0});
    QCOMPARE(d.component(r1)->reference(), QStringLiteral("R1"));
    QCOMPARE(d.components()[1].reference(), QStringLiteral("R2"));
    QCOMPARE(d.component(c1)->reference(), QStringLiteral("C1"));
    QCOMPARE(d.component(g)->reference(), QStringLiteral("GND1"));
    QCOMPARE(d.component(rv)->reference(), QStringLiteral("VR1"));
    QCOMPARE(d.component(u)->reference(), QStringLiteral("U1"));

    QVERIFY(d.setComponentLabels(r1, QStringLiteral("X"), QString()));
    const ComponentId r3 = d.addComponent(SymbolIds::Resistor, {300, 0});
    QCOMPARE(d.component(r3)->reference(), QStringLiteral("R1")); // freed number reused
}

void TstDocument::addComponentRejectsUnknownSymbolWithoutConsumingId()
{
    Document document;
    QCOMPARE(document.addComponent(SymbolId(QStringLiteral("NotARealSymbol")), QPointF(0, 0)),
             kInvalidComponentId);
    QCOMPARE(document.addComponent(SymbolId(), QPointF(0, 0)), kInvalidComponentId);
    QVERIFY(document.components().isEmpty());
    QCOMPARE(document.addComponent(SymbolIds::Resistor, QPointF(0, 0)), ComponentId(1));
}
