#include "../symbolrenderer.h"
#include "../symbolmimedata.h"

#include <QImage>
#include <QMimeData>
#include <QPainter>
#include <QtTest>

#include <memory>

class SymbolRendererTests : public QObject
{
    Q_OBJECT

private slots:
    void loosenessScalesWithoutMovingAnchors();
    void consistentAndDoubleStrokes();
    void handDrawnAnchorsAndReview();
    void monochromeFillPixels();
    void valveHeatersRenderWithAndWithoutConnections();
    void previewFitsTargetAndPaintsArtwork();
    void previewPreservesNativeScaleAndShrinksLargeArtwork();
    void pathPrimitivesRenderCurvesFillAndFillRules();
    void symbolMimeDataCarriesKnownVersionedId();
};

void SymbolRendererTests::previewFitsTargetAndPaintsArtwork()
{
    const SymbolLibrary &library = builtinSymbolLibrary();
    const LibrarySymbol *symbol = library.find(SymbolIds::OpAmpGrid10);
    QVERIFY(symbol);

    QImage image(64, 48, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        SymbolRenderer::paintPreview(&painter, *symbol, QRectF(0, 0, 64, 48));
    }

    int paintedPixels = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) != 0)
                ++paintedPixels;
        }
    }
    QVERIFY(paintedPixels > 0);
}

void SymbolRendererTests::previewPreservesNativeScaleAndShrinksLargeArtwork()
{
    for (const int width : {20, 200}) {
        LibrarySymbol symbol;
        symbol.primitives = {{PrimitiveKind::Polygon,
            {{0, 0}, {qreal(width), 0}, {qreal(width), qreal(width) / 2},
             {0, qreal(width) / 2}}, 0.0}};
        QImage image(64, 48, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        {
            QPainter painter(&image);
            SymbolRenderer::paintPreview(&painter, symbol, QRectF(0, 0, 64, 48));
        }
        QRect painted;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (qAlpha(image.pixel(x, y))) painted |= QRect(x, y, 1, 1);
        QVERIFY(!painted.isEmpty());
        // Allow a pixel on either side for the antialiased outline.
        const int expectedWidth = width == 20 ? 20 : 56;
        QVERIFY(painted.width() >= expectedWidth);
        QVERIFY(painted.width() <= expectedWidth + 2);
        QVERIFY(painted.left() >= 3);
        QVERIFY(painted.right() < 61);
    }
}

void SymbolRendererTests::pathPrimitivesRenderCurvesFillAndFillRules()
{
    LibrarySymbol symbol;
    SymbolPrimitive path;
    path.kind = PrimitiveKind::Path;
    path.filled = true;
    path.fillRule = PathFillRule::EvenOdd;
    path.segments = {
        {PathSegmentKind::MoveTo, {{0, 0}}},
        {PathSegmentKind::LineTo, {{20, 0}}},
        {PathSegmentKind::CubicTo, {{20, 20}, {0, 20}, {0, 0}}},
        {PathSegmentKind::Close, {}},
        {PathSegmentKind::MoveTo, {{5, 5}}},
        {PathSegmentKind::LineTo, {{15, 5}}},
        {PathSegmentKind::LineTo, {{15, 15}}},
        {PathSegmentKind::LineTo, {{5, 15}}},
        {PathSegmentKind::Close, {}},
    };
    symbol.primitives = {path};

    QVector<RenderedSymbolPrimitive> rendered = SymbolRenderer::primitives(symbol);
    QCOMPARE(rendered.size(), 1);
    QVERIFY(rendered[0].filled);
    QCOMPARE(rendered[0].path.fillRule(), Qt::OddEvenFill);
    QVERIFY(rendered[0].path.contains(QPointF(2, 2)));
    QVERIFY(!rendered[0].path.contains(QPointF(10, 10)));
    QVERIFY(rendered[0].path.elementCount() > 8);

    QImage evenOddImage(48, 48, QImage::Format_ARGB32_Premultiplied);
    evenOddImage.fill(Qt::transparent);
    {
        QPainter painter(&evenOddImage);
        SymbolRenderer::paintPreview(&painter, symbol, QRectF(0, 0, 48, 48));
    }
    int paintedPixels = 0;
    for (int y = 0; y < evenOddImage.height(); ++y)
        for (int x = 0; x < evenOddImage.width(); ++x)
            if (qAlpha(evenOddImage.pixel(x, y)) > 0) ++paintedPixels;
    QVERIFY(paintedPixels > 0);
    QCOMPARE(qAlpha(evenOddImage.pixel(24, 24)), 0);

    symbol.primitives[0].fillRule = PathFillRule::NonZero;
    rendered = SymbolRenderer::primitives(symbol);
    QCOMPARE(rendered[0].path.fillRule(), Qt::WindingFill);
    QVERIFY(rendered[0].path.contains(QPointF(10, 10)));

    symbol.primitives[0].filled.reset();
    rendered = SymbolRenderer::primitives(symbol);
    QVERIFY(!rendered[0].filled); // Open Path default.

    SymbolPrimitive polygon;
    polygon.kind = PrimitiveKind::Polygon;
    polygon.points = {{0, 0}, {10, 0}, {10, 10}};
    symbol.primitives = {polygon};
    rendered = SymbolRenderer::primitives(symbol);
    QCOMPARE(rendered.size(), 1);
    QVERIFY(rendered[0].filled); // Legacy Polygon default remains filled.
}

void SymbolRendererTests::symbolMimeDataCarriesKnownVersionedId()
{
    std::unique_ptr<QMimeData> mimeData(SymbolMimeData::create(SymbolIds::OpAmpGrid10));
    QCOMPARE(QString::fromLatin1(SymbolMimeData::kSymbolIdMimeType),
             QStringLiteral("application/vnd.schematic.symbol-id-v1"));
    QCOMPARE(mimeData->formats(), QStringList{QString::fromLatin1(
                 SymbolMimeData::kSymbolIdMimeType)});
    const std::optional<SymbolId> decoded = SymbolMimeData::read(mimeData.get());
    QVERIFY(decoded);
    QCOMPARE(decoded->value(), SymbolIds::OpAmpGrid10.value());

    QMimeData unknown;
    unknown.setData(QString::fromLatin1(SymbolMimeData::kSymbolIdMimeType),
                    QByteArrayLiteral("unknown-symbol"));
    QVERIFY(!SymbolMimeData::read(&unknown));

    QMimeData wrongVersion;
    wrongVersion.setData(QStringLiteral("application/vnd.schematic.symbol-id-v2"),
                         SymbolIds::OpAmpGrid10.value().toUtf8());
    QVERIFY(!SymbolMimeData::read(&wrongVersion));
}

void SymbolRendererTests::monochromeFillPixels()
{
    LibrarySymbol symbol;
    SymbolPrimitive polygon;
    polygon.kind = PrimitiveKind::Polygon;
    polygon.points = {{0,0}, {20,0}, {20,20}, {0,20}};
    for (int mode = 0; mode < 3; ++mode) {
        polygon.filled = mode != 0;
        polygon.fillColor = mode == 2 ? PrimitiveFillColor::Black : PrimitiveFillColor::White;
        symbol.primitives = {polygon};
        QImage image(48, 48, QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
        { QPainter painter(&image); SymbolRenderer::paintPreview(&painter, symbol, {0,0,48,48}); }
        const auto pixel = image.pixelColor(24,24);
        if (!mode) QCOMPARE(pixel.alpha(), 0);
        else QCOMPARE(pixel, QColor(mode == 2 ? Qt::black : Qt::white));
    }
    SymbolPrimitive path;
    path.kind = PrimitiveKind::Path; path.filled = true;
    path.fillColor = PrimitiveFillColor::Black; path.fillRule = PathFillRule::EvenOdd;
    path.segments = {{PathSegmentKind::MoveTo, {{0,0}}}, {PathSegmentKind::LineTo, {{20,0}}},
        {PathSegmentKind::LineTo, {{20,20}}}, {PathSegmentKind::LineTo, {{0,20}}}, {PathSegmentKind::Close, {}},
        {PathSegmentKind::MoveTo, {{5,5}}}, {PathSegmentKind::LineTo, {{15,5}}},
        {PathSegmentKind::LineTo, {{15,15}}}, {PathSegmentKind::LineTo, {{5,15}}}, {PathSegmentKind::Close, {}}};
    symbol.primitives = {path};
    QImage image(48,48,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent);
    { QPainter painter(&image); SymbolRenderer::paintPreview(&painter, symbol, {0,0,48,48}); }
    QCOMPARE(image.pixelColor(16,16), QColor(Qt::black));
    QCOMPARE(image.pixelColor(24,24).alpha(), 0);
}


void SymbolRendererTests::valveHeatersRenderWithAndWithoutConnections()
{
    QImage sheet(1200, 520, QImage::Format_ARGB32_Premultiplied);
    sheet.fill(Qt::white);
    int column = 0;
    for (const auto &family : {QString("Triode"), QString("Pentode"),
                              QString("PentodeCathodeSuppressor"), QString("BeamTetrode"), QString("Diode")}) {
        for (int row=0; row<2; ++row) {
            const auto *symbol = builtinSymbolLibrary().find(SymbolId("Valve." + family +
                (row == 0 ? ".WithHeaters" : ".NoHeaters")));
            QVERIFY(symbol);
            QImage image(120,120,QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::white);
            {
                QPainter painter(&image);
                painter.setPen(QPen(Qt::black, 1));
                painter.translate(60,60);
                for (const auto &primitive : SymbolRenderer::primitives(*symbol)) {
                    painter.setBrush(SymbolRenderer::fillBrush(primitive));
                    painter.drawPath(primitive.path);
                }
            }
            // Always-visible heater apex, cathode, envelope; external heater lead only in row 0.
            QVERIFY(image.pixelColor(60,84).lightness() < 128);
            QVERIFY(image.pixelColor(60,80).lightness() < 128);
            QVERIFY(image.pixelColor(95,60).lightness() < 128);
            QCOMPARE(image.pixelColor(50,103).lightness() < 128, row == 0);
            QPainter painter(&sheet);
            painter.drawImage(QRect(column*240, row*260,240,240), image);
        }
        ++column;
    }
    // Optional review sheet uses precisely the application's primitive conversion and brushes.
    const QString output = qEnvironmentVariable("SCHEMATIC_VALVE_REVIEW_IMAGE");
    if (!output.isEmpty()) QVERIFY(sheet.save(output));
}

QTEST_GUILESS_MAIN(SymbolRendererTests)
#include "tst_symbolrenderer.moc"

void SymbolRendererTests::handDrawnAnchorsAndReview()
{
    QPainterPath path; path.moveTo(0,0); path.lineTo(60,0); path.lineTo(60,40);
    const auto styled = SymbolRenderer::handDrawnPath(path, 17);
    QCOMPARE(styled, SymbolRenderer::handDrawnPath(path, 17));
    QVERIFY(styled != path);
    QVERIFY(styled != SymbolRenderer::handDrawnPath(path, 18));
    QCOMPARE(styled.currentPosition(), QPointF(60,40));
    bool corner = false;
    for (int i=0; i<styled.elementCount(); ++i) {
        const auto e=styled.elementAt(i);
        if (e.x==60 && e.y==0) corner=true;
    }
    QVERIFY(corner);
    QImage sheet(1000,400,QImage::Format_ARGB32_Premultiplied); sheet.fill(Qt::white);
    QPainter painter(&sheet); painter.setRenderHint(QPainter::Antialiasing);
    const QStringList ids = {"Resistor", "Capacitor", "Diode", "Valve.Triode.WithHeaters", "Valve.Pentode.WithHeaters"};
    int column=0;
    for (const auto &id : ids) {
        const auto *symbol=builtinSymbolLibrary().find(SymbolId(id)); QVERIFY2(symbol,qPrintable(id));
        for(int row=0; row<2; ++row) {
            painter.save(); painter.translate(column*200+100,row*200+100); painter.scale(2,2);
            painter.setPen(QPen(Qt::black,1));
            for(const auto &p : SymbolRenderer::primitives(*symbol)) {
                const auto output=row ? SymbolRenderer::handDrawnPath(p.path,17) : p.path;
                QCOMPARE(output.fillRule(),p.path.fillRule());
                QCOMPARE(output.currentPosition(),p.path.currentPosition());
                painter.setBrush(SymbolRenderer::fillBrush(p)); painter.drawPath(output);
            }
            painter.restore();
        }
        ++column;
    }
    painter.end();
    QVERIFY(sheet.save(QCoreApplication::applicationDirPath()+"/drawing-style-review.png"));
}

void SymbolRendererTests::consistentAndDoubleStrokes()
{
    const auto *symbol=builtinSymbolLibrary().find(SymbolIds::Capacitor); QVERIFY(symbol);
    const auto a=SymbolRenderer::handDrawnPrimitives(*symbol,1,true,2);
    const auto b=SymbolRenderer::handDrawnPrimitives(*symbol,999,true,2);
    const auto individual=SymbolRenderer::handDrawnPrimitives(*symbol,1,false,1);
    const auto other=SymbolRenderer::handDrawnPrimitives(*symbol,999,false,1);
    const auto original=SymbolRenderer::primitives(*symbol);
    QCOMPARE(a.size(),original.size()); bool differs=false;
    for(int i=0;i<a.size();++i) {
        QCOMPARE(a[i].path,b[i].path); QCOMPARE(a[i].secondaryPath,b[i].secondaryPath);
        QVERIFY(!a[i].secondaryPath.isEmpty()); QVERIFY(a[i].secondaryPath!=a[i].path);
        QCOMPARE(a[i].path.currentPosition(),original[i].path.currentPosition());
        QCOMPARE(a[i].secondaryPath.currentPosition(),original[i].path.currentPosition());
        QCOMPARE(individual[i].path,SymbolRenderer::handDrawnPath(original[i].path,1u*2654435761u+i+1));
        QVERIFY(individual[i].secondaryPath.isEmpty());
        if(individual[i].path!=other[i].path) differs=true;
    }
    QVERIFY(differs);
    QPainterPath wire; wire.moveTo(0,0); wire.lineTo(100,0);
    const auto single=SymbolRenderer::handDrawnWire(wire,1,1);
    const auto dual=SymbolRenderer::handDrawnWire(wire,1,2);
    QVERIFY(dual.elementCount()>single.elementCount()); QCOMPARE(dual.currentPosition(),QPointF(100,0));
}

void SymbolRendererTests::loosenessScalesWithoutMovingAnchors()
{
    QPainterPath path; path.moveTo(0,0); path.lineTo(60,0); path.cubicTo(70,0,70,20,60,20);
    QCOMPARE(SymbolRenderer::handDrawnPath(path,17,0),path);
    const auto normal=SymbolRenderer::handDrawnPath(path,17);
    QCOMPARE(normal,SymbolRenderer::handDrawnPath(path,17,1));
    const auto loose=SymbolRenderer::handDrawnPath(path,17,3);
    QVERIFY(loose!=normal); QCOMPARE(loose.currentPosition(),path.currentPosition());
    QCOMPARE(loose,SymbolRenderer::handDrawnPath(path,17,3));
    // The first generated line control's lateral offset scales exactly.
    QVERIFY(qAbs(loose.elementAt(1).y-normal.elementAt(1).y*3)<1e-10);
}
