#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "document.h"

#include <QMainWindow>

class QLabel;
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

private:
    void createActions();
    void createMenus();
    void createToolBar();
    void createStatusBar();
    void updateWindowTitle();
    bool maybeSave();
    // Step 4 scaffolding only - see m_document. Populates m_document with a
    // few components (including a branch) covering every built-in symbol
    // kind and pushes it into m_scene via SchematicScene::setDocument() so
    // rendering can be visually checked. Removed once step 6 wires up real
    // file loading.
    void loadSampleDocument();

    SchematicScene *m_scene = nullptr;
    SchematicView  *m_view  = nullptr;

    QLabel *m_positionLabel = nullptr;
    QLabel *m_zoomLabel     = nullptr;

    QString m_currentFilePath;
    bool    m_documentModified = false;

    // Step 4 scaffolding only: a Document instance rendered read-only via
    // SchematicScene::setDocument() so the new rendering code can be
    // visually verified end-to-end before real file I/O exists. Step 6
    // (MainWindow file I/O) replaces this with an actually-loaded/edited
    // document; onNew()/onOpen() do not yet reset or populate it beyond this
    // temporary sample.
    Document m_document;
};

#endif // MAINWINDOW_H
