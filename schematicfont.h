#ifndef SCHEMATICFONT_H
#define SCHEMATICFONT_H

#include <QFont>
#include <QSettings>

// Application-wide drawing text preferences. Sizes are scene units, so zoom
// and export scale text with the rest of the schematic.
namespace SchematicFont {
inline QString family()
{
    const auto saved = QSettings().value("appearance/schematicFontFamily").toString().trimmed();
    return saved.isEmpty() ? QFont().family() : saved;
}
inline int size(const char *key, int fallback)
{
    bool ok = false;
    const int value = QSettings().value(key, fallback).toInt(&ok);
    return ok && value >= 4 && value <= 72 ? value : fallback;
}
inline QFont labels()
{
    QFont font(family()); font.setPixelSize(size("appearance/labelFontSize", 8)); return font;
}
inline QFont notes()
{
    QFont font(family()); font.setPixelSize(size("appearance/noteFontSize", 10)); return font;
}
}
#endif
