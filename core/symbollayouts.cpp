#include "symbollibrary.h"
#include <QJsonArray>
#include <QSet>
#include <QRegularExpression>
#include <algorithm>

const SymbolLayoutControl *symbolLayoutControl(const LibrarySymbol &symbol, const QString &key)
{
    for (const auto &control : symbol.layoutControls)
        if (control.key == key) return &control;
    return nullptr;
}

QStringList validateSymbolLayoutControls(const LibrarySymbol &symbol)
{
    QStringList errors;
    const QRegularExpression keyPattern(QStringLiteral("^[a-z][a-z0-9_]*$"));
    const auto clean = [](const QString &s) { return !s.isEmpty() && s == s.trimmed()
        && !s.contains('\n') && !s.contains('\r') && !s.contains(QChar(0)); };
    QSet<QString> controls;
    for (const auto &control : symbol.layoutControls) {
        if (!keyPattern.match(control.key).hasMatch() || controls.contains(control.key)
            || !clean(control.label) || control.choices.size() < 2)
            errors << QStringLiteral("Invalid or duplicate layout control");
        controls.insert(control.key);
        QSet<QString> choices;
        bool self = false;
        for (const auto &choice : control.choices) {
            if (!keyPattern.match(choice.key).hasMatch() || choices.contains(choice.key)
                || !clean(choice.label) || !clean(choice.symbolId.value()))
                errors << QStringLiteral("Invalid or duplicate layout choice");
            choices.insert(choice.key);
            QSet<QString> from, to;
            for (const auto &mapping : choice.terminals) {
                if (!clean(mapping.from) || !clean(mapping.to) || from.contains(mapping.from) || to.contains(mapping.to))
                    errors << QStringLiteral("Layout terminal mapping must be one-to-one");
                from.insert(mapping.from); to.insert(mapping.to);
            }
            QSet<QString> names;
            for (const auto &terminal : symbol.terminals) names.insert(terminal.name);
            if (from != names || choice.terminals.size() != symbol.terminals.size())
                errors << QStringLiteral("Layout mapping must cover every source terminal");
            if (choice.key == control.current) {
                self = choice.symbolId == symbol.id;
                for (const auto &mapping : choice.terminals)
                    self = self && mapping.from == mapping.to;
            } else if (choice.symbolId == symbol.id) {
                errors << QStringLiteral("Only the current layout choice may target this symbol");
            }
        }
        if (!self) errors << QStringLiteral("Current layout choice must map this symbol to itself");
    }
    return errors;
}

QVector<int> layoutTerminalMapping(const LibrarySymbol &source, const SymbolLayoutControl &control,
                                 const SymbolLayoutChoice &choice, const LibrarySymbol &target,
                                 QStringList *errors)
{
    const auto fail = [&](const QString &message) { if (errors) errors->append(message); return QVector<int>{}; };
    if (target.id != choice.symbolId || source.terminals.size() != target.terminals.size())
        return fail(QStringLiteral("Layout target must have the same terminal count"));
    const auto *targetControl = symbolLayoutControl(target, control.key);
    if (!targetControl || targetControl->current != choice.key)
        return fail(QStringLiteral("Layout target does not declare the expected choice"));
    QVector<int> indices(source.terminals.size(), -1);
    QSet<int> destinations;
    for (const auto &mapping : choice.terminals) {
        int from = -1, to = -1;
        for (int i = 0; i < source.terminals.size(); ++i) if (source.terminals[i].name == mapping.from) from = i;
        for (int i = 0; i < target.terminals.size(); ++i) if (target.terminals[i].name == mapping.to) to = i;
        if (from < 0 || to < 0 || indices[from] >= 0 || destinations.contains(to))
            return fail(QStringLiteral("Layout mapping has unknown or duplicate terminals"));
        indices[from] = to; destinations.insert(to);
    }
    if (indices.contains(-1)) return fail(QStringLiteral("Layout mapping is incomplete"));
    return indices;
}

void readSymbolLayoutControls(const QJsonObject &object, LibrarySymbol *symbol, QStringList *errors)
{
    if (!object.contains("layoutControls")) return;
    if (!object.value("layoutControls").isArray()) { errors->append("layoutControls must be an array"); return; }
    for (const auto &value : object.value("layoutControls").toArray()) {
        const auto entry = value.toObject();
        if (!value.isObject() || !entry.value("key").isString() || !entry.value("label").isString()
            || !entry.value("current").isString() || !entry.value("choices").isArray()) {
            errors->append("Malformed layout control"); continue;
        }
        SymbolLayoutControl control{entry.value("key").toString(), entry.value("label").toString(), entry.value("current").toString(), {}};
        for (const auto &choiceValue : entry.value("choices").toArray()) {
            const auto c = choiceValue.toObject();
            if (!choiceValue.isObject() || !c.value("key").isString() || !c.value("label").isString()
                || !c.value("symbol").isString() || !c.value("terminals").isArray()) {
                errors->append("Malformed layout choice"); continue;
            }
            SymbolLayoutChoice choice{c.value("key").toString(), c.value("label").toString(), SymbolId(c.value("symbol").toString()), {}};
            for (const auto &mappingValue : c.value("terminals").toArray()) {
                const auto m = mappingValue.toObject();
                if (!mappingValue.isObject() || !m.value("from").isString() || !m.value("to").isString())
                    errors->append("Malformed layout terminal mapping");
                else choice.terminals.append({m.value("from").toString(), m.value("to").toString()});
            }
            control.choices.append(choice);
        }
        symbol->layoutControls.append(control);
    }
}

QJsonArray symbolLayoutControlsJson(const LibrarySymbol &symbol)
{
    QJsonArray controls;
    for (const auto &control : symbol.layoutControls) {
        QJsonArray choices;
        for (const auto &choice : control.choices) {
            QJsonArray terminals;
            for (const auto &mapping : choice.terminals)
                terminals.append(QJsonObject{{"from", mapping.from}, {"to", mapping.to}});
            choices.append(QJsonObject{{"key", choice.key}, {"label", choice.label}, {"symbol", choice.symbolId.value()}, {"terminals", terminals}});
        }
        controls.append(QJsonObject{{"key", control.key}, {"label", control.label}, {"current", control.current}, {"choices", choices}});
    }
    return controls;
}

QStringList SymbolLibrary::layoutClosure(const QStringList &roots, QStringList *errors) const
{
    QStringList pending = roots, result;
    QSet<QString> visited;
    while (!pending.isEmpty()) {
        const QString id = pending.takeLast();
        if (visited.contains(id)) continue;
        visited.insert(id);
        const auto *source = find(SymbolId(id));
        if (!source) { if (errors) errors->append("Missing layout symbol: " + id); continue; }
        result.append(id);
        for (const auto &control : source->layoutControls) for (const auto &choice : control.choices) {
            const auto *target = find(choice.symbolId);
            if (!target) { if (errors) errors->append("Missing layout alternative: " + choice.symbolId.value()); continue; }
            layoutTerminalMapping(*source, control, choice, *target, errors);
            pending.append(choice.symbolId.value());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}
