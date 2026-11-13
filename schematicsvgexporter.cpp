#include "schematicsvgexporter.h"
#include "schematicscene.h"

#include <QBuffer>
#include <QPainter>
#include <QSaveFile>
#include <QSvgGenerator>
#include <cmath>
#include <limits>

QByteArray SchematicSvgExporter::render(const Document &document, QString *error)
{
    if (error) error->clear();
    const auto fail = [error](const QString &message) {
        if (error) *error = message;
        return QByteArray{};
    };
    if (document.components().isEmpty() && document.wires().isEmpty() && document.notes().isEmpty())
        return fail(QStringLiteral("There is nothing to export."));
    Document snapshot = document;
    SchematicScene scene;
    scene.bindDocument(&snapshot); // Select mode: no terminal/hover overlays.
    scene.setBackgroundBrush(QBrush(Qt::NoBrush));
    constexpr qreal margin = 10.0;
    const QRectF source = scene.itemsBoundingRect().adjusted(-margin, -margin, margin, margin);
    const qreal limit = std::numeric_limits<int>::max();
    if (!std::isfinite(source.x()) || !std::isfinite(source.y())
        || !std::isfinite(source.width()) || !std::isfinite(source.height())
        || source.width() <= 0 || source.height() <= 0
        || source.width() > limit || source.height() > limit)
        return fail(QStringLiteral("The drawing bounds are outside the supported SVG size."));

    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly))
        return fail(QStringLiteral("Could not prepare SVG output."));
    QSvgGenerator generator;
    generator.setOutputDevice(&buffer);
    generator.setResolution(96);
    generator.setSize(QSize(int(std::ceil(source.width())), int(std::ceil(source.height()))));
    const QRectF target(QPointF(0, 0), source.size());
    generator.setViewBox(target);
    generator.setTitle(QStringLiteral("Schematic"));
    generator.setDescription(QStringLiteral("Exported from Schematic"));
    QPainter painter;
    if (!painter.begin(&generator)) return fail(QStringLiteral("Could not start SVG rendering."));
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.fillRect(target, Qt::white);
    scene.render(&painter, target, source, Qt::IgnoreAspectRatio);
    if (!painter.end() || bytes.isEmpty())
        return fail(QStringLiteral("Could not finish SVG rendering."));
    return bytes;
}

bool SchematicSvgExporter::write(const Document &document, const QString &path, QString *error)
{
    const QByteArray bytes = render(document, error);
    if (bytes.isEmpty()) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
        if (error) *error = file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}
