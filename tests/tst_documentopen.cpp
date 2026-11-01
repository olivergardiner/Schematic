#include "../mainwindow.h"
#include "documentloadresult.h"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

class DocumentOpenTests : public QObject
{
    Q_OBJECT
private slots:
    void reviewChoicesAndAtomicOpen();
    void missingAndIncompatibleChoices();
};

void DocumentOpenTests::reviewChoicesAndAtomicOpen()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    struct Restore { SymbolLibrary saved; ~Restore() { applicationSymbolLibrary() = saved; } } restore{applicationSymbolLibrary()};
    QStandardPaths::setTestModeEnabled(true);
    MainWindow window;
    window.m_symbolPackageStore = SymbolPackageStore(temporary.filePath("library"));
    QVERIFY(window.m_editController->commit("Place", [&] {
        return window.m_document.addComponent(SymbolIds::Resistor, {}) != kInvalidComponentId;
    }));
    window.m_currentFilePath = "unchanged.schematic";
    const auto before = window.m_document.toJsonBytes();
    const int history = window.m_editController->undoStack()->count();

    Document candidate;
    candidate.addComponent(SymbolIds::Resistor, {100, 0});
    auto embedded = *candidate.symbol(SymbolIds::Resistor);
    embedded.displayName = "Embedded artwork";
    QStringList errors;
    QVERIFY(candidate.useSymbol(embedded, &errors));
    const QString path = temporary.filePath("candidate.schematic");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(candidate.toJsonBytes());
    file.close();

    QTimer::singleShot(0, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        const auto choices = dialog->findChildren<QComboBox *>();
        QCOMPARE(choices.size(), 1);
        QCOMPARE(choices[0]->count(), 3);
        dialog->reject();
    });
    QVERIFY(!window.openDocumentFrom(path));
    QCOMPARE(window.m_document.toJsonBytes(), before);
    QCOMPARE(window.m_currentFilePath, QString("unchanged.schematic"));
    QCOMPARE(window.m_editController->undoStack()->count(), history);
    QVERIFY(!window.m_editController->undoStack()->isClean());

    // A failed batch must leave dirty history untouched without a save prompt.
    QFile blocked(temporary.filePath("blocked"));
    QVERIFY(blocked.open(QIODevice::WriteOnly));
    blocked.write("keep");
    blocked.close();
    window.m_symbolPackageStore = SymbolPackageStore(blocked.fileName());
    QTimer::singleShot(0, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        dialog->findChildren<QComboBox *>()[0]->setCurrentIndex(1);
        dialog->accept();
        QTimer::singleShot(0, [] {
            auto *warning = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            QVERIFY(warning);
            warning->accept();
        });
    });
    QVERIFY(!window.openDocumentFrom(path));
    QCOMPARE(window.m_document.toJsonBytes(), before);
    QCOMPARE(window.m_currentFilePath, QString("unchanged.schematic"));
    QCOMPARE(window.m_editController->undoStack()->count(), history);
    QCOMPARE(applicationSymbolLibrary().find(embedded.id)->displayName,
             restore.saved.find(embedded.id)->displayName);

    QVERIFY(!window.m_editController->undoStack()->isClean());
    window.m_editController->setClean();
    // Selecting the active version changes only the candidate and commits open.
    QTimer::singleShot(0, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        dialog->findChildren<QComboBox *>()[0]->setCurrentIndex(2);
        dialog->accept();
    });
    QVERIFY(window.openDocumentFrom(path));
    QCOMPARE(window.m_currentFilePath, path);
    QCOMPARE(window.m_editController->undoStack()->count(), 0);
    QCOMPARE(window.m_document.symbol(embedded.id)->displayName,
             applicationSymbolLibrary().find(embedded.id)->displayName);

    // Default embedded choice remains usable even when persistence is unavailable.
    QTimer::singleShot(0, [] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        dialog->accept();
    });
    QVERIFY(window.openDocumentFrom(path));
    QCOMPARE(window.m_document.symbol(embedded.id)->displayName, embedded.displayName);
    const QString savedPath = temporary.filePath("saved.schematic");
    QVERIFY(window.writeDocumentTo(savedPath));
    QFile saved(savedPath);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    auto reopened = Document::fromJsonBytes(saved.readAll());
    QVERIFY(reopened.document);
    QCOMPARE(reopened.document->toJsonBytes(), window.m_document.toJsonBytes());
}

void DocumentOpenTests::missingAndIncompatibleChoices()
{
    QTemporaryDir temporary;
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    struct Restore { SymbolLibrary saved; ~Restore() { applicationSymbolLibrary() = saved; } } restore{applicationSymbolLibrary()};
    MainWindow window;
    window.m_symbolPackageStore = SymbolPackageStore(temporary.filePath("library"));
    Document candidate;
    candidate.addComponent(SymbolIds::Resistor, {});
    auto custom = *candidate.symbol(SymbolIds::Resistor);
    custom.id = SymbolId(QStringLiteral("NewDocumentSymbol"));
    QStringList errors;
    QVERIFY(candidate.useSymbol(custom, &errors));
    candidate.addComponent(custom.id, {100, 0});
    auto root = candidate.toJson();
    auto definitions = root.value("symbols").toArray();
    auto resistor = definitions[1].toObject();
    auto terminals = resistor.value("terminals").toArray();
    auto terminal = terminals[0].toObject();
    terminal.insert("x", -25);
    terminals[0] = terminal;
    resistor.insert("terminals", terminals);
    definitions[1] = resistor;
    root.insert("symbols", definitions);
    const QString path = temporary.filePath("missing.schematic");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(root).toJson());
    file.close();
    QTimer::singleShot(0, [] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        QVERIFY(dialog);
        const auto choices = dialog->findChildren<QComboBox *>();
        QCOMPARE(choices.size(), 2);
        QCOMPARE(choices[0]->count(), 2); // missing: embedded or add
        QCOMPARE(choices[1]->count(), 1); // terminals differ: embedded only
        choices[0]->setCurrentIndex(1);
        dialog->accept();
    });
    QVERIFY(window.openDocumentFrom(path));
    QVERIFY(applicationSymbolLibrary().find(custom.id));
    QVERIFY(window.m_symbolActions.contains(custom.id));
    QCOMPARE(window.m_document.terminalPosition(*window.m_document.component(1), 0), QPointF(-25, 0));
    SymbolLibrary restarted = builtinSymbolLibrary();
    errors = window.m_symbolPackageStore.loadInto(&restarted);
    QVERIFY(errors.isEmpty());
    QVERIFY(restarted.find(custom.id));
}

QTEST_MAIN(DocumentOpenTests)
#include "tst_documentopen.moc"
