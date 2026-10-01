#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "document.h"
#include "schematicscene.h"
#include "identifiers.h"

#include <QMainWindow>

class QLabel;
class QComboBox;
class QDoubleSpinBox;
class QToolButton;
class QAction;
class QActionGroup;
class SchematicScene;
class SchematicView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onNew();
    void onOpen();
    bool onSave();
    bool onSaveAs();
    void onAbout();
    void onZoomFactorChanged(qreal factor);
    void onMouseScenePositionChanged(const QPointF &scenePos);
    // Marks the document modified and refreshes the title bar after any
    // edit SchematicScene commits (see SchematicScene::documentEdited()).
    void onDocumentEdited();
    // Surfaces SchematicScene::statusMessage() in the status bar.
    void onSceneStatusMessage(const QString &message);
    void onGridSpacingChanged(double spacing);
    void onSelectModeTriggered();
    void onDrawWireModeTriggered();
    // Enters PlaceComponent mode and sets kind as the symbol to place -
    // connected to each symbol button in the dock (see createSymbolDock()).
    void onPlaceComponentKindChanged(SymbolKind kind);
    // Opens the reference+value edit dialog for componentId, pre-filled
    // from its current labels. Called from SchematicScene's double-click
    // handler (see SchematicScene::componentDoubleClicked()).
    void onEditComponentLabels(ComponentId componentId);
    // Renames (reference only) the single currently selected component -
    // see m_renameAction. Disabled unless exactly one component is
    // selected - see updateRenameActionEnabled().
    void onRenameSelectedComponent();
    void updateRenameActionEnabled();

private:
    void createActions();
    void createMenus();
    void createToolBar();
    void createSymbolDock();
    void createStatusBar();
    void updateWindowTitle();
    bool maybeSave();
    // Writes m_document's current JSON bytes to path via QSaveFile, so a
    // write failure (disk full, permission denied, etc.) can never leave a
    // partially-written or corrupted file at path. Shows a QMessageBox on
    // failure. Does not touch
    // m_currentFilePath or m_documentModified; callers (onSave()/
    // onSaveAs()) are responsible for committing those only on success.
    bool writeDocumentTo(const QString &path);

    SchematicScene *m_scene = nullptr;
    SchematicView  *m_view  = nullptr;

    QLabel *m_positionLabel = nullptr;
    QLabel *m_zoomLabel     = nullptr;
    QDoubleSpinBox *m_gridSpacingSpin  = nullptr;
    QAction        *m_selectAction     = nullptr;
    QAction        *m_drawWireAction   = nullptr;
    // Rename (F2) is a QAction owned by MainWindow itself (not m_view) with
    // Qt::WindowShortcut context, so it fires regardless of whether focus
    // is in the canvas or the symbol dock - both are descendants of this
    // window, but the dock is a *sibling* of m_view, not a child of it, so
    // scoping the shortcut to m_view alone would miss dock-focused presses.
    QAction        *m_renameAction     = nullptr;
    // Exclusive group covering every tool-selection action: Select,
    // DrawWire, and one checkable QAction per built-in SymbolKind (added in
    // createSymbolDock()). Keeping all of these in one group means exactly
    // one tool - whichever was clicked most recently - is ever checked, and
    // clicking any symbol button both enters PlaceComponent mode and sets
    // that symbol as the kind to place, replacing the old separate
    // "Place" toggle + combo box.
    QActionGroup   *m_modeGroup        = nullptr;

    QString m_currentFilePath;
    bool    m_documentModified = false;

    // The single Document instance this window edits, starting out empty.
    // Bound to m_scene via SchematicScene::bindDocument() immediately in
    // the constructor (see below) and re-bound immediately after any later
    // wholesale replacement (onNew(), a successful onOpen()) - see
    // bindDocument()'s lifetime-requirement comment in schematicscene.h.
    Document m_document;
};

#endif // MAINWINDOW_H
