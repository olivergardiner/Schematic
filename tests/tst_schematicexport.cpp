#include "../schematicsvgexporter.h"
#include "../schematicscene.h"
#include "../schematicitems.h"
#include "document.h"

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
    void noteAppearanceExport();
    void wrappedNotesExportWithoutFrames();
    void vectorLabelsFillsAndWhiteBackground();
    void tightBoundsAndTransforms();
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
