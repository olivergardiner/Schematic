#ifndef PROPERTIESPANEL_H
#define PROPERTIESPANEL_H
#include "document.h"
#include <QWidget>
#include <functional>

class SchematicScene;
class DocumentEditController;
class QTableWidget;
class QLabel;
class QPushButton;

class PropertiesPanel : public QWidget
{
    Q_OBJECT
public:
    PropertiesPanel(Document *document, SchematicScene *scene, DocumentEditController *edits,
                    QWidget *parent = nullptr);
    void invalidate();
    void focusProperty(const QString &id);
private:
    void refresh();
    bool apply(const QVector<ComponentId> &targets, const QVector<PropertyEdit> &edits);
    void addProperty();
    void renameProperty();
    void updateButtons();
    bool finishTextEdit();
    Document *m_document;
    SchematicScene *m_scene;
    DocumentEditController *m_edits;
    QTableWidget *m_table;
    QLabel *m_summary;
    QLabel *m_error;
    QPushButton *m_resetPosition;
    QPushButton *m_add;
    QPushButton *m_remove;
    QPushButton *m_rename;
    quint64 m_epoch = 0;
    bool m_refreshPending = false;
    bool m_refreshing = false;
    QString m_focusProperty;
    std::function<bool()> m_finishEdit;
};
#endif
