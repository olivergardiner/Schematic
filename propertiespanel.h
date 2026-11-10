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
class QFormLayout;
class QPlainTextEdit;
class QDoubleSpinBox;

class PropertiesPanel : public QWidget
{
    Q_OBJECT
public:
    PropertiesPanel(Document *document, SchematicScene *scene, DocumentEditController *edits,
                    QWidget *parent = nullptr);
    ~PropertiesPanel() override;
    void invalidate();
    void focusProperty(const QString &id);
    void focusNote();
private:
    void refresh();
    bool refreshNote();
    bool apply(const QVector<ComponentId> &targets, const QVector<PropertyEdit> &edits);
    void addProperty();
    void renameProperty();
    void updateButtons();
    bool finishTextEdit();
    void refreshLayouts(const QVector<ComponentId> &targets);
    void refreshJustification(const QVector<ComponentId> &targets);
    void changeNoteAppearance(NoteId id, std::optional<PropertyJustification> justification,
                              int colourTarget = 0, bool clear = false);
    Document *m_document;
    SchematicScene *m_scene;
    DocumentEditController *m_edits;
    QTableWidget *m_table;
    QLabel *m_summary;
    QFormLayout *m_layoutControls;
    QFormLayout *m_appearanceControls;
    QLabel *m_error;
    QPushButton *m_resetPosition;
    QPushButton *m_add;
    QPushButton *m_remove;
    QPushButton *m_rename;
    quint64 m_epoch = 0;
    bool m_refreshPending = false;
    bool m_refreshing = false;
    QString m_focusProperty;
    QWidget *m_noteControls;
    QPlainTextEdit *m_noteText;
    QDoubleSpinBox *m_noteWidth;
    QFormLayout *m_noteAppearance;
    bool m_focusNote = false;
    bool m_noteTextDirty = false;
    bool m_noteWidthDirty = false;
    std::function<bool()> m_finishEdit;
};
#endif
