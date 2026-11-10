#include "symbolmanifest.h"
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <functional>

namespace {
bool categoryName(const QString &name)
{
    return !name.isEmpty() && name == name.trimmed() && !name.contains('/')
        && !name.contains('\n') && !name.contains('\r') && !name.contains(QChar(0))
        && !name.contains(QChar(0x2028)) && !name.contains(QChar(0x2029));
}
ManifestCategory *ensureCategory(QVector<ManifestCategory> &roots, const QString &path)
{
    auto *siblings = &roots;
    ManifestCategory *node = nullptr;
    for (const auto &part : path.split('/')) {
        node = nullptr;
        for (auto &candidate : *siblings) if (candidate.name == part) { node = &candidate; break; }
        if (!node) { siblings->append({part, {}, {}}); node = &siblings->last(); }
        siblings = &node->children;
    }
    return node;
}
}

std::optional<SymbolManifest> SymbolManifest::fromJson(const QJsonObject &json, QStringList *errors)
{
    SymbolManifest result;
    QStringList problems;
    QSet<QString> files;
    const QRegularExpression filePattern(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*\\.json$"));
    const auto addFile = [&](const QJsonValue &entry) {
        const auto name = entry.toString();
        if (!entry.isString() || !filePattern.match(name).hasMatch() || files.contains(name)) {
            problems << QStringLiteral("Invalid, unsafe or duplicate symbol filename: %1").arg(name);
            return false;
        }
        files.insert(name); return true;
    };
    result.legacy = json.contains("symbols");
    if (result.legacy) {
        if (!json.value("symbols").isArray()) problems << "symbols must be an array";
        for (const auto &entry : json.value("symbols").toArray())
            if (addFile(entry)) result.legacyEntries.append({entry.toString(), {}});
        if (json.contains("categories")) {
            if (!json.value("categories").isArray()) problems << "categories must be an array";
            QSet<QString> declared;
            for (const auto &entry : json.value("categories").toArray()) {
                const auto path = entry.toString();
                bool valid = entry.isString() && !declared.contains(path);
                for (const auto &part : path.split('/')) valid = valid && categoryName(part);
                if (!valid) problems << "Invalid, duplicate or mixed legacy category path: " + path;
                else { declared.insert(path); ensureCategory(result.categories, path); }
            }
        }
    } else {
        if (!json.value("categories").isArray()) problems << "Manifest requires a categories array";
        std::function<void(const QJsonArray &, QVector<ManifestCategory> &, int)> parse;
        parse = [&](const QJsonArray &array, QVector<ManifestCategory> &nodes, int depth) {
            if (depth > 64) { problems << "Category tree is too deep"; return; }
            QSet<QString> siblings;
            for (const auto &value : array) {
                const auto object = value.toObject(); const auto name = object.value("name").toString();
                if (!value.isObject() || !object.value("name").isString() || !categoryName(name) || siblings.contains(name)) {
                    problems << "Invalid or duplicate category name: " + name; continue;
                }
                siblings.insert(name);
                ManifestCategory node{name, {}, {}};
                if (object.contains("symbols") && !object.value("symbols").isArray()) problems << "Category symbols must be an array: " + name;
                for (const auto &entry : object.value("symbols").toArray())
                    if (addFile(entry)) node.symbols.append(entry.toString());
                if (object.contains("children") && !object.value("children").isArray()) problems << "Category children must be an array: " + name;
                parse(object.value("children").toArray(), node.children, depth + 1);
                nodes.append(node);
            }
        };
        parse(json.value("categories").toArray(), result.categories, 0);
    }
    if (json.contains("overrides") && !json.value("overrides").isArray()) problems << "overrides must be an array";
    QSet<QString> overrides;
    for (const auto &value : json.value("overrides").toArray()) {
        const auto id = value.toString();
        if (!value.isString() || overrides.contains(id) || !files.contains(id + ".json"))
            problems << "Invalid or duplicate manifest override: " + id;
        else { overrides.insert(id); result.overrides.append(id); }
    }
    if (!problems.isEmpty()) { if (errors) errors->append(problems); return std::nullopt; }
    return result;
}

QJsonObject SymbolManifest::toJson() const
{
    if (legacy) {
        // Category membership for flat files is unknown until definitions are
        // read. Never silently drop unresolved entries when serializing one.
        QJsonArray files;
        for (const auto &entry : legacyEntries) files.append(entry.file);
        QJsonObject json{{"symbols",files}};
        if (!categories.isEmpty()) json.insert("categories",QJsonArray::fromStringList(categoryPaths()));
        if (!overrides.isEmpty()) json.insert("overrides",QJsonArray::fromStringList(overrides));
        return json;
    }
    std::function<QJsonArray(const QVector<ManifestCategory> &)> encode;
    encode = [&](const QVector<ManifestCategory> &nodes) {
        QJsonArray array;
        for (const auto &node : nodes) {
            QJsonObject object{{"name", node.name}};
            if (!node.symbols.isEmpty()) object.insert("symbols", QJsonArray::fromStringList(node.symbols));
            if (!node.children.isEmpty()) object.insert("children", encode(node.children));
            array.append(object);
        }
        return array;
    };
    QJsonObject json{{"categories", encode(categories)}};
    if (!overrides.isEmpty()) json.insert("overrides", QJsonArray::fromStringList(overrides));
    return json;
}

QVector<ManifestSymbolEntry> SymbolManifest::entries() const
{
    if (legacy) return legacyEntries;
    QVector<ManifestSymbolEntry> result;
    std::function<void(const QVector<ManifestCategory> &, const QString &)> visit;
    visit = [&](const QVector<ManifestCategory> &nodes, const QString &parent) {
        for (const auto &node : nodes) {
            const auto path = parent.isEmpty() ? node.name : parent + '/' + node.name;
            for (const auto &file : node.symbols) result.append({file, path});
            visit(node.children, path);
        }
    };
    visit(categories, {}); return result;
}

QStringList SymbolManifest::categoryPaths() const
{
    QStringList result;
    std::function<void(const QVector<ManifestCategory> &, const QString &)> visit;
    visit = [&](const QVector<ManifestCategory> &nodes, const QString &parent) {
        for (const auto &node : nodes) {
            const auto path = parent.isEmpty() ? node.name : parent + '/' + node.name;
            result.append(path); visit(node.children, path);
        }
    };
    visit(categories, {}); return result;
}

void SymbolManifest::placeSymbol(const QString &file, const QString &category)
{
    auto *destination = ensureCategory(categories, category);
    if (destination->symbols.contains(file)) return;
    std::function<void(QVector<ManifestCategory> &)> remove;
    remove = [&](QVector<ManifestCategory> &nodes) {
        for (auto &node : nodes) { node.symbols.removeAll(file); remove(node.children); }
    };
    remove(categories);
    // Reacquire after traversing; category nodes themselves have not moved.
    ensureCategory(categories, category)->symbols.append(file);
}
