#include "mainwindow.h"
#include "schematicscene.h"
#include "schematicview.h"

#include "wireendpoint.h"

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <QCloseEvent>
#include <QAction>
#include <QKeySequence>
#include <QIcon>

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
    createStatusBar();

    connect(m_view, &SchematicView::zoomFactorChanged, this, &MainWindow::onZoomFactorChanged);
    connect(m_view, &SchematicView::mouseScenePositionChanged,
            this, &MainWindow::onMouseScenePositionChanged);

    resize(1000, 700);
    updateWindowTitle();

    loadSampleDocument();
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

    m_scene->clear();
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

    // File format not yet implemented; this establishes the UI flow so
    // document I/O can be dropped in without further UI changes.
    m_currentFilePath = path;
    m_documentModified = false;
    updateWindowTitle();
}

bool MainWindow::onSave()
{
    if (m_currentFilePath.isEmpty())
        return onSaveAs();

    // File format not yet implemented.
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

    m_currentFilePath = path;
    return onSave();
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

void MainWindow::loadSampleDocument()
{
    // Step 4 scaffolding only - see the declaration comment in mainwindow.h.
    // Lays out one of every built-in symbol kind, straight and cornered
    // wires, and a branch (so a junction dot is exercised), purely to
    // visually verify SchematicScene::setDocument() rendering. Interior
    // corner vertices below were hand-computed from each terminal's actual
    // rotated world position (see Component::terminalPosition()) so every
    // route segment stays axis-aligned, as validateRouteGeometry() requires.
    const ComponentId jack = m_document.addComponent(SymbolKind::Jack, {-100, 0});
    const ComponentId resistor = m_document.addComponent(SymbolKind::Resistor, {0, 0});
    const ComponentId capacitor = m_document.addComponent(SymbolKind::Capacitor, {100, 0},
                                                            Rotation::Deg90);
    const ComponentId diode = m_document.addComponent(SymbolKind::Diode, {200, 0});
    const ComponentId ground = m_document.addComponent(SymbolKind::Ground, {200, 80});
    const ComponentId opAmp = m_document.addComponent(SymbolKind::OpAmp, {0, 150});
    const ComponentId pot = m_document.addComponent(SymbolKind::Potentiometer, {150, 150},
                                                      Rotation::Deg180);

    // jack.sleeve (-80,0) -> resistor.1 (-20,0): already collinear.
    const auto lead = m_document.addWire(makeTerminalEndpoint(jack, 1), {},
                                          makeTerminalEndpoint(resistor, 0));
    // Branch partway along that lead, then route up to the op-amp's in+.
    if (lead) {
        if (const auto node = m_document.branchWireAt(*lead, QPointF(-50, 0)))
            m_document.addWire(makeNodeEndpoint(*node), {QPointF(-50, 142)},
                                makeTerminalEndpoint(opAmp, 0));
    }
    // resistor.2 (20,0) -> capacitor.1 (100,-20): one corner at (100,0).
    m_document.addWire(makeTerminalEndpoint(resistor, 1), {QPointF(100, 0)},
                        makeTerminalEndpoint(capacitor, 0));
    // capacitor.2 (100,20) -> diode.anode (180,0): one corner at (180,20).
    m_document.addWire(makeTerminalEndpoint(capacitor, 1), {QPointF(180, 20)},
                        makeTerminalEndpoint(diode, 0));
    // diode.cathode (220,0) -> ground.1 (200,60): one corner at (220,60).
    m_document.addWire(makeTerminalEndpoint(diode, 1), {QPointF(220, 60)},
                        makeTerminalEndpoint(ground, 0));
    // pot.wiper (150,170) -> op-amp.v+ (0,134): one corner at (150,134).
    m_document.addWire(makeTerminalEndpoint(pot, 2), {QPointF(150, 134)},
                        makeTerminalEndpoint(opAmp, 3));

    m_scene->setDocument(m_document);
}
