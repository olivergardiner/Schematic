#include "tst_instanceproperties.h"
#include "document.h"
#include "documentloadresult.h"
#include <QTest>
#include <QJsonArray>
#include <QJsonDocument>

using Kind = PropertyEdit::Kind;

void TstInstanceProperties::defaultsAndInstanceVisibility()
{
    Document document;
    const auto ground = document.addComponent(SymbolIds::Ground, {});
    const auto resistor = document.addComponent(SymbolIds::Resistor, {100,0});
    QVERIFY(!document.component(ground)->property("reference")->visible);
    QVERIFY(document.component(resistor)->property("reference")->visible);
    QVERIFY(document.component(resistor)->property("value")->visible);
    QVERIFY(document.component(resistor)->value().isEmpty());
    QVERIFY(document.editProperties({ground}, {{Kind::SetVisible,"reference",{},{},true}}));
    auto library = *document.symbol(SymbolIds::Ground); library.showsReference = false;
    QStringList errors; QVERIFY(document.useSymbol(library,&errors));
    QVERIFY(document.component(ground)->property("reference")->visible);
    QVERIFY(document.editProperties({resistor}, {{Kind::Add,"tolerance","Tolerance","±5%",false}}));
    QVERIFY(document.editProperties({resistor}, {{Kind::SetValue,"value",{}," 4k7 "}}));
    QCOMPARE(document.component(resistor)->value(), QString("4k7"));
    QCOMPARE(document.component(resistor)->property("tolerance")->value, QString("±5%"));
    QVERIFY(!document.component(resistor)->property("tolerance")->visible);
    QVERIFY(document.editProperties({resistor}, {{Kind::Rename,"tolerance","Accuracy"}}));
    QCOMPARE(document.component(resistor)->properties().last().id, QString("tolerance"));
    QCOMPARE(document.component(resistor)->properties().last().name, QString("Accuracy"));
    QCOMPARE(propertyIdFromName(" Voltage rating "),QString("voltage_rating"));
    QCOMPARE(propertyIdFromName("5% tolerance"),QString("property_5_tolerance"));
}

void TstInstanceProperties::bulkEditsAreAtomicAndPreserveOtherFields()
{
    Document document;
    const auto a = document.addComponent(SymbolIds::Resistor, {});
    const auto b = document.addComponent(SymbolIds::Resistor, {100,0});
    QVERIFY(document.addWire(makeTerminalEndpoint(a,1),{},makeTerminalEndpoint(b,0)));
    const QJsonValue wires = document.toJson().value("wires");
    QVERIFY(document.editProperties({a}, {{Kind::Add,"tolerance","Tolerance","1%",true}}));
    QVERIFY(document.editProperties({a,b}, {{Kind::SetValue,"tolerance","Tolerance","2%"}}));
    QVERIFY(document.component(a)->property("tolerance")->visible);
    QVERIFY(!document.component(b)->property("tolerance")->visible);
    QVERIFY(document.editProperties({a,b}, {{Kind::SetVisible,"tolerance","Tolerance",{},true}}));
    QCOMPARE(document.component(a)->property("tolerance")->value,QString("2%"));
    QCOMPARE(document.component(b)->property("tolerance")->value,QString("2%"));
    QVERIFY(document.editProperties({a,b}, {{Kind::SetValue,"value",{},"[Multiple values]"}}));
    QCOMPARE(document.component(a)->value(), QString("[Multiple values]"));
    const auto before = document.toJsonBytes();
    for (const auto &operation : QVector<PropertyEdit>{
        {Kind::SetValue,"reference",{},document.component(b)->reference()},
        {Kind::SetValue,"value",{},"multi\nline"}, {Kind::Remove,"reference"},
        {Kind::Rename,"tolerance","Value"}, {Kind::Add,"tolerance","Tolerance"},
        {Kind::Add,"INVALID","Bad"}, {Kind::Add,"different","Tolerance"}}) {
        QStringList errors;
        QVERIFY(!document.editProperties({a,b},{operation},&errors));
        QVERIFY(!errors.isEmpty()); QCOMPARE(document.toJsonBytes(),before);
    }
    QVERIFY(!document.editProperties({a,999}, {{Kind::SetValue,"value",{},"invalid target"}}));
    QCOMPARE(document.toJsonBytes(),before);
    QVERIFY(document.editProperties({a,b}, {{Kind::Remove,"tolerance"}}));
    QVERIFY(!document.component(a)->property("tolerance"));
    QVERIFY(!document.component(b)->property("tolerance"));
    QCOMPARE(document.toJson()["wires"],wires);
}

void TstInstanceProperties::v5ValidationAndLegacyMigration()
{
    Document document;
    const auto id = document.addComponent(SymbolIds::Ground, {});
    QVERIFY(document.editProperties({id}, {{Kind::SetValue,"value",{},"Ground"},
                                         {Kind::Add,"note","Note","Star point",true}}));
    const auto current = document.toJson();
    QCOMPARE(current["formatVersion"].toInt(), 6);
    const auto loaded = Document::fromJson(current); QVERIFY(loaded.document);
    QCOMPARE(loaded.document->toJson(),current);
    for (int version = 1; version <= 4; ++version) {
        auto root = current; root["formatVersion"] = version;
        auto entries = root["components"].toArray(); auto entry = entries[0].toObject();
        entry.remove("properties"); entry["reference"] = "GND1"; entry["value"] = " Ground ";
        if (version == 1) entry.remove("mirrored");
        entries[0] = entry; root["components"] = entries;
        const auto old = Document::fromJson(root);
        QVERIFY2(old.document,qPrintable(old.errors.join('\n')));
        QCOMPARE(old.document->component(id)->properties().size(),2);
        QCOMPARE(old.document->component(id)->value(),QString("Ground"));
        QVERIFY(!old.document->component(id)->property("reference")->visible);
        QVERIFY(old.document->component(id)->property("value")->visible);
        QCOMPARE(old.document->toJson()["formatVersion"].toInt(), 6);
    }
    for (int problem = 0; problem < 7; ++problem) {
        auto root = current; auto entries = root["components"].toArray(); auto entry = entries[0].toObject();
        auto props = entry["properties"].toArray(); auto property = props[0].toObject();
        if (problem == 0) entry["reference"] = "GND1";
        if (problem == 1) props.removeAt(0);
        if (problem == 2) property["visible"] = "yes";
        if (problem == 3) property["value"] = " ";
        if (problem == 4) property["name"] = "renamed reference";
        if (problem == 5) props.append(props[2]);
        if (problem == 6) property["value"] = 42;
        if (problem != 1) props[0] = property;
        entry["properties"] = props; entries[0] = entry; root["components"] = entries;
        const auto invalid = Document::fromJson(root);
        QVERIFY(!invalid.document); QVERIFY(!invalid.errors.isEmpty());
    }
}

void TstInstanceProperties::propertyBlockPersistenceAndTransforms()
{
    Document d; const auto a = d.addComponent(SymbolIds::Resistor, {});
    const auto b = d.addComponent(SymbolIds::Capacitor, {100,0});
    const auto wire = d.addWire(makeTerminalEndpoint(a,1), {}, makeTerminalEndpoint(b,0)); QVERIFY(wire);
    const QJsonValue wires = d.toJson().value("wires");
    QVERIFY(!d.component(a)->propertyBlockOffset());
    QVERIFY(d.setPropertyBlockOffset({a}, QPointF(23.5,-17.25)));
    QCOMPARE(d.toJson().value("wires"), wires);
    auto saved = d.toJsonBytes();
    auto loaded = Document::fromJsonBytes(saved); QVERIFY(loaded.document);
    QCOMPARE(loaded.document->component(a)->propertyBlockOffset(), std::optional<QPointF>(QPointF(23.5,-17.25)));
    QVERIFY(!d.setPropertyBlockOffset({a,9999}, QPointF(1,2)));
    QVERIFY(!d.setPropertyBlockOffset({a}, QPointF(qInf(),0)));
    QCOMPARE(d.toJsonBytes(),saved);
    QVERIFY(d.transformSelection({a,b},{*wire},{SelectionTransform::Kind::Translate,{20,10}}));
    QCOMPARE(d.component(a)->propertyBlockOffset(),std::optional<QPointF>(QPointF(23.5,-17.25)));
    QVERIFY(d.transformSelection({a,b},{*wire},{SelectionTransform::Kind::Rotate,{}, {},1}));
    QCOMPARE(d.component(a)->propertyBlockOffset(),std::optional<QPointF>(QPointF(23.5,-17.25)));
    auto root = loaded.document->toJson(); auto components = root["components"].toArray();
    auto c = components[0].toObject(); c["propertyBlockOffset"] = QJsonObject{{"x",1}};
    components[0]=c; root["components"]=components; QVERIFY(!Document::fromJson(root).document);
    root=loaded.document->toJson(); root["formatVersion"]=5; QVERIFY(!Document::fromJson(root).document);
    components=root["components"].toArray(); c=components[0].toObject(); c.remove("propertyBlockOffset");
    components[0]=c; root["components"]=components; auto legacy=Document::fromJson(root);
    QVERIFY(legacy.document); QVERIFY(!legacy.document->component(a)->propertyBlockOffset());
    QVERIFY(d.setPropertyBlockOffset({a,b},std::nullopt));
    QVERIFY(!d.setPropertyBlockOffset({a,b},std::nullopt));
}

void TstInstanceProperties::clipboardConnectivityAndAtomicPaste()
{
    Document source; const auto a=source.addComponent(SymbolIds::Resistor,{});
    const auto b=source.addComponent(SymbolIds::Capacitor,{100,0});
    const auto g=source.addComponent(SymbolIds::Ground,{50,60});
    auto main=source.addWire(makeTerminalEndpoint(a,1),{},makeTerminalEndpoint(b,0)); QVERIFY(main);
    auto node=source.branchWireAt(*main,{50,0}); QVERIFY(node);
    QVERIFY(source.addWire(makeNodeEndpoint(*node),{},makeTerminalEndpoint(g,0)));
    QVERIFY(source.editProperties({a},{{Kind::SetValue,"value",{},"4k7"}, {Kind::Add,"tolerance","Tolerance","±5%",true}}));
    QVERIFY(source.setPropertyBlockOffset({a},QPointF(25,-20)));
    QVector<WireId> ids; for(const auto &w:source.wires()) ids.append(w.id);
    const auto original=source.toJsonBytes(); QStringList errors;
    const auto bytes=source.copyFragment({a},ids,&errors); QVERIFY2(!bytes.isEmpty(),qPrintable(errors.join('\n')));
    QCOMPARE(source.toJsonBytes(),original);
    const auto fragment=Document::fromJson(QJsonDocument::fromJson(bytes).object()["document"].toObject());
    QVERIFY(fragment.document); QCOMPARE(fragment.document->components().size(),1);
    QCOMPARE(fragment.document->nodes().size(),3); QCOMPARE(fragment.document->junctionPoints().size(),1);
    QVector<ComponentId> pasted; QVector<WireId> routes;
    QVERIFY2(source.pasteFragment(bytes,{10,10},&pasted,&routes,&errors),qPrintable(errors.join('\n')));
    QCOMPARE(pasted.size(),1); QCOMPARE(routes.size(),3); QVERIFY(pasted[0]!=a);
    const auto *copy=source.component(pasted[0]); QCOMPARE(copy->reference(),QString("R2"));
    QCOMPARE(copy->value(),QString("4k7")); QCOMPARE(copy->property("tolerance")->value,QString("±5%"));
    QCOMPARE(copy->propertyBlockOffset(),std::optional<QPointF>(QPointF(25,-20)));
    QCOMPARE(copy->position(),QPointF(10,10));
    for(auto id:routes) for(const auto &end:{source.wire(id)->start,source.wire(id)->end})
        if(isTerminal(end)) QCOMPARE(std::get<TerminalRef>(end).component,pasted[0]);
    QCOMPARE(source.junctionPoints().size(),2);
    const auto beforeBad=source.toJsonBytes();
    QVERIFY(!source.pasteFragment("not json",{},&pasted,&routes,&errors));
    auto bad=QJsonDocument::fromJson(bytes).object(); bad["clipboardVersion"]=2;
    QVERIFY(!source.pasteFragment(QJsonDocument(bad).toJson(),{},nullptr,nullptr,&errors));
    QCOMPARE(source.toJsonBytes(),beforeBad);
    auto changed=*source.symbol(SymbolIds::Resistor); changed.displayName+=" altered";
    QVERIFY(source.useSymbol(changed,&errors)); const auto beforeConflict=source.toJsonBytes();
    QVERIFY(!source.pasteFragment(bytes,{},nullptr,nullptr,&errors)); QCOMPARE(source.toJsonBytes(),beforeConflict);
    Document destination; const auto wireOnly=fragment.document->copyFragment({},ids);
    QVERIFY(!wireOnly.isEmpty()); QVERIFY(destination.pasteFragment(wireOnly,{20,20},&pasted,&routes,&errors));
    QVERIFY(pasted.isEmpty()); QCOMPARE(routes.size(),3); QCOMPARE(destination.junctionPoints().size(),1);
    QVERIFY(destination.components().isEmpty());
}

void TstInstanceProperties::clipboardCoincidentEndpointsStaySeparate()
{
    Document source; const auto a=source.addComponent(SymbolIds::Resistor,{});
    const auto b=source.addComponent(SymbolIds::Resistor,{});
    const auto c=source.addComponent(SymbolIds::Resistor,{100,0});
    const auto d=source.addComponent(SymbolIds::Resistor,{100,30});
    auto x=source.addWire(makeTerminalEndpoint(a,1),{},makeTerminalEndpoint(c,0));
    auto y=source.addWire(makeTerminalEndpoint(b,1),{{60,0},{60,30}},makeTerminalEndpoint(d,0));
    QVERIFY(x); QVERIFY(y); Document target;
    const auto bytes=source.copyFragment({},{*x,*y});
    QVERIFY(target.pasteFragment(bytes,{10,10})); QCOMPARE(target.nodes().size(),4);
    QVERIFY(target.wires()[0].start!=target.wires()[1].start);
    QCOMPARE(target.nodes().value(std::get<NodeId>(target.wires()[0].start)),
             target.nodes().value(std::get<NodeId>(target.wires()[1].start)));
    QVERIFY(target.junctionPoints().isEmpty());
}
