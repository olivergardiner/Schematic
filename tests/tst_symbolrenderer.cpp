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
    void previewFitsTargetAndPaintsArtwork();
    void pathPrimitivesRenderCurvesFillAndFillRules();
    void symbolMimeDataCarriesKnownVersionedId();
};

void SymbolRendererTests::previewFitsTargetAndPaintsArtwork()
{
    const SymbolLibrary &library = builtinSymbolLibrary();
    const LibrarySymbol *symbol = library.find(SymbolIds::OpAmp);
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
    std::unique_ptr<QMimeData> mimeData(SymbolMimeData::create(SymbolIds::OpAmp));
    QCOMPARE(QString::fromLatin1(SymbolMimeData::kSymbolIdMimeType),
             QStringLiteral("application/vnd.schematic.symbol-id-v1"));
    QCOMPARE(mimeData->formats(), QStringList{QString::fromLatin1(
                 SymbolMimeData::kSymbolIdMimeType)});
    const std::optional<SymbolId> decoded = SymbolMimeData::read(mimeData.get());
    QVERIFY(decoded);
    QCOMPARE(decoded->value(), SymbolIds::OpAmp.value());

    QMimeData unknown;
    unknown.setData(QString::fromLatin1(SymbolMimeData::kSymbolIdMimeType),
                    QByteArrayLiteral("unknown-symbol"));
    QVERIFY(!SymbolMimeData::read(&unknown));

    QMimeData wrongVersion;
    wrongVersion.setData(QStringLiteral("application/vnd.schematic.symbol-id-v2"),
                         SymbolIds::OpAmp.value().toUtf8());
    QVERIFY(!SymbolMimeData::read(&wrongVersion));
}

QTEST_GUILESS_MAIN(SymbolRendererTests)
#include "tst_symbolrenderer.moc"
