#include "../schematicsvgexporter.h"
#include "../symbolrenderer.h"
#include "../schematicfont.h"
#include <QSettings>
#include <QScopeGuard>
#include "../schematicscene.h"
#include "../schematicitems.h"
#include "document.h"
#include "documentloadresult.h"

#include <QFile>
#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QTextBlock>
#include <QtTest>

class ExportScene : public SchematicScene
{
public:
    void press(QPointF point) {
        QGraphicsSceneMouseEvent event(QEvent::GraphicsSceneMousePress);
        event.setScenePos(point); event.setButton(Qt::LeftButton); event.setButtons(Qt::LeftButton);
        mousePressEvent(&event);
    }
};

class SchematicExportTests : public QObject
{
    Q_OBJECT
private slots:
    void globalFontsReachSvgAndNoteLayout();
    void drawingOptionsExportAndReview();
    void handDrawnExportMatchesSceneAndTopology();
    void noteAppearanceExport();
    void wrappedNotesExportWithoutFrames();
    void vectorLabelsFillsAndWhiteBackground();
    void tightBoundsAndTransforms();
    void powerManagementLeadTipsRender();
    void potentiometerArtworkTracksTerminals();
    void liveEditingStateIsExcluded();
    void fileFailuresPreserveExistingOutput();
};

void SchematicExportTests::noteAppearanceExport()
{
    Document doc; const auto id=doc.addNote({},"Short\nSecond",120);
    QVERIFY(doc.setNoteAppearance(id,PropertyJustification::Right,"#ffe080","#204080"));
    NoteItem item(*doc.note(id));
    QCOMPARE(item.document()->firstBlock().blockFormat().alignment(),Qt::Alignment(Qt::AlignRight));
    QCOMPARE(item.document()->lastBlock().blockFormat().alignment(),Qt::Alignment(Qt::AlignRight));
    const auto bytes=SchematicSvgExporter::render(doc);
    QSvgRenderer renderer(bytes); QVERIFY(renderer.isValid());
    QImage image(renderer.defaultSize()*4,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
    { QPainter painter(&image); renderer.render(&painter); }
    int fill=0,outline=0;
    for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x) {
        const auto colour=image.pixelColor(x,y);
        if(colour==QColor("#ffe080")) ++fill;
        if(colour==QColor("#204080")) ++outline;
    }
    QVERIFY(fill>100); QVERIFY(outline>50);
    QVERIFY(image.save(QCoreApplication::applicationDirPath()+"/styled-note-export.png"));
}

void SchematicExportTests::wrappedNotesExportWithoutFrames()
{
    Document doc;
    const QString text = "A wrapped note with several words.\n\nSecond <plain> paragraph & punctuation.";
    const auto note = doc.addNote({10,-20},text,160);
    QVERIFY(note);
    const auto wide = SchematicSvgExporter::render(doc);
    QSvgRenderer wideRenderer(wide); QVERIFY(wideRenderer.isValid());
    QVERIFY(doc.setNote(note,{10,-20},text,80));
    const auto narrow = SchematicSvgExporter::render(doc);
    QSvgRenderer renderer(narrow); QVERIFY(renderer.isValid());
    QVERIFY(renderer.viewBoxF().height() > wideRenderer.viewBoxF().height());
    QVERIFY(renderer.viewBoxF().width() < wideRenderer.viewBoxF().width());
    QXmlStreamReader xml(narrow);
    QString labels; int images = 0;
    while (!xml.atEnd()) {
        xml.readNext(); if (!xml.isStartElement()) continue;
        if (xml.name() == QStringLiteral("image")) ++images;
        for (const auto &attribute : xml.attributes())
            if (attribute.name() == QStringLiteral("stroke"))
                QVERIFY(attribute.value() == QStringLiteral("none") || QColor(attribute.value().toString()) == Qt::black);
        if (xml.name() == QStringLiteral("text")) labels += xml.readElementText(QXmlStreamReader::IncludeChildElements) + ' ';
    }
    QVERIFY(!xml.hasError()); QCOMPARE(images,0);
    QVERIFY(labels.contains("Second")); QVERIFY(labels.contains("<plain>")); QVERIFY(labels.contains("punctuation."));
    QImage image(renderer.defaultSize()*4,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::white);
    { QPainter painter(&image); renderer.render(&painter); }
    QVERIFY(image.save(QCoreApplication::applicationDirPath()+"/wrapped-note-export.png"));
}

void SchematicExportTests::vectorLabelsFillsAndWhiteBackground()
{
    Document doc;
    const auto a = doc.addComponent(SymbolIds::Resistor, {});
    const auto b = doc.addComponent(SymbolIds::Resistor, {100,0});
    QVERIFY(doc.addWire(makeTerminalEndpoint(a,1), {}, makeTerminalEndpoint(b,0)));
    const auto rail = doc.placeComponent(SymbolIds::RailVCC, {50,-20});
    QVERIFY(rail != kInvalidComponentId);
    QVERIFY(doc.setComponentLabels(a, "R1", "10k & <test>"));
    QVERIFY(doc.setComponentLabels(b, "R2", "hidden-value"));
    QVERIFY(doc.editProperties({b}, {{PropertyEdit::Kind::SetVisible,"value",{},{},false}}));
    const auto before = doc.toJsonBytes();
    QString error;
    const auto bytes = SchematicSvgExporter::render(doc, &error);
    QVERIFY2(!bytes.isEmpty(), qPrintable(error));
    QCOMPARE(doc.toJsonBytes(), before);
    QXmlStreamReader xml(bytes);
    QString labels;
    int paths = 0, images = 0, dots = 0;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;
        if (xml.name() == QStringLiteral("image")) ++images;
        if (xml.name() == QStringLiteral("path")) ++paths;
        if (xml.name() == QStringLiteral("ellipse")
            && xml.attributes().value("rx").toDouble() == 2.5
            && xml.attributes().value("ry").toDouble() == 2.5) ++dots;
        if (xml.name() == QStringLiteral("circle")
            && xml.attributes().value("r").toDouble() == 2.5) ++dots;
        for (const auto &attribute : xml.attributes()) {
            if (attribute.name() == QStringLiteral("fill") || attribute.name() == QStringLiteral("stroke")) {
                const auto value = attribute.value().toString();
                if (value == "none") continue;
                const QColor colour(value);
                QVERIFY2(colour == Qt::black || colour == Qt::white, qPrintable(value));
            }
        }
        if (xml.name() == QStringLiteral("text")) labels += xml.readElementText(QXmlStreamReader::IncludeChildElements);
    }
    QVERIFY2(!xml.hasError(), qPrintable(xml.errorString()));
    QVERIFY(paths > 0);
    QCOMPARE(images, 0);
    QCOMPARE(dots, 1);
    QVERIFY(labels.contains("R1"));
    QVERIFY(labels.contains("Vcc"));
    QVERIFY(labels.contains("10k & <test>"));
    QVERIFY(!labels.contains("hidden-value"));
    QSvgRenderer renderer(bytes);
    QVERIFY(renderer.isValid());
    QVERIFY(renderer.viewBoxF().width() < 400);
    QVERIFY(renderer.viewBoxF().height() < 200);
    QImage image(renderer.defaultSize() * 4, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    { QPainter painter(&image); renderer.render(&painter); }
    QCOMPARE(image.pixelColor(0,0), QColor(Qt::white));
    int black = 0, white = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = image.pixelColor(x,y);
            QCOMPARE(pixel.alpha(), 255);
            if (pixel == Qt::black) ++black;
            if (pixel == Qt::white) ++white;
        }
    QVERIFY(black > 20); QVERIFY(white > 20);
    QVERIFY(image.save(QCoreApplication::applicationDirPath() + "/svg-export-preview.png"));
    image.fill(Qt::white);
    { QPainter painter(&image); renderer.render(&painter); }
    QVERIFY(image.save(QCoreApplication::applicationDirPath() + "/svg-export-preview-white.png"));
    QVERIFY(SchematicSvgExporter::write(doc, QCoreApplication::applicationDirPath() + "/svg-export-preview.svg", &error));
}

void SchematicExportTests::tightBoundsAndTransforms()
{
    Document doc;
    const auto a = doc.addComponent(SymbolIds::Resistor, {});
    const auto b = doc.addComponent(SymbolIds::Resistor, {100,0});
    QVERIFY(doc.editProperties({a,b}, {{PropertyEdit::Kind::SetVisible,"reference",{},{},false}}));
    QVERIFY(doc.addWire(makeTerminalEndpoint(a,1), {}, makeTerminalEndpoint(b,0)));
    QSvgRenderer plain(SchematicSvgExporter::render(doc));
    // Artwork runs from -20 to 120, with a half-unit stroke on each side
    // and a ten-unit export margin. The resistor body is ten units tall.
    QCOMPARE(plain.viewBoxF(), QRectF(0,0,161,31));

    Document rotated;
    const auto diode = rotated.addComponent(SymbolIds::Diode, {100,-50}, Rotation::Deg90);
    QVERIFY(rotated.editProperties({diode}, {{PropertyEdit::Kind::SetVisible,"reference",{},{},false}}));
    const auto normal = SchematicSvgExporter::render(rotated);
    QVERIFY(rotated.setComponentMirrored(diode, true));
    const auto mirrored = SchematicSvgExporter::render(rotated);
    QSvgRenderer renderer(mirrored);
    QVERIFY(renderer.isValid());
    QCOMPARE(renderer.viewBoxF(), QRectF(0,0,31,61));
    QVERIFY(normal != mirrored);
    // Fixed model-space probes: cathode bar is above the centre after
    // mirroring before rotation, rather than below it.
    QImage image(124,244,QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    { QPainter painter(&image); renderer.render(&painter); }
    QCOMPARE(image.pixelColor(62,102), QColor(Qt::black)); // (100,-55), on the cathode bar
    QCOMPARE(image.pixelColor(0,0), QColor(Qt::white));
}

void SchematicExportTests::potentiometerArtworkTracksTerminals()
{
    for (const Rotation rotation : {Rotation::Deg0, Rotation::Deg90,
                                      Rotation::Deg180, Rotation::Deg270}) {
        Document document;
        const auto id = document.addComponent(SymbolIds::Potentiometer, {100, -50}, rotation);
        QVERIFY(document.setComponentMirrored(id, true));
        const Component &component = *document.component(id);
        const LibrarySymbol &symbol = *document.symbol(component.symbolId());
        ExportScene scene;
        scene.bindDocument(&document);
        const ComponentItem *artwork = nullptr;
        for (auto *item : scene.items()) {
            if (auto *candidate = qgraphicsitem_cast<ComponentItem *>(item))
                artwork = candidate;
        }
        QVERIFY(artwork);
        for (TerminalId terminal = 0; terminal < symbol.terminals.size(); ++terminal) {
            QCOMPARE(artwork->mapToScene(symbol.terminals[terminal].offset),
                     document.terminalPosition(component, terminal));
        }
    }
}

void SchematicExportTests::powerManagementLeadTipsRender()
{
    struct Pin {
        QString name;
        QPointF position;
    };
    const QMap<QString, QVector<Pin>> symbols{
        {"SwitchedCapacitorInverter", {{"BOOST", {50, -30}}, {"CAP+", {-50, -30}},
            {"GND", {0, 50}}, {"CAP-", {-50, 30}}, {"VOUT", {50, -10}},
            {"LV", {50, 10}}, {"OSC", {50, 30}}, {"V+", {0, -50}}}},
        {"ThreeTerminalRegulator", {{"IN", {-30, 0}}, {"OUT", {30, 0}}, {"GND", {0, 30}}}},
    };
    for (auto entry = symbols.cbegin(); entry != symbols.cend(); ++entry) {
        const SymbolId symbolId(entry.key());
        Document document;
        const auto id = document.addComponent(symbolId, {});
        const LibrarySymbol *symbol = document.symbol(symbolId);
        QVERIFY(symbol);
        QCOMPARE(symbol->terminals.size(), entry->size());
        const auto *component = document.component(id);
        QVERIFY(component);
        SchematicScene scene;
        scene.bindDocument(&document);
        const QRectF source = scene.itemsBoundingRect().adjusted(-10, -10, 10, 10);
        QSvgRenderer renderer(SchematicSvgExporter::render(document));
        QVERIFY(renderer.isValid());
        QCOMPARE(renderer.viewBoxF().size(), source.size());
        QImage canvas((source.size() * 4).toSize(), QImage::Format_RGB32);
        canvas.fill(Qt::white);
        QImage svg = canvas;
        { QPainter painter(&canvas); scene.render(&painter, QRectF(canvas.rect()), source); }
        { QPainter painter(&svg); renderer.render(&painter, QRectF(svg.rect())); }
        for (int i = 0; i < entry->size(); ++i) {
            const auto &pin = entry->at(i);
            QCOMPARE(symbol->terminals[i].name, pin.name);
            QCOMPARE(document.terminalPosition(*component, i), pin.position);
            const QPoint pixel = ((pin.position - source.topLeft()) * 4).toPoint();
            QCOMPARE(canvas.pixelColor(pixel), QColor(Qt::black));
            QCOMPARE(svg.pixelColor(pixel), QColor(Qt::black));
        }
        if (entry.key() == "SwitchedCapacitorInverter") {
            const QByteArray numbered = SchematicSvgExporter::render(document);
            QVERIFY(document.setSymbolLayout(id, "pin_numbers", "hidden"));
            QCOMPARE(document.component(id)->symbolId(), SymbolId("SwitchedCapacitorInverter.NoPinNumbers"));
            for (int i = 0; i < entry->size(); ++i)
                QCOMPARE(document.terminalPosition(*document.component(id), i), entry->at(i).position);
            const QByteArray unnumbered = SchematicSvgExporter::render(document);
            QVERIFY(unnumbered != numbered);
            auto loaded = Document::fromJsonBytes(document.toJsonBytes());
            QVERIFY(loaded.document);
            QCOMPARE(SchematicSvgExporter::render(*loaded.document), unnumbered);
            QVERIFY(loaded.document->setSymbolLayout(id, "pin_numbers", "shown"));
            QCOMPARE(SchematicSvgExporter::render(*loaded.document), numbered);
        }
    }
}

void SchematicExportTests::liveEditingStateIsExcluded()
{
    Document doc;
    const auto a = doc.addComponent(SymbolIds::Resistor, {});
    const auto b = doc.addComponent(SymbolIds::Resistor, {100,0});
    const auto wire = doc.addWire(makeTerminalEndpoint(a,1), {}, makeTerminalEndpoint(b,0));
    QVERIFY(wire);
    const auto baseline = SchematicSvgExporter::render(doc);
    const auto before = doc.toJsonBytes();
    ExportScene scene;
    scene.bindDocument(&doc);
    scene.restoreSelection({{a},{*wire}});
    QCOMPARE(SchematicSvgExporter::render(doc), baseline);
    scene.setEditMode(SchematicScene::EditMode::DrawWire);
    scene.press({20,0}); scene.press({20,200}); // unfinished draft, terminal indicators visible
    QCOMPARE(SchematicSvgExporter::render(doc), baseline);
    QCOMPARE(scene.editMode(), SchematicScene::EditMode::DrawWire);
    QCOMPARE(doc.toJsonBytes(), before);
}

void SchematicExportTests::fileFailuresPreserveExistingOutput()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath("drawing.svg");
    QFile existing(path);
    QVERIFY(existing.open(QIODevice::WriteOnly));
    QCOMPARE(existing.write("keep-me"), qint64(7)); existing.close();
    QString error;
    Document empty;
    QVERIFY(!SchematicSvgExporter::write(empty, path, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(existing.open(QIODevice::ReadOnly));
    QCOMPARE(existing.readAll(), QByteArray("keep-me")); existing.close();
    Document doc;
    doc.addComponent(SymbolIds::Resistor, {});
    QVERIFY(!SchematicSvgExporter::write(doc, directory.filePath("missing/drawing.svg"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(SchematicSvgExporter::write(doc, path, &error));
    QVERIFY(error.isEmpty());
    QVERIFY(existing.open(QIODevice::ReadOnly));
    QSvgRenderer renderer(existing.readAll());
    QVERIFY(renderer.isValid());
}

QTEST_MAIN(SchematicExportTests)
#include "tst_schematicexport.moc"

void SchematicExportTests::handDrawnExportMatchesSceneAndTopology()
{
    Document doc;
    const auto a=doc.addComponent(SymbolIds::Resistor,{});
    const auto b=doc.addComponent(SymbolIds::Resistor,{100,0});
    const auto wire=doc.addWire(makeTerminalEndpoint(a,1),{},makeTerminalEndpoint(b,0)); QVERIFY(wire);
    const auto vertices=doc.wire(*wire)->vertices;
    const auto nets=doc.computeNets();
    const auto standard=SchematicSvgExporter::render(doc);
    QVERIFY(doc.setDrawingStyle(DrawingStyle::HandDrawn));
    const auto bytes=SchematicSvgExporter::render(doc);
    QVERIFY(bytes!=standard);
    QCOMPARE(bytes,SchematicSvgExporter::render(doc));
    QCOMPARE(doc.wire(*wire)->vertices,vertices);
    const auto styledNets=doc.computeNets();
    QCOMPARE(styledNets.size(),nets.size());
    for(int i=0; i<nets.size(); ++i) QCOMPARE(styledNets[i].terminals,nets[i].terminals);
    auto loaded=Document::fromJsonBytes(doc.toJsonBytes()); QVERIFY(loaded.document);
    QCOMPARE(SchematicSvgExporter::render(*loaded.document),bytes);
    SchematicScene scene; scene.bindDocument(&doc);
    const QRectF source=scene.itemsBoundingRect().adjusted(-10,-10,10,10);
    QSvgRenderer renderer(bytes); QVERIFY(renderer.isValid());
    // QSvgGenerator serializes decimal coordinates with limited precision.
    QVERIFY(qAbs(renderer.viewBoxF().width()-source.width())<0.001);
    QVERIFY(qAbs(renderer.viewBoxF().height()-source.height())<0.001);
    QImage canvas(800,400,QImage::Format_ARGB32_Premultiplied); canvas.fill(Qt::white);
    QImage svg=canvas;
    { QPainter p(&canvas); p.setRenderHint(QPainter::Antialiasing); scene.render(&p,QRectF(canvas.rect()),source,Qt::IgnoreAspectRatio); }
    { QPainter p(&svg); renderer.render(&p,QRectF(svg.rect())); }
    int black=0, disagreements=0;
    for(int y=0; y<svg.height(); ++y) for(int x=0; x<svg.width(); ++x) {
        const bool exported=svg.pixelColor(x,y).lightness()<128;
        const bool displayed=canvas.pixelColor(x,y).lightness()<128;
        if(exported) ++black;
        if(exported!=displayed) ++disagreements;
    }
    QVERIFY(black>100);
    QVERIFY2(disagreements<2000,qPrintable(QString::number(disagreements)));
    QCOMPARE(svg.pixelColor(0,0),QColor(Qt::white));
    QVERIFY(svg.save(QCoreApplication::applicationDirPath()+"/hand-drawn-export.png"));
}

void SchematicExportTests::drawingOptionsExportAndReview()
{
    // Double outlines must not cancel the filled body's hit area or its fill.
    QPainterPath body; body.addRect(QRectF(5,5,30,30));
    ComponentPrimitiveItem filled(body,SymbolRenderer::handDrawnPath(body,123));
    filled.setBrush(Qt::white); filled.setPen(QPen(Qt::black,1));
    QVERIFY(filled.shape().contains(QPointF(20,20)));
    QImage probe(40,40,QImage::Format_ARGB32_Premultiplied); probe.fill(Qt::blue);
    { QPainter p(&probe); QStyleOptionGraphicsItem option; filled.paint(&p,&option,nullptr); }
    QCOMPARE(probe.pixelColor(20,20),QColor(Qt::white));
    QCOMPARE(probe.pixelColor(0,0),QColor(Qt::blue));
    QImage review(1200,640,QImage::Format_ARGB32_Premultiplied); review.fill(Qt::white);
    QPainter sheet(&review);
    for(int mode=0;mode<4;++mode) {
        Document doc;
        const auto a=doc.addComponent(SymbolIds::Capacitor,{});
        const auto b=doc.addComponent(SymbolIds::Capacitor,{100,0});
        QVERIFY(doc.addWire(makeTerminalEndpoint(a,1),{},makeTerminalEndpoint(b,0)));
        doc.addComponent(SymbolIds::ValveTriodeWithHeaters,{160,0});
        doc.addComponent(SymbolIds::OpAmpGrid10,{240,0});
        doc.setDrawingStyle(DrawingStyle::HandDrawn);
        doc.setDrawingVariation(mode%2 ? DrawingVariation::Consistent : DrawingVariation::Individual);
        doc.setDrawingStrokes(mode/2+1);
        doc.setDrawingLooseness(mode/2 ? 2.5 : 1.0);
        const auto data=SchematicSvgExporter::render(doc); QVERIFY(!data.isEmpty());
        QCOMPARE(data,SchematicSvgExporter::render(doc));
        auto loaded=Document::fromJsonBytes(doc.toJsonBytes()); QVERIFY(loaded.document);
        QCOMPARE(data,SchematicSvgExporter::render(*loaded.document));
        QSvgRenderer renderer(data); QVERIFY(renderer.isValid());
        const QRectF panel((mode%2)*600,(mode/2)*320,600,320);
        sheet.setPen(Qt::black); sheet.drawText(panel.adjusted(15,5,-5,-5),Qt::AlignTop|Qt::AlignLeft,
            QString(mode%2 ? "Consistent" : "Individual") + (mode/2 ? " / Double / 250%" : " / Single / 100%"));
        renderer.render(&sheet,panel.adjusted(10,35,-10,-10));
        if(mode==3) {
            QImage image(900,360,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
            { QPainter p(&image); renderer.render(&p); }
            QCOMPARE(image.pixelColor(0,0),QColor(Qt::white));
            SchematicScene scene; scene.bindDocument(&doc);
            const auto bounds=scene.itemsBoundingRect().adjusted(-10,-10,10,10);
            QVERIFY(qAbs(bounds.width()-renderer.viewBoxF().width())<0.001);
            QVERIFY(qAbs(bounds.height()-renderer.viewBoxF().height())<0.001);
            // The filled op-amp body stays opaque white with two unfilled outlines.
            bool filled=false;
            for(auto *item : scene.items()) if(auto *component=qgraphicsitem_cast<ComponentItem *>(item)) {
                if(component->componentId()!=doc.components().last().id()) continue;
                for(auto *child:component->childItems()) if(auto *p=qgraphicsitem_cast<QGraphicsPathItem *>(child)) {
                    if(p->brush().color()==Qt::white && p->brush().style()!=Qt::NoBrush) filled=true;
                }
            }
            QVERIFY(filled);
        }
    }
    sheet.end(); QVERIFY(review.save(QCoreApplication::applicationDirPath()+"/drawing-options-review.png"));
}

void SchematicExportTests::globalFontsReachSvgAndNoteLayout()
{
    const auto oldOrg=QCoreApplication::organizationName(), oldApp=QCoreApplication::applicationName();
    const auto oldFormat=QSettings::defaultFormat();
    QTemporaryDir settingsDirectory; QVERIFY(settingsDirectory.isValid());
    QCoreApplication::setOrganizationName("SchematicFontTests"); QCoreApplication::setApplicationName("Fonts");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDirectory.path());
    const auto restoreSettingsContext=qScopeGuard([&] {
        QSettings::setDefaultFormat(oldFormat);
        QCoreApplication::setOrganizationName(oldOrg); QCoreApplication::setApplicationName(oldApp);
    });
    QSettings settings;
    const QStringList keys={"appearance/schematicFontFamily","appearance/labelFontSize","appearance/noteFontSize"};
    QVariantList saved; for(const auto &key:keys) { saved.append(settings.value(key)); settings.remove(key); }
    const auto cleanup=qScopeGuard([&] {
        for(int i=0;i<keys.size();++i) { if(saved[i].isValid()) settings.setValue(keys[i],saved[i]); else settings.remove(keys[i]); }
    });
    Document doc; doc.addComponent(SymbolIds::Resistor,{});
    const auto id=doc.addNote({0,40},"A longer wrapped note for font layout",100);
    const auto before=doc.toJsonBytes();
    const NoteItem normal(*doc.note(id)); const qreal oldHeight=normal.boundingRect().height();
    settings.setValue(keys[0],"Courier New"); settings.setValue(keys[1],12); settings.setValue(keys[2],20);
    NoteItem larger(*doc.note(id)); QVERIFY(larger.boundingRect().height()>oldHeight);
    const auto bytes=SchematicSvgExporter::render(doc); QSvgRenderer renderer(bytes); QVERIFY(renderer.isValid());
    QXmlStreamReader xml(bytes); bool family=false, labelSize=false, noteSize=false;
    while(!xml.atEnd()) {
        xml.readNext(); if(!xml.isStartElement())continue;
        const auto attributes=xml.attributes();
        if(attributes.value("font-family")==QStringLiteral("Courier New")) family=true;
        if(attributes.value("font-size")==QStringLiteral("12")) labelSize=true;
        if(attributes.value("font-size")==QStringLiteral("20")) noteSize=true;
    }
    QVERIFY(!xml.hasError()); QVERIFY(family); QVERIFY(labelSize); QVERIFY(noteSize);
    QCOMPARE(doc.toJsonBytes(),before);
    QImage image(renderer.defaultSize()*3,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::white);
    { QPainter p(&image); renderer.render(&p); }
    QVERIFY(image.save(QCoreApplication::applicationDirPath()+"/font-export.png"));
}
