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
