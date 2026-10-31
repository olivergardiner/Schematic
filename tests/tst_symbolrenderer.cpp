#include "../symbolrenderer.h"

#include <QImage>
#include <QPainter>
#include <QtTest>

class SymbolRendererTests : public QObject
{
    Q_OBJECT

private slots:
    void previewFitsTargetAndPaintsArtwork();
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

QTEST_GUILESS_MAIN(SymbolRendererTests)
#include "tst_symbolrenderer.moc"
