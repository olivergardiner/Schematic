#ifndef MAINWINDOW_H
#define MAINWINDOW_H

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

    SchematicScene *m_scene = nullptr;
    SchematicView  *m_view  = nullptr;

    QLabel *m_positionLabel = nullptr;
    QLabel *m_zoomLabel     = nullptr;

    QString m_currentFilePath;
    bool    m_documentModified = false;
};

#endif // MAINWINDOW_H
