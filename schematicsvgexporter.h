#ifndef SCHEMATICSVGEXPORTER_H
#define SCHEMATICSVGEXPORTER_H

#include <QByteArray>
#include <QString>

class Document;

// Export a pristine scene built from the committed document, independent
// of live scene selection, previews, cursor position and view settings.
class SchematicSvgExporter
{
public:
    static QByteArray render(const Document &document, QString *error = nullptr);
    static bool write(const Document &document, const QString &path, QString *error = nullptr);
};

#endif // SCHEMATICSVGEXPORTER_H
