#include "mainwindow.h"
#include "schematicscene.h"
#include "schematicview.h"

#include "wireendpoint.h"
#include "documentloadresult.h"

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QDockWidget>
#include <QStatusBar>
#include <QLabel>
#include <QToolButton>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QFileDialog>
#include <QCloseEvent>
#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QIcon>
#include <QSaveFile>
#include <QFile>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QSignalBlocker>

namespace {
// Decimal places shown by the grid-spacing spin box. The document stores
// the exact spacing (any finite positive value the file format accepts); the
// control only displays it at this precision, and loading never writes the
// displayed value back into the document.
constexpr int kGridSpinDecimals = 4;
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_scene(new SchematicScene(this))
    , m_view(new SchematicView(this))
{
    m_view->setScene(m_scene);
    setCentralWidget(m_view);

    createActions();
    createMenus();
    createToolBar();
    createSymbolDock();
    createStatusBar();

    connect(m_view, &SchematicView::zoomFactorChanged, this, &MainWindow::onZoomFactorChanged);
    connect(m_view, &SchematicView::mouseScenePositionChanged,
            this, &MainWindow::onMouseScenePositionChanged);
    connect(m_scene, &SchematicScene::documentEdited, this, &MainWindow::onDocumentEdited);
    connect(m_scene, &SchematicScene::statusMessage, this, &MainWindow::onSceneStatusMessage);
    connect(m_scene, &SchematicScene::componentDoubleClicked,
            this, &MainWindow::onEditComponentLabels);
    connect(m_scene, &SchematicScene::selectionChanged,
            this, &MainWindow::updateRenameActionEnabled);

    resize(1000, 700);
    updateWindowTitle();

    // m_document starts out empty (default-constructed); bind it to the
    // scene explicitly here rather than relying on some other call to do so
    // indirectly - see the m_document declaration comment in mainwindow.h.
    m_scene->bindDocument(&m_document);
    m_view->setGridSpacing(m_document.gridSpacing());
    updateRenameActionEnabled();
}

MainWindow::~MainWindow() = default;

void MainWindow::createActions()
{
    // Actions are created inline in createMenus()/createToolBar(); this
    // function exists as a placeholder for when actions need to be shared
    // between more widgets (e.g. context menus) as the app grows.
}

void MainWindow::createMenus()
{
    auto *fileMenu = menuBar()->addMenu(tr("&File"));

    auto *newAction = fileMenu->addAction(tr("&New"), this, &MainWindow::onNew);
    newAction->setShortcut(QKeySequence::New);

    auto *openAction = fileMenu->addAction(tr("&Open..."), this, &MainWindow::onOpen);
    openAction->setShortcut(QKeySequence::Open);

    fileMenu->addSeparator();

    auto *saveAction = fileMenu->addAction(tr("&Save"), this, &MainWindow::onSave);
    saveAction->setShortcut(QKeySequence::Save);

    auto *saveAsAction = fileMenu->addAction(tr("Save &As..."), this, &MainWindow::onSaveAs);
    saveAsAction->setShortcut(QKeySequence::SaveAs);

    fileMenu->addSeparator();

    auto *exitAction = fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
    exitAction->setShortcut(QKeySequence::Quit);

    auto *editMenu = menuBar()->addMenu(tr("&Edit"));
    auto *undoAction = editMenu->addAction(tr("&Undo"));
    undoAction->setShortcut(QKeySequence::Undo);
    undoAction->setEnabled(false);
    auto *redoAction = editMenu->addAction(tr("&Redo"));
    redoAction->setShortcut(QKeySequence::Redo);
    redoAction->setEnabled(false);

    editMenu->addSeparator();
    m_renameAction = editMenu->addAction(tr("&Rename..."), this,
                                         &MainWindow::onRenameSelectedComponent);
    m_renameAction->setShortcut(Qt::Key_F2);
    // Qt::WindowShortcut (the default for an action added to a menu, but
    // set explicitly for clarity) fires as long as this window - or any
    // descendant widget, including both m_view and the symbol dock added
    // in createSymbolDock() - has focus. Scoping this to m_view alone would
    // miss F2 presses while focus is in the dock, since the dock is a
    // sibling of m_view under QMainWindow, not a child of it.
    m_renameAction->setShortcutContext(Qt::WindowShortcut);
    m_renameAction->setEnabled(false); // Enabled only while exactly one component is selected.
    addAction(m_renameAction);

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    auto *zoomInAction = viewMenu->addAction(tr("Zoom &In"), m_view, &SchematicView::zoomIn);
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    auto *zoomOutAction = viewMenu->addAction(tr("Zoom &Out"), m_view, &SchematicView::zoomOut);
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    viewMenu->addAction(tr("Zoom to &Fit"), m_view, &SchematicView::zoomToFit);
    auto *zoomResetAction = viewMenu->addAction(tr("&Reset Zoom"), m_view, &SchematicView::zoomReset);
    zoomResetAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About"), this, &MainWindow::onAbout);
    helpMenu->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
}

void MainWindow::createToolBar()
{
    auto *toolBar = addToolBar(tr("Main"));
    toolBar->setObjectName("mainToolBar");
    toolBar->addAction(tr("New"), this, &MainWindow::onNew);
    toolBar->addAction(tr("Open"), this, &MainWindow::onOpen);
    toolBar->addAction(tr("Save"), this, &MainWindow::onSave);
    toolBar->addSeparator();
    toolBar->addAction(tr("Zoom In"), m_view, &SchematicView::zoomIn);
    toolBar->addAction(tr("Zoom Out"), m_view, &SchematicView::zoomOut);
    toolBar->addAction(tr("Fit"), m_view, &SchematicView::zoomToFit);
    toolBar->addSeparator();

    // m_modeGroup also gathers the per-symbol placement actions added in
    // createSymbolDock(), so Select/DrawWire/each-symbol-button are all
    // mutually exclusive - see the m_modeGroup declaration comment in
    // mainwindow.h.
    m_modeGroup = new QActionGroup(this);
    m_modeGroup->setExclusive(true);

    m_selectAction = toolBar->addAction(tr("Select"));
    m_selectAction->setCheckable(true);
    m_selectAction->setChecked(true);
    m_modeGroup->addAction(m_selectAction);
    connect(m_selectAction, &QAction::triggered, this, &MainWindow::onSelectModeTriggered);

    m_drawWireAction = toolBar->addAction(tr("Draw Wire"));
    m_drawWireAction->setCheckable(true);
    m_modeGroup->addAction(m_drawWireAction);
    connect(m_drawWireAction, &QAction::triggered, this, &MainWindow::onDrawWireModeTriggered);

    toolBar->addSeparator();
    auto *gridLabel = new QLabel(tr("Grid spacing:"), this);
    toolBar->addWidget(gridLabel);
    m_gridSpacingSpin = new QDoubleSpinBox(this);
    m_gridSpacingSpin->setRange(0.1, 1000.0);
    m_gridSpacingSpin->setDecimals(kGridSpinDecimals);
    m_gridSpacingSpin->setValue(m_document.gridSpacing());
    m_gridSpacingSpin->setToolTip(
        tr("Sets the snap increment and minor grid line spacing; major grid "
           "lines are drawn every 10 minor intervals."));
    connect(m_gridSpacingSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onGridSpacingChanged);
    toolBar->addWidget(m_gridSpacingSpin);
}

void MainWindow::createSymbolDock()
{
    auto *dock = new QDockWidget(tr("Symbols"), this);
    dock->setObjectName("symbolDock");
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    auto *container = new QWidget(dock);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);

    for (SymbolKind kind : {SymbolKind::Resistor, SymbolKind::Capacitor, SymbolKind::Diode,
                            SymbolKind::Ground, SymbolKind::OpAmp, SymbolKind::Potentiometer,
                            SymbolKind::Jack}) {
        // One QAction per symbol, added to m_modeGroup so it is mutually
        // exclusive with Select/Draw Wire, bound to a QToolButton via
        // setDefaultAction() (the standard Qt pattern for a button whose
        // checked state and click behaviour are entirely driven by its
        // action - no manual signal syncing needed).
        auto *action = new QAction(symbolKindName(kind), this);
        action->setCheckable(true);
        m_modeGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, kind]() {
            onPlaceComponentKindChanged(kind);
        });

        auto *button = new QToolButton(container);
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setMinimumWidth(96);
        layout->addWidget(button);
    }
    layout->addStretch(1);

    dock->setWidget(container);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
}

void MainWindow::createStatusBar()
{
    m_positionLabel = new QLabel(tr("  "), this);
    m_zoomLabel = new QLabel(tr("Zoom: 100%"), this);

    statusBar()->addWidget(m_positionLabel, 1);
    statusBar()->addPermanentWidget(m_zoomLabel);
}

void MainWindow::updateWindowTitle()
{
    const QString name = m_currentFilePath.isEmpty() ? tr("Untitled") : m_currentFilePath;
    setWindowTitle(QString("%1%2 - Schematic").arg(m_documentModified ? "*" : "", name));
}

void MainWindow::syncGridControls()
{
    // The spin box rounds to its displayed precision and clamps to its
    // range, so a loaded spacing outside that (still valid in the file) must
    // not travel back through valueChanged() into the document. Block the
    // signal while updating; the model keeps the exact loaded value and the
    // view uses it directly.
    {
        const QSignalBlocker blocker(m_gridSpacingSpin);
        m_gridSpacingSpin->setValue(m_document.gridSpacing());
    }
    m_view->setGridSpacing(m_document.gridSpacing());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave())
        event->accept();
    else
        event->ignore();
}

bool MainWindow::maybeSave()
{
    if (!m_documentModified)
        return true;

    const auto result = QMessageBox::warning(this, tr("Schematic"),
        tr("The document has been modified.\nDo you want to save your changes?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    switch (result) {
    case QMessageBox::Save:
        return onSave();
    case QMessageBox::Cancel:
        return false;
    default:
        return true;
    }
}

void MainWindow::onNew()
{
    if (!maybeSave())
        return;

    m_document = Document();
    m_scene->bindDocument(&m_document);
    syncGridControls();
    m_currentFilePath.clear();
    m_documentModified = false;
    updateWindowTitle();
}

void MainWindow::onOpen()
{
    if (!maybeSave())
        return;

    const QString path = QFileDialog::getOpenFileName(this, tr("Open Schematic"), QString(),
                                                        tr("Schematic Files (*.schematic);;All Files (*)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Schematic"),
            tr("Could not open \"%1\" for reading: %2").arg(path, file.errorString()));
        return;
    }
    const DocumentLoadResult result = Document::fromJsonBytes(file.readAll());
    if (!result.document) {
        QMessageBox::warning(this, tr("Schematic"),
            tr("\"%1\" could not be loaded:\n%2").arg(path, result.errors.join('\n')));
        return; // The current document is left completely unchanged.
    }

    m_document = *result.document;
    m_scene->bindDocument(&m_document);
    syncGridControls();
    m_currentFilePath = path;
    m_documentModified = false;
    updateWindowTitle();

    if (!result.warnings.isEmpty())
        statusBar()->showMessage(result.warnings.join(QStringLiteral("; ")), 5000);
}

bool MainWindow::writeDocumentTo(const QString &path)
{
    // QSaveFile writes to a temporary file alongside path and only
    // atomically renames it into place on a successful commit(), so a
    // write failure can never leave a partially-written or corrupted file
    // at path.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Schematic"),
            tr("Could not open \"%1\" for writing: %2").arg(path, file.errorString()));
        return false;
    }
    const QByteArray bytes = m_document.toJsonBytes();
    const qint64 written = file.write(bytes);
    if (written != bytes.size()) {
        // A short or failed write must never be committed over the existing
        // file; discard the temporary file instead.
        const QString reason = file.errorString();
        file.cancelWriting();
        QMessageBox::warning(this, tr("Schematic"),
            tr("Could not save \"%1\": only %2 of %3 bytes could be written (%4)")
                .arg(path).arg(qMax<qint64>(written, 0)).arg(bytes.size()).arg(reason));
        return false;
    }
    if (!file.commit()) {
        QMessageBox::warning(this, tr("Schematic"),
            tr("Could not save \"%1\": %2").arg(path, file.errorString()));
        return false;
    }
    return true;
}

bool MainWindow::onSave()
{
    if (m_currentFilePath.isEmpty())
        return onSaveAs();

    if (!writeDocumentTo(m_currentFilePath))
        return false;
    m_documentModified = false;
    updateWindowTitle();
    return true;
}

bool MainWindow::onSaveAs()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Schematic As"), QString(),
                                                        tr("Schematic Files (*.schematic);;All Files (*)"));
    if (path.isEmpty())
        return false;

    // Only commit m_currentFilePath once the write actually succeeds - a
    // failed save must not change which file subsequent plain Save calls
    // target.
    if (!writeDocumentTo(path))
        return false;
    m_currentFilePath = path;
    m_documentModified = false;
    updateWindowTitle();
    return true;
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, tr("About Schematic"),
        tr("<h3>Schematic</h3>"
           "<p>A Qt-based schematic capture application focused on "
           "presentation rather than EDA.</p>"));
}

void MainWindow::onZoomFactorChanged(qreal factor)
{
    m_zoomLabel->setText(tr("Zoom: %1%").arg(qRound(factor * 100)));
}

void MainWindow::onMouseScenePositionChanged(const QPointF &scenePos)
{
    m_positionLabel->setText(tr("X: %1, Y: %2")
                                  .arg(scenePos.x(), 0, 'f', 1)
                                  .arg(scenePos.y(), 0, 'f', 1));
}

void MainWindow::onDocumentEdited()
{
    m_documentModified = true;
    updateWindowTitle();
}

void MainWindow::onSceneStatusMessage(const QString &message)
{
    statusBar()->showMessage(message, 3000);
}

void MainWindow::onGridSpacingChanged(double spacing)
{
    const qreal previousSpacing = m_document.gridSpacing();
    if (!m_document.setGridSpacing(spacing))
        return;
    m_view->setGridSpacing(m_document.gridSpacing());
    if (!qFuzzyCompare(previousSpacing, m_document.gridSpacing())) {
        m_documentModified = true;
        updateWindowTitle();
    }
}

void MainWindow::onSelectModeTriggered()
{
    m_scene->setEditMode(SchematicScene::EditMode::Select);
}

void MainWindow::onDrawWireModeTriggered()
{
    m_scene->setEditMode(SchematicScene::EditMode::DrawWire);
}

void MainWindow::onPlaceComponentKindChanged(SymbolKind kind)
{
    m_scene->setEditMode(SchematicScene::EditMode::PlaceComponent);
    m_scene->setPlaceComponentKind(kind);
}

void MainWindow::onEditComponentLabels(ComponentId componentId)
{
    const Component *component = m_document.component(componentId);
    if (!component)
        return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Edit Component"));
    auto *form = new QFormLayout;
    auto *referenceEdit = new QLineEdit(component->reference(), &dialog);
    auto *valueEdit = new QLineEdit(component->value(), &dialog);
    form->addRow(tr("Reference:"), referenceEdit);
    form->addRow(tr("Value:"), valueEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addLayout(form);
    layout->addWidget(buttons);

    while (dialog.exec() == QDialog::Accepted) {
        if (m_document.setComponentLabels(componentId, referenceEdit->text(), valueEdit->text())) {
            m_scene->refreshAfterExternalEdit(); // Marks modified via onDocumentEdited().
            return;
        }
        QMessageBox::warning(&dialog, tr("Schematic"),
            tr("The reference must be non-empty and unique among components."));
    }
}

void MainWindow::onRenameSelectedComponent()
{
    const std::optional<ComponentId> selected = m_scene->singleSelectedComponent();
    if (!selected)
        return;
    const Component *component = m_document.component(*selected);
    if (!component)
        return;

    bool ok = false;
    const QString newReference = QInputDialog::getText(this, tr("Rename Component"),
        tr("Reference:"), QLineEdit::Normal, component->reference(), &ok);
    if (!ok)
        return;

    if (!m_document.setComponentLabels(*selected, newReference, component->value())) {
        QMessageBox::warning(this, tr("Schematic"),
            tr("The reference must be non-empty and unique among components."));
        return;
    }
    m_scene->refreshAfterExternalEdit();
}

void MainWindow::updateRenameActionEnabled()
{
    m_renameAction->setEnabled(m_scene->singleSelectedComponent().has_value());
}
