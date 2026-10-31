#ifndef DOCUMENTEDITCONTROLLER_H
#define DOCUMENTEDITCONTROLLER_H

#include "document.h"

#include <QObject>
#include <QVector>
#include <QUndoStack>

#include <functional>

class DocumentEditController : public QObject
{
public:
    using Selection = QVector<ComponentId>;
    using EditOperation = std::function<bool()>;
    using SelectionGetter = std::function<Selection()>;
    using StateRestored = std::function<void(const Selection &)>;

    explicit DocumentEditController(Document *document, QObject *parent = nullptr);

    QUndoStack *undoStack() { return &m_undoStack; }
    bool commit(const QString &description, const EditOperation &operation,
                SelectionGetter afterSelectionGetter = {});
    void setSelectionCallbacks(SelectionGetter getter, StateRestored restored);
    void setClean() { m_undoStack.setClean(); }
    void resetHistory();

private:
    Document *m_document = nullptr;
    SelectionGetter m_selectionGetter;
    StateRestored m_stateRestored;
    QUndoStack m_undoStack;
};

#endif // DOCUMENTEDITCONTROLLER_H
